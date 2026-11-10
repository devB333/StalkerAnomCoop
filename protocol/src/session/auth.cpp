#include "coop/session/auth.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace coop::session
{
proto::Digest transcript(const proto::Hello& h, const proto::AuthChallenge& c)
{
    std::vector<std::uint8_t> t;
    proto::Writer w(t);
    static const char kLabel[] = "COOP-AUTH-v1";
    w.raw(kLabel, sizeof(kLabel)); // includes the terminating zero
    w.uuid(c.campaign_id);
    w.digest(c.server_nonce);
    w.digest(h.client_nonce);
    w.uuid(h.player_id);
    w.digest(h.build_id);
    w.digest(h.content_digest);
    w.u16(h.major);
    w.u16(h.minor);
    return sha256(t.data(), t.size());
}

bool make_proof(const proto::Hello& h, const proto::AuthChallenge& c, const PrivateKey& key, proto::AuthProof& proof)
{
    Signature sig;
    if (!sign(key, transcript(h, c), sig))
        return false;
    proof.signature = sig;
    return true;
}

bool check_proof(const proto::Hello& h, const proto::AuthChallenge& c, const PublicKey& pub, const proto::AuthProof& proof)
{
    return verify(pub, transcript(h, c), proof.signature);
}

static const char kJoinAlphabet[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ"; // 31 symbols, none ambiguous

std::string make_join_code()
{
    std::string code;
    while (code.size() < 10)
    {
        std::uint8_t b = 0;
        if (!random_bytes(&b, 1))
            return std::string();
        if (b < 248) // 8 * 31: no modulo bias
            code.push_back(kJoinAlphabet[b % 31]);
    }
    return code.substr(0, 5) + "-" + code.substr(5);
}

bool normalize_join_code(const std::string& in, std::string& out)
{
    out.clear();
    for (char ch : in)
    {
        if (ch == '-' || ch == ' ' || ch == '\t')
            continue;
        if (ch >= 'a' && ch <= 'z')
            ch = char(ch - 'a' + 'A');
        if (!std::strchr(kJoinAlphabet, ch) || ch == 0)
            return false;
        out.push_back(ch);
    }
    return out.size() == 10;
}

proto::Digest join_mac(const std::string& code, const proto::Digest& t, const PublicKey& pub)
{
    std::string norm;
    if (!normalize_join_code(code, norm))
        norm.clear();
    std::vector<std::uint8_t> b;
    proto::Writer w(b);
    static const char kLabel[] = "COOP-JOIN-v1";
    w.raw(kLabel, sizeof(kLabel));
    w.raw(norm.data(), norm.size());
    w.digest(t);
    w.raw(pub.data(), pub.size());
    return sha256(b.data(), b.size());
}

bool make_join_proof(const proto::Hello& h, const proto::AuthChallenge& c, const PrivateKey& key,
    const std::string& code, proto::AuthProof& proof)
{
    std::string norm;
    PublicKey pub;
    if (!normalize_join_code(code, norm) || !public_of(key, pub) || !make_proof(h, c, key, proof))
        return false;
    proof.join = true;
    proof.public_key = pub;
    proof.join_mac = join_mac(norm, transcript(h, c), pub);
    return true;
}

bool check_join_proof(const proto::Hello& h, const proto::AuthChallenge& c, const std::string& code,
    const proto::AuthProof& proof)
{
    std::string norm;
    if (!proof.join || !normalize_join_code(code, norm) || !check_proof(h, c, proof.public_key, proof))
        return false;
    const proto::Digest want = join_mac(norm, transcript(h, c), proof.public_key);
    std::uint8_t diff = 0; // constant time
    for (std::size_t i = 0; i < want.size(); ++i)
        diff |= std::uint8_t(want[i] ^ proof.join_mac[i]);
    return diff == 0;
}

bool Roster::add(const RosterEntry& e, std::string* error)
{
    auto fail = [&](const char* why) {
        if (error)
            *error = why;
        return false;
    };
    if (e.player_id.zero())
        return fail("zero player id");
    if (find(e.player_id))
        return fail("player listed twice");
    if (entries_.size() >= kMaxPlayers)
        return fail("roster full");
    if (e.display_name.empty() || e.display_name.size() > proto::kMaxDisplayName)
        return fail("display name must be 1-32 characters");
    for (char ch : e.display_name)
        if (ch < 0x20 || ch >= 0x7F)
            return fail("display name must be printable ASCII");
    for (const RosterEntry& o : entries_)
        if (o.key == e.key)
            return fail("two players share a key");
    entries_.push_back(e);
    return true;
}

bool Roster::parse(const std::string& text, std::string* error)
{
    std::istringstream in(text);
    std::string lineText;
    int n = 0;
    while (std::getline(in, lineText))
    {
        ++n;
        if (!lineText.empty() && lineText.back() == '\r')
            lineText.pop_back();
        if (lineText.empty() || lineText[0] == '#')
            continue;
        std::istringstream ls(lineText);
        std::string word, id, key;
        ls >> word >> id >> key;
        std::string name;
        std::getline(ls, name);
        while (!name.empty() && name.front() == ' ')
            name.erase(name.begin());
        RosterEntry e;
        std::string why;
        if (word != "player")
            why = "expected 'player'";
        else if (!uuid_from_hex(id, e.player_id))
            why = "bad player id";
        else if (!from_hex(key, e.key.data(), e.key.size()))
            why = "bad public key";
        else
        {
            e.display_name = name;
            if (!add(e, &why))
                why = why.empty() ? "rejected" : why;
            else
                continue;
        }
        if (error)
            *error = "line " + std::to_string(n) + ": " + why;
        return false;
    }
    return true;
}

bool Roster::load(const std::string& path, std::string* error)
{
    std::ifstream f(path);
    if (!f)
    {
        if (error)
            *error = "cannot open roster";
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parse(ss.str(), error);
}

const RosterEntry* Roster::find(const proto::Uuid128& player) const
{
    for (const RosterEntry& e : entries_)
        if (e.player_id == player)
            return &e;
    return nullptr;
}

std::string Roster::line(const RosterEntry& e)
{
    return "player " + uuid_hex(e.player_id) + " " + to_hex(e.key.data(), e.key.size()) + " " + e.display_name;
}
} // namespace coop::session
