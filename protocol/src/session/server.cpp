#include "coop/session/server.h"

#include <algorithm>

namespace coop::session
{
Server::Server(net::Transport& transport, const ServerConfig& config, Roster roster)
    : t_(transport), config_(config), roster_(std::move(roster))
{
    slots_.resize(std::min<std::uint32_t>(config_.max_players, proto::kMaxSlots));
}

std::string Server::AddressOf(const std::string& remote)
{
    const std::size_t colon = remote.rfind(':');
    return colon == std::string::npos ? remote : remote.substr(0, colon);
}

void Server::note(const std::string& line)
{
    if (log_.size() < 10000)
        log_.push_back(line);
}

void Server::update(std::vector<SessionEvent>& out)
{
    events_.clear();
    t_.poll(events_);
    for (const net::Event& e : events_)
    {
        switch (e.type)
        {
        case net::EventType::Connected:
            if (e.incoming)
                onConnected(e.connection);
            break;
        case net::EventType::Disconnected:
            onDisconnected(e.connection, std::string(net::to_string(e.reason)) + " (" + std::to_string(e.code) + ")", out);
            break;
        case net::EventType::Message:
        {
            auto it = conns_.find(e.connection);
            if (it != conns_.end())
                onMessage(it->second, e, out);
            break;
        }
        }
    }
    // Handshakes that do not finish in time lose their connection, so an idle
    // or slow peer cannot hold one of the transport's connection places.
    const std::int64_t now = t_.now_us();
    std::vector<net::ConnectionId> late;
    for (auto& kv : conns_)
        if (kv.second.state != State::Active &&
            now - kv.second.started_us > std::int64_t(config_.handshake_timeout_ms) * 1000)
            late.push_back(kv.first);
    for (net::ConnectionId id : late)
    {
        ++counters_.handshake_timeouts;
        closeConn(id, close_code::kHandshakeTimeout, "handshake timeout", false, nullptr, "handshake timeout");
    }
}

void Server::onConnected(net::ConnectionId id)
{
    net::ConnectionStatus st;
    std::string address = "?";
    if (t_.status(id, st))
        address = AddressOf(st.remote_address);
    // Per-address rate of new handshakes.
    const std::int64_t now = t_.now_us();
    auto& times = handshakesByAddress_[address];
    while (!times.empty() && now - times.front() > std::int64_t(config_.address_window_ms) * 1000)
        times.pop_front();
    if (times.size() >= config_.max_handshakes_per_address)
    {
        ++counters_.rejected_address_rate;
        note("refused a connection from " + address + ": too many handshakes");
        t_.close(id, close_code::kRateLimited, "too many connection attempts", false);
        return;
    }
    times.push_back(now);
    if (handshakesByAddress_.size() > 4096)
        handshakesByAddress_.erase(handshakesByAddress_.begin()); // bound the table itself
    Conn& c = conns_[id];
    c.id = id;
    c.address = address;
    c.started_us = now;
    ++counters_.handshakes_started;
}

void Server::onDisconnected(net::ConnectionId id, const std::string& why, std::vector<SessionEvent>& out)
{
    auto it = conns_.find(id);
    if (it == conns_.end())
        return;
    if (it->second.state == State::Active && it->second.slot >= 0)
    {
        Slot& s = slots_[std::size_t(it->second.slot)];
        if (s.conn == id)
        {
            s.conn = net::kNoConnection;
            SessionEvent e;
            e.type = SessionEventType::Left;
            e.player = s.player;
            e.slot = std::uint8_t(it->second.slot);
            e.slot_generation = s.generation;
            e.session_id = s.session_id;
            e.detail = why;
            out.push_back(std::move(e));
        }
    }
    conns_.erase(it);
}

void Server::closeConn(net::ConnectionId id, int code, const char* detail, bool flush, std::vector<SessionEvent>* out,
    const std::string& why)
{
    t_.close(id, code, detail, flush);
    std::vector<SessionEvent> scratch;
    onDisconnected(id, why, out ? *out : scratch);
}

void Server::violation(Conn& c, const std::string& why, std::vector<SessionEvent>& out)
{
    ++counters_.violations;
    note("protocol violation from " + c.address + ": " + why);
    closeConn(c.id, net::close_code::kProtocolViolation, "protocol violation", false, &out, "protocol violation: " + why);
}

bool Server::sendTo(Conn& c, const proto::Message& m)
{
    const proto::MessageType type = proto::type_of(m);
    const std::uint64_t epoch = proto::handshake_type(std::uint16_t(type)) ? 0 : c.guard.epoch();
    std::vector<std::uint8_t> bytes;
    if (!proto::encode(m, epoch, c.seq.next(proto::channel_of(type)), bytes))
        return false;
    return t_.send(c.id, proto::channel_of(type), bytes.data(), std::uint32_t(bytes.size())) == net::SendResult::Sent;
}

void Server::reject(Conn& c, proto::RejectReason reason, const char* detail)
{
    proto::Reject r;
    r.reason = std::uint16_t(reason);
    r.detail = detail;
    sendTo(c, r);
    note(std::string("rejected ") + c.address + ": " + detail);
    // Flush so the Reject arrives before the close.
    closeConn(c.id, close_code::kRejected, detail, true, nullptr, detail);
}

bool Server::allow(Conn& c, net::ChannelId ch)
{
    const RateLimit& lim = config_.rates[ch];
    Bucket& b = c.buckets[ch];
    const std::int64_t now = t_.now_us();
    if (!b.started)
    {
        b.started = true;
        b.tokens = lim.burst;
    }
    else
        b.tokens = std::min(lim.burst, b.tokens + float(now - b.at_us) * 1e-6f * lim.per_second);
    b.at_us = now;
    if (b.tokens < 1.f)
        return false;
    b.tokens -= 1.f;
    return true;
}

void Server::onMessage(Conn& c, const net::Event& e, std::vector<SessionEvent>& out)
{
    proto::Decoded d;
    const proto::DecodeResult r = proto::decode(e.channel, proto::Role::Server, e.payload.data(), e.payload.size(), d);
    if (r.ignorable)
        return;
    if (!r.ok())
        return violation(c, std::string("malformed ") + proto::to_string(r.error) + " at " + r.field, out);
    const proto::GuardResult g = c.guard.check(e.channel, d.header);
    if (g.verdict == proto::Verdict::Violation)
        return violation(c, g.why, out);
    if (g.verdict == proto::Verdict::Drop)
    {
        ++counters_.dropped_by_guard;
        return;
    }
    switch (c.state)
    {
    case State::AwaitHello:
        if (auto* h = std::get_if<proto::Hello>(&d.body))
            return onHello(c, *h);
        return violation(c, std::string("expected hello, got ") + proto::to_string(proto::MessageType(d.header.type)), out);
    case State::AwaitProof:
        if (auto* p = std::get_if<proto::AuthProof>(&d.body))
            return onProof(c, *p, out);
        return violation(c, std::string("expected auth proof, got ") + proto::to_string(proto::MessageType(d.header.type)), out);
    case State::Active:
    {
        if (!allow(c, e.channel))
        {
            const bool reliable = e.channel != proto::kSnapshot && e.channel != proto::kInput;
            if (!reliable)
            {
                ++counters_.rate_limited_drops;
                return;
            }
            ++counters_.rate_limited_closes;
            note("rate limit on channel " + std::to_string(e.channel) + " from " + c.address);
            return closeConn(c.id, close_code::kRateLimited, "rate limit", false, &out, "rate limit");
        }
        const Slot& s = slots_[std::size_t(c.slot)];
        SessionEvent ev;
        ev.type = SessionEventType::Message;
        ev.player = s.player; // from the binding, never from the message
        ev.slot = std::uint8_t(c.slot);
        ev.slot_generation = s.generation;
        ev.session_id = s.session_id;
        ev.channel = e.channel;
        ev.message = std::move(d);
        out.push_back(std::move(ev));
        return;
    }
    }
}

void Server::onHello(Conn& c, const proto::Hello& h)
{
    const std::uint16_t why = proto::check_hello(h, config_.build, nullptr);
    if (why == std::uint16_t(proto::RejectReason::VersionMismatch))
    {
        ++counters_.rejected_version;
        return reject(c, proto::RejectReason::VersionMismatch, "protocol version differs");
    }
    if (why == std::uint16_t(proto::RejectReason::BuildMismatch))
    {
        ++counters_.rejected_build;
        return reject(c, proto::RejectReason::BuildMismatch, "game build differs");
    }
    if (why == std::uint16_t(proto::RejectReason::ContentMismatch))
    {
        ++counters_.rejected_content;
        return reject(c, proto::RejectReason::ContentMismatch, "game content differs");
    }
    c.hello = h; // exactly as sent: the transcript covers it
    // Always challenge, even for players not on the roster, so a stranger
    // learns nothing about which player ids exist.
    random_bytes(c.challenge.server_nonce.data(), c.challenge.server_nonce.size());
    c.challenge.campaign_id = config_.campaign_id;
    sendTo(c, c.challenge);
    c.state = State::AwaitProof;
}

void Server::onProof(Conn& c, const proto::AuthProof& p, std::vector<SessionEvent>& out)
{
    const RosterEntry* who = roster_.find(c.hello.player_id);
    if (!who || !check_proof(c.hello, c.challenge, who->key, p))
    {
        ++counters_.rejected_auth;
        return reject(c, proto::RejectReason::AuthFailed, "authentication failed");
    }
    // The player's slot: theirs from before (reconnect), else a free one.
    int slot = -1;
    for (std::size_t i = 0; i < slots_.size(); ++i)
        if (slots_[i].bound && slots_[i].player == who->player_id)
            slot = int(i);
    if (slot < 0)
    {
        for (std::size_t i = 0; i < slots_.size() && slot < 0; ++i)
            if (!slots_[i].bound)
                slot = int(i);
        if (slot < 0)
        {
            ++counters_.rejected_full;
            return reject(c, proto::RejectReason::ServerFull, "all player slots are taken");
        }
        Slot& s = slots_[std::size_t(slot)];
        s.bound = true;
        s.player = who->player_id;
        ++s.generation;
    }
    Slot& s = slots_[std::size_t(slot)];
    if (s.conn != net::kNoConnection)
    {
        // The same player again: the new authenticated connection replaces
        // the old one, which may be dead and waiting for its timeout
        // (S04-T01 finding 2).
        ++counters_.replaced;
        const net::ConnectionId old = s.conn;
        auto oldIt = conns_.find(old);
        std::string oldAddress = oldIt != conns_.end() ? oldIt->second.address : "?";
        t_.close(old, close_code::kReplaced, "replaced by a new connection", false);
        SessionEvent left;
        left.type = SessionEventType::Left;
        left.player = s.player;
        left.slot = std::uint8_t(slot);
        left.slot_generation = s.generation;
        left.session_id = s.session_id;
        left.replaced = true;
        left.detail = "replaced by a new connection";
        out.push_back(std::move(left));
        s.conn = net::kNoConnection;
        conns_.erase(old);
        note("player in slot " + std::to_string(slot) + " reconnected from " + c.address + "; closed " + oldAddress);
    }
    proto::Welcome w;
    w.minor = std::min(c.hello.minor, config_.build.minor);
    w.campaign_id = config_.campaign_id;
    w.session_id = random_uuid();
    w.player_id = who->player_id;
    w.party_id = config_.party_id;
    w.slot = std::uint8_t(slot);
    w.slot_generation = s.generation;
    w.authority_epoch = config_.authority_epoch;
    w.server_tick = 0;
    w.tick_rate_hz = config_.tick_rate_hz;
    w.snapshot_rate_hz = config_.snapshot_rate_hz;
    w.resume_token.resize(32);
    random_bytes(w.resume_token.data(), w.resume_token.size());
    s.resume_token_hash = sha256(w.resume_token.data(), w.resume_token.size());
    if (!sendTo(c, w))
        return violation(c, "welcome could not be sent", out);
    std::fill(w.resume_token.begin(), w.resume_token.end(), std::uint8_t(0));
    c.guard.begin_session(config_.authority_epoch);
    c.state = State::Active;
    c.slot = slot;
    s.conn = c.id;
    s.session_id = w.session_id;
    ++counters_.joins;
    note("player in slot " + std::to_string(slot) + " joined from " + c.address);
    SessionEvent j;
    j.type = SessionEventType::Joined;
    j.player = s.player;
    j.slot = std::uint8_t(slot);
    j.slot_generation = s.generation;
    j.session_id = s.session_id;
    out.push_back(std::move(j));
}

bool Server::send(const proto::Uuid128& player, const proto::Message& message)
{
    for (Slot& s : slots_)
        if (s.bound && s.player == player && s.conn != net::kNoConnection)
        {
            auto it = conns_.find(s.conn);
            return it != conns_.end() && it->second.state == State::Active && sendTo(it->second, message);
        }
    return false;
}

bool Server::release(const proto::Uuid128& player)
{
    for (Slot& s : slots_)
        if (s.bound && s.player == player)
        {
            if (s.conn != net::kNoConnection)
            {
                t_.close(s.conn, close_code::kReleased, "released", true);
                conns_.erase(s.conn);
            }
            const std::uint32_t generation = s.generation;
            s = Slot{};
            s.generation = generation; // the next owner gets generation + 1
            return true;
        }
    return false;
}

std::vector<PlayerView> Server::players() const
{
    std::vector<PlayerView> v;
    for (std::size_t i = 0; i < slots_.size(); ++i)
        if (slots_[i].bound)
            v.push_back({slots_[i].player, std::uint8_t(i), slots_[i].generation, slots_[i].conn != net::kNoConnection,
                slots_[i].session_id});
    return v;
}
} // namespace coop::session
