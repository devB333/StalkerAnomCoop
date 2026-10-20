// Shared helpers for session tests: a roster of players with keys, server
// and client wrappers, and a raw client that speaks the codec directly.
#pragma once

#include "coop/session/client.h"
#include "coop/session/server.h"
#include "rig.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <optional>

namespace coop::test
{
using namespace coop::session;
using proto::Uuid128;

struct Cast
{
    Roster roster;
    std::vector<Uuid128> ids;
    std::vector<PublicKey> pubs;
    std::vector<std::shared_ptr<PrivateKey>> keys;
};

inline Cast MakeCast(int n)
{
    Cast c;
    for (int i = 0; i < n; ++i)
    {
        auto k = std::make_shared<PrivateKey>();
        RosterEntry e;
        REQUIRE(generate_key(*k, e.key));
        e.player_id = random_uuid();
        e.display_name = "Player " + std::to_string(i + 1);
        REQUIRE(c.roster.add(e));
        c.ids.push_back(e.player_id);
        c.pubs.push_back(e.key);
        c.keys.push_back(k);
    }
    return c;
}

inline proto::LocalBuild Build(std::uint8_t content = 2)
{
    proto::LocalBuild b;
    b.build_id.fill(1);
    b.content_digest.fill(content);
    return b;
}

inline ServerConfig SConfig()
{
    ServerConfig c;
    c.build = Build();
    c.campaign_id = {10, 10};
    c.party_id = {11, 11};
    c.authority_epoch = 5;
    return c;
}

inline PrivateKey Copy(const PrivateKey& k)
{
    // Keys cannot be copied; round-trip through a key file for a second holder.
    const std::string path = "copy-" + std::to_string(reinterpret_cast<std::uintptr_t>(&k)) + ".key";
    Uuid128 who = {1, 1};
    PrivateKey out;
    PublicKey pub;
    REQUIRE(save_key_file(path, who, k, nullptr) && load_key_file(path, who, out, pub, nullptr));
    std::remove(path.c_str());
    return out;
}

struct Srv
{
    std::unique_ptr<net::Transport> t;
    std::unique_ptr<Server> s;
    std::vector<SessionEvent> ev;
    std::string address;
    // max_connections: the transport's own cap on open connections, which
    // refuses (1001) before the session layer sees a peer.
    Srv(Rig& rig, const Cast& cast, const ServerConfig& cfg = SConfig(), std::uint32_t max_connections = 4)
    {
        net::TransportConfig tc = transport_config(cfg.liveness);
        tc.max_connections = max_connections;
        t = rig.make(tc);
        address = rig.address();
        REQUIRE(t->listen(address.c_str()));
        s = std::make_unique<Server>(*t, cfg, cast.roster);
    }
    void update() { s->update(ev); }
    int count(SessionEventType type) const
    {
        return int(std::count_if(ev.begin(), ev.end(), [&](const SessionEvent& e) { return e.type == type; }));
    }
};

struct Cli
{
    std::unique_ptr<net::Transport> t;
    std::unique_ptr<Client> c;
    std::vector<ClientEvent> ev;
    Cli(Rig& rig, const Uuid128& player, PrivateKey key, const proto::LocalBuild& build = Build(),
        const LivenessConfig& liveness = {}, std::optional<proto::ResumeRequest> resume = std::nullopt)
    {
        t = rig.make(transport_config(liveness));
        ClientConfig cfg;
        cfg.build = build;
        cfg.player_id = player;
        cfg.display_name = "p";
        cfg.expected_campaign = {10, 10};
        cfg.liveness = liveness;
        cfg.resume = std::move(resume);
        c = std::make_unique<Client>(*t, cfg, std::move(key));
    }
    int count(ClientEventType type) const
    {
        return int(std::count_if(ev.begin(), ev.end(), [&](const ClientEvent& e) { return e.type == type; }));
    }
    void update() { c->update(ev); }
    const ClientEvent* find(ClientEventType type) const
    {
        for (const ClientEvent& e : ev)
            if (e.type == type)
                return &e;
        return nullptr;
    }
};

// A client that speaks the codec directly, for attacks and broken peers.
struct Raw
{
    Node node;
    proto::SequenceCounter seq;
    Raw(Rig& rig) : node{rig.make(proto::transport_config())} {}
    net::ConnectionId conn() const
    {
        const net::Event* e = node.Find(net::EventType::Connected);
        return e ? e->connection : net::kNoConnection;
    }
    bool send(const proto::Message& m, std::uint64_t epoch = 0)
    {
        std::vector<std::uint8_t> b;
        const proto::MessageType t = proto::type_of(m);
        if (!proto::encode(m, epoch, seq.next(proto::channel_of(t)), b))
            return false;
        return node.t->send(conn(), proto::channel_of(t), b.data(), std::uint32_t(b.size())) == net::SendResult::Sent;
    }
    template <class T>
    std::optional<T> first() const
    {
        for (const net::Event& e : node.events)
            if (e.type == net::EventType::Message)
            {
                proto::Decoded d;
                if (proto::decode(e.channel, proto::Role::Client, e.payload.data(), e.payload.size(), d).ok())
                    if (auto* p = std::get_if<T>(&d.body))
                        return *p;
            }
        return std::nullopt;
    }
    const net::Event* ended() const { return node.Find(net::EventType::Disconnected); }
};

inline bool Run(Rig& rig, const std::function<bool()>& step, std::int64_t ms) { return Pump(rig, {}, step, ms); }

inline proto::Hello HelloFor(const Uuid128& player, const proto::LocalBuild& b = Build())
{
    proto::Hello h;
    h.build_id = b.build_id;
    h.content_digest = b.content_digest;
    h.player_id = player;
    h.display_name = "raw";
    random_bytes(h.client_nonce.data(), 32);
    return h;
}

inline const SessionEvent* Joined(const Srv& s, const Uuid128& player)
{
    for (const SessionEvent& e : s.ev)
        if (e.type == SessionEventType::Joined && e.player == player)
            return &e;
    return nullptr;
}
} // namespace coop::test
