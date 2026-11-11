#include "coop/session/client.h"

namespace coop::session
{
Client::Client(net::Transport& transport, const ClientConfig& config, PrivateKey key)
    : t_(transport), config_(config), key_(std::move(key))
{
}

bool Client::connect(const std::string& address)
{
    if (state_ != ClientState::Idle)
        return false;
    conn_ = t_.connect(address.c_str());
    if (conn_ == net::kNoConnection)
        return false;
    state_ = ClientState::Connecting;
    return true;
}

bool Client::sendRaw(const proto::Message& m)
{
    const proto::MessageType type = proto::type_of(m);
    const std::uint64_t epoch = proto::handshake_type(std::uint16_t(type)) ? 0 : guard_.epoch();
    std::vector<std::uint8_t> bytes;
    const net::ChannelId channel = proto::channel_of(type);
    // B64: a reliable channel's sequence is taken only by a sent message
    // (SequenceCounter::done)
    const bool sent = proto::encode(m, epoch, seq_.peek(channel), bytes) &&
        t_.send(conn_, channel, bytes.data(), std::uint32_t(bytes.size())) == net::SendResult::Sent;
    seq_.done(channel, sent);
    return sent;
}

bool Client::send(const proto::Message& m) { return state_ == ClientState::Active && sendRaw(m); }

void Client::close(int code, const char* detail)
{
    if (conn_ != net::kNoConnection)
        t_.close(conn_, code, detail, true);
    conn_ = net::kNoConnection;
    state_ = ClientState::Ended;
}

void Client::end(net::CloseReason reason, int code, const std::string& detail, std::vector<ClientEvent>& out)
{
    if (state_ == ClientState::Ended)
        return;
    state_ = ClientState::Ended;
    conn_ = net::kNoConnection;
    ClientEvent e;
    e.type = ClientEventType::Ended;
    e.reason = reason;
    e.code = code;
    e.detail = detail;
    out.push_back(std::move(e));
}

void Client::violation(const std::string& why, std::vector<ClientEvent>& out)
{
    ++violations_;
    t_.close(conn_, net::close_code::kProtocolViolation, "protocol violation", false);
    end(net::CloseReason::ProtocolViolation, net::close_code::kProtocolViolation, why, out);
}

std::optional<proto::ResumeRequest> Client::resume_request(std::uint64_t last_event, std::uint64_t last_snapshot) const
{
    if (welcome_.session_id.zero())
        return std::nullopt;
    proto::ResumeRequest r;
    r.session_id = welcome_.session_id;
    r.token = welcome_.resume_token;
    r.last_event = last_event;
    r.last_snapshot = last_snapshot;
    return r;
}

void Client::tick(std::vector<ClientEvent>& out)
{
    if (state_ != ClientState::Active)
        return;
    const std::int64_t now = t_.now_us();
    const LivenessConfig& l = config_.liveness;
    const std::int64_t silent = now - lastHeard_us_;
    if (silent > std::int64_t(l.timeout_ms) * 1000)
    {
        t_.close(conn_, close_code::kTimeout, "timed out", false);
        return end(net::CloseReason::Timeout, close_code::kTimeout, "nothing heard from the server", out);
    }
    if (!suspect_ && silent > std::int64_t(l.suspect_after_ms) * 1000)
    {
        suspect_ = true;
        ClientEvent e;
        e.type = ClientEventType::Suspect;
        out.push_back(std::move(e));
    }
    if (now >= nextBeat_us_)
    {
        nextBeat_us_ = now + std::int64_t(l.heartbeat_interval_ms) * 1000;
        proto::Heartbeat hb;
        hb.nonce = ++beatNonce_;
        if (sendRaw(hb))
        {
            beats_.push_back({hb.nonce, now});
            if (beats_.size() > 64)
                beats_.pop_front();
        }
    }
}

void Client::update(std::vector<ClientEvent>& out)
{
    events_.clear();
    t_.poll(events_);
    for (const net::Event& e : events_)
    {
        if (e.connection != conn_ || state_ == ClientState::Ended)
            continue;
        if (e.type == net::EventType::Connected)
        {
            hello_ = proto::Hello{};
            hello_.major = config_.build.major;
            hello_.minor = config_.build.minor;
            hello_.build_id = config_.build.build_id;
            hello_.content_digest = config_.build.content_digest;
            hello_.player_id = config_.player_id;
            hello_.display_name = config_.display_name;
            random_bytes(hello_.client_nonce.data(), hello_.client_nonce.size());
            if (config_.resume)
            {
                hello_.resume = true;
                hello_.resume_request = *config_.resume;
            }
            if (!sendRaw(hello_))
                return violation("hello could not be sent", out);
            state_ = ClientState::AwaitChallenge;
            continue;
        }
        if (e.type == net::EventType::Disconnected)
        {
            end(e.reason, e.code, e.detail, out);
            continue;
        }
        proto::Decoded d;
        const proto::DecodeResult r = proto::decode(e.channel, proto::Role::Client, e.payload.data(), e.payload.size(), d);
        if (r.ignorable)
            continue;
        if (!r.ok())
            return violation(std::string("malformed ") + proto::to_string(r.error) + " at " + r.field, out);
        const proto::GuardResult g = guard_.check(e.channel, d.header);
        if (g.verdict == proto::Verdict::Violation)
            return violation(g.why, out);
        if (g.verdict == proto::Verdict::Drop)
            continue;
        if (auto* rj = std::get_if<proto::Reject>(&d.body))
        {
            if (state_ != ClientState::AwaitChallenge && state_ != ClientState::AwaitWelcome)
                return violation("reject outside the handshake", out);
            ClientEvent ev;
            ev.type = ClientEventType::Rejected;
            ev.reject = *rj;
            out.push_back(std::move(ev));
            continue; // the server closes next
        }
        switch (state_)
        {
        case ClientState::AwaitChallenge:
        {
            auto* ch = std::get_if<proto::AuthChallenge>(&d.body);
            if (!ch)
                return violation("expected a challenge", out);
            if (!config_.expected_campaign.zero() && ch->campaign_id != config_.expected_campaign)
                return violation("challenge for another campaign", out);
            proto::AuthProof proof;
            const bool made = config_.join_code.empty() ? make_proof(hello_, *ch, key_, proof)
                                                        : make_join_proof(hello_, *ch, key_, config_.join_code, proof);
            if (!made || !sendRaw(proof))
                return violation("proof could not be made", out);
            state_ = ClientState::AwaitWelcome;
            break;
        }
        case ClientState::AwaitWelcome:
        {
            auto* w = std::get_if<proto::Welcome>(&d.body);
            if (!w || w->player_id != config_.player_id)
                return violation("expected a welcome for this player", out);
            welcome_ = *w;
            guard_.begin_session(w->authority_epoch);
            state_ = ClientState::Active;
            lastHeard_us_ = t_.now_us();
            nextBeat_us_ = lastHeard_us_;
            ClientEvent ev;
            ev.type = ClientEventType::Welcomed;
            ev.welcome = *w;
            ev.resumed = config_.resume && config_.resume->session_id == w->session_id;
            out.push_back(std::move(ev));
            break;
        }
        case ClientState::Active:
        {
            lastHeard_us_ = t_.now_us();
            if (suspect_)
            {
                suspect_ = false;
                ClientEvent rec;
                rec.type = ClientEventType::Recovered;
                out.push_back(std::move(rec));
            }
            if (auto* hb = std::get_if<proto::Heartbeat>(&d.body))
            {
                for (auto it = beats_.begin(); it != beats_.end(); ++it)
                    if (it->first == hb->echo)
                    {
                        rtts_.push_back(double(lastHeard_us_ - it->second) / 1000.0);
                        beats_.erase(beats_.begin(), it + 1);
                        break;
                    }
                if (hb->echo == 0) // the server asking: answer
                {
                    proto::Heartbeat reply;
                    reply.nonce = ++beatNonce_;
                    reply.echo = hb->nonce;
                    sendRaw(reply);
                }
                break;
            }
            ClientEvent ev;
            ev.type = ClientEventType::Message;
            ev.channel = e.channel;
            ev.message = std::move(d);
            out.push_back(std::move(ev));
            break;
        }
        default:
            return violation("message in the wrong state", out);
        }
    }
    tick(out);
}
} // namespace coop::session
