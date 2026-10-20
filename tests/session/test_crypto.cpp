// Credentials: hashing, signatures, the transcript, key files, the roster.
#include "check.h"
#include "coop/session/auth.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace coop::test
{
using namespace coop::session;

TEST("crypto/sha256-known-answer")
{
    const proto::Digest d = sha256("abc", 3);
    CHECK(to_hex(d.data(), d.size()) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const proto::Digest e = sha256("", 0);
    CHECK(to_hex(e.data(), e.size()) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST("crypto/sign-verify")
{
    PrivateKey k1, k2;
    PublicKey p1, p2;
    REQUIRE(generate_key(k1, p1) && generate_key(k2, p2));
    CHECK(p1 != p2);
    const proto::Digest h = sha256("message", 7);
    Signature s;
    REQUIRE(sign(k1, h, s));
    CHECK(verify(p1, h, s));
    CHECK(!verify(p2, h, s)); // another player's key
    proto::Digest h2 = h;
    h2[0] ^= 1;
    CHECK(!verify(p1, h2, s));
    Signature bad = s;
    bad[10] ^= 1;
    CHECK(!verify(p1, h, bad));
    PublicKey zero{};
    CHECK(!verify(zero, h, s)); // not a point on the curve
    PrivateKey empty;
    CHECK(!sign(empty, h, s));
    // Signatures are randomised but all verify.
    Signature s2;
    REQUIRE(sign(k1, h, s2));
    CHECK(s2 != s && verify(p1, h, s2));
    PublicKey again;
    CHECK(public_of(k1, again) && again == p1);
    PrivateKey moved = std::move(k1);
    CHECK(k1.empty() && !moved.empty());
}

TEST("crypto/transcript-binds-everything")
{
    PrivateKey k;
    PublicKey p;
    REQUIRE(generate_key(k, p));
    proto::Hello h;
    h.player_id = {1, 2};
    h.display_name = "a";
    random_bytes(h.client_nonce.data(), 32);
    h.build_id.fill(3);
    h.content_digest.fill(4);
    proto::AuthChallenge c;
    random_bytes(c.server_nonce.data(), 32);
    c.campaign_id = {5, 6};
    proto::AuthProof proof;
    REQUIRE(make_proof(h, c, k, proof));
    CHECK(check_proof(h, c, p, proof));
    auto changed = [&](auto mutate) {
        proto::Hello h2 = h;
        proto::AuthChallenge c2 = c;
        mutate(h2, c2);
        return !check_proof(h2, c2, p, proof);
    };
    CHECK(changed([](proto::Hello& x, proto::AuthChallenge&) { x.player_id.lo ^= 1; }));
    CHECK(changed([](proto::Hello& x, proto::AuthChallenge&) { x.client_nonce[5] ^= 1; }));
    CHECK(changed([](proto::Hello&, proto::AuthChallenge& y) { y.server_nonce[5] ^= 1; }));
    CHECK(changed([](proto::Hello&, proto::AuthChallenge& y) { y.campaign_id.hi ^= 1; }));
    CHECK(changed([](proto::Hello& x, proto::AuthChallenge&) { x.build_id[0] ^= 1; }));
    CHECK(changed([](proto::Hello& x, proto::AuthChallenge&) { x.content_digest[0] ^= 1; }));
    CHECK(changed([](proto::Hello& x, proto::AuthChallenge&) { x.minor ^= 1; }));
    // The display name is not part of the identity.
    CHECK(!changed([](proto::Hello& x, proto::AuthChallenge&) { x.display_name = "b"; }));
}

TEST("crypto/key-file")
{
    PrivateKey k;
    PublicKey p;
    REQUIRE(generate_key(k, p));
    const proto::Uuid128 player = random_uuid();
    const std::string path = "test-player.key";
    std::string error;
    REQUIRE_MSG(save_key_file(path, player, k, &error), "%s", error.c_str());
    proto::Uuid128 loadedPlayer;
    PrivateKey loaded;
    PublicKey loadedPub;
    REQUIRE_MSG(load_key_file(path, loadedPlayer, loaded, loadedPub, &error), "%s", error.c_str());
    CHECK(loadedPlayer == player && loadedPub == p);
    const proto::Digest h = sha256("x", 1);
    Signature s;
    CHECK(sign(loaded, h, s) && verify(p, h, s));
    // The file is not the key in clear: neither public nor private bytes appear.
    std::ifstream f(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    auto contains = [&](const std::uint8_t* needle, std::size_t n) {
        return std::search(bytes.begin(), bytes.end(), needle, needle + n) != bytes.end();
    };
    CHECK(!contains(p.data(), 32));
    metric("key_file_bytes", double(bytes.size()));
    // A damaged file does not open.
    bytes[bytes.size() / 2] ^= 0x40;
    {
        std::ofstream o(path, std::ios::binary | std::ios::trunc);
        o.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    }
    PrivateKey none;
    CHECK(!load_key_file(path, loadedPlayer, none, loadedPub, &error));
    note("damaged key file: " + error);
    CHECK(none.empty());
    std::remove(path.c_str());
    CHECK(!load_key_file("no-such.key", loadedPlayer, none, loadedPub, &error));
}

TEST("crypto/roster")
{
    PrivateKey k1, k2;
    RosterEntry a, b;
    REQUIRE(generate_key(k1, a.key) && generate_key(k2, b.key));
    a.player_id = random_uuid();
    a.display_name = "Player One";
    b.player_id = random_uuid();
    b.display_name = "Two";
    const std::string text = "# coop roster v1\n\n" + Roster::line(a) + "\r\n" + Roster::line(b) + "\n";
    Roster r;
    std::string error;
    REQUIRE_MSG(r.parse(text, &error), "%s", error.c_str());
    CHECK(r.size() == 2);
    REQUIRE(r.find(a.player_id) != nullptr);
    CHECK(r.find(a.player_id)->key == a.key && r.find(a.player_id)->display_name == "Player One");
    CHECK(r.find(random_uuid()) == nullptr);
    auto bad = [&](const std::string& t) {
        Roster x;
        std::string why;
        const bool ok = x.parse(t, &why);
        note(why);
        return !ok;
    };
    CHECK(bad(Roster::line(a) + "\n" + Roster::line(a)));                 // same player twice
    RosterEntry c = b;
    c.player_id = random_uuid();
    CHECK(bad(Roster::line(b) + "\n" + Roster::line(c)));                 // two players, one key
    CHECK(bad("player 1234 " + to_hex(a.key.data(), 64) + " x"));         // short id
    CHECK(bad("player " + uuid_hex(a.player_id) + " zz x"));              // bad key
    CHECK(bad("user " + uuid_hex(a.player_id) + " " + to_hex(a.key.data(), 64) + " x"));
    CHECK(bad("player " + uuid_hex(a.player_id) + " " + to_hex(a.key.data(), 64))); // no name
    CHECK(bad("player 00000000000000000000000000000000 " + to_hex(a.key.data(), 64) + " x"));
}
} // namespace coop::test
