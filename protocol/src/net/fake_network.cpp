// In-memory Transport backend. See fake_network.h for the model.
#include "coop/net/fake_network.h"

#include "policy.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <queue>
#include <random>

namespace coop::net
{
namespace
{
class FakeTransport;

struct Msg
{
    std::uint8_t lane = 0;
    bool reliable = true;
    std::vector<std::uint8_t> bytes;
    std::int64_t at_us = 0; // arrival time once in flight
};

// One direction of a connection, owned by the sending side.
struct Direction
{
    std::vector<std::deque<Msg>> queued; // per sender channel
    std::vector<std::uint64_t> queuedBytes;
    std::vector<std::uint64_t> unacked;
    std::deque<Msg> flight; // arrival order
    struct Ack
    {
        std::int64_t at_us;
        std::uint8_t lane;
        std::uint32_t bytes;
    };
    std::deque<Ack> acks;
    double tokens = 0;
    std::size_t rr = 0;
    bool linger = false; // closed with flush: send the close once queues are empty
    bool closeSent = false;
    int closeCode = 0;
    std::string closeDetail;

    void init(std::size_t channels)
    {
        queued.assign(channels, {});
        queuedBytes.assign(channels, 0);
        unacked.assign(channels, 0);
    }
    bool empty() const
    {
        for (auto b : queuedBytes)
            if (b)
                return false;
        return true;
    }
};

// A connection between side 0 (connector) and side 1 (listener).
struct Link
{
    int endpoint[2] = {0, 0}; // endpoint ids; 0 = none or destroyed
    ConnectionId id[2] = {kNoConnection, kNoConnection};
    Direction dir[2];         // dir[s]: from side s to side 1-s
    bool open[2] = {true, false};
    std::int64_t silentSince = -1; // a side crashed at this time
    bool timeoutSent[2] = {false, false};
};

struct Notice
{
    enum Kind
    {
        Incoming,  // listener: a connect request (link side 0 is the requester)
        Connected,
        Closed,    // the peer closed with `code`
        TimedOut,  // the peer went silent
        ConnectTimeout,
    } kind;
    std::shared_ptr<Link> link;
    int code = 0;
    std::string detail;
};
} // namespace

struct FakeNetwork::State
{
    FakeLinkConfig link;
    std::mt19937_64 rng;
    std::int64_t now = 0;
    int nextEndpoint = 1;
    std::map<int, FakeTransport*> endpoints;
    std::map<std::string, int> listeners;
    std::vector<std::weak_ptr<Link>> links;
    struct Timed
    {
        std::int64_t at;
        std::uint64_t seq;
        std::function<void()> fn;
        bool operator>(const Timed& o) const { return at != o.at ? at > o.at : seq > o.seq; }
    };
    std::priority_queue<Timed, std::vector<Timed>, std::greater<Timed>> timed;
    std::uint64_t seq = 0;

    explicit State(const FakeLinkConfig& l) : link(l), rng(l.seed) {}

    std::int64_t latencyUs() const { return std::int64_t(link.latency_ms) * 1000; }
    void at(std::int64_t when, std::function<void()> fn) { timed.push({when, seq++, std::move(fn)}); }
    FakeTransport* endpoint(int id) const
    {
        auto it = endpoints.find(id);
        return it == endpoints.end() ? nullptr : it->second;
    }
    void notify(int endpointId, Notice n);
    void step();
    void transmit(Link& l, int s);
    void deliver(Link& l, int s);
};

namespace
{
class FakeTransport final : public Transport
{
public:
    FakeTransport(std::shared_ptr<FakeNetwork::State> net, int id, const TransportConfig& config)
        : net_(std::move(net)), id_(id), config_(config)
    {
        counters_.channels.resize(config_.channels.size());
    }

    ~FakeTransport() override
    {
        if (!closed_ && !crashed_)
            closeAll(0);
        net_->endpoints.erase(id_);
        if (!listenName_.empty())
            net_->listeners.erase(listenName_);
    }

    // ---- called by the network
    bool crashed() const { return crashed_; }
    bool acceptsRequests() const { return !crashed_ && !closed_; }
    void crash()
    {
        crashed_ = true;
        for (auto& kv : conns_)
            if (kv.second.link->silentSince < 0)
                kv.second.link->silentSince = net_->now;
        if (!listenName_.empty())
            net_->listeners.erase(listenName_);
        listenName_.clear();
    }
    void addNotice(Notice n) { notices_.push_back(std::move(n)); }
    enum class Accept
    {
        Accepted,
        Full,
        Vanished,
    };
    Accept deliver(ConnectionId id, Msg& m)
    {
        if (crashed_)
            return Accept::Vanished;
        auto it = conns_.find(id);
        if (it == conns_.end())
            return Accept::Vanished;
        Conn& c = it->second;
        if (c.inbox.size() >= config_.recv_buffer_messages || c.inboxBytes + m.bytes.size() > config_.recv_buffer_bytes)
            return Accept::Full;
        c.inboxBytes += m.bytes.size();
        c.inbox.push_back(std::move(m));
        return Accept::Accepted;
    }

    // ---- Transport
    bool listen(const char* address, std::string* error) override
    {
        if (!owner_.here())
            return wrongThread(error);
        if (closed_ || crashed_)
            return fail(error, "transport is shut down");
        if (!listenName_.empty())
            return fail(error, "already listening");
        if (!address || !*address)
            return fail(error, "bad address");
        if (net_->listeners.count(address))
            return fail(error, std::string("address in use: ") + address);
        listenName_ = address;
        net_->listeners[listenName_] = id_;
        return true;
    }

    ConnectionId connect(const char* address, std::string* error) override
    {
        if (!owner_.here())
        {
            wrongThread(error);
            return kNoConnection;
        }
        if (closed_ || crashed_ || !address || !*address)
        {
            fail(error, closed_ || crashed_ ? "transport is shut down" : "bad address");
            return kNoConnection;
        }
        auto link = std::make_shared<Link>();
        link->endpoint[0] = id_;
        link->dir[0].init(config_.channels.size());
        const ConnectionId id = addConn(link, 0, false, address);
        link->id[0] = id;
        net_->links.push_back(link);
        FakeNetwork::State* net = net_.get(); // the network owns its timed callbacks
        const std::string target = address;
        net_->at(net_->now + net_->latencyUs(), [net, link, target] {
            auto it = net->listeners.find(target);
            if (it != net->listeners.end())
                net->notify(it->second, {Notice::Incoming, link});
        });
        const int me = id_;
        net_->at(net_->now + std::int64_t(config_.connect_timeout_ms) * 1000,
            [net, link, me] { net->notify(me, {Notice::ConnectTimeout, link}); });
        return id;
    }

    SendResult send(ConnectionId id, ChannelId channel, const void* data, std::uint32_t size) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return SendResult::WrongThread;
        }
        if (closed_ || crashed_)
            return SendResult::NotConnected;
        if (channel >= config_.channels.size())
            return SendResult::InvalidChannel;
        auto it = conns_.find(id);
        if (it == conns_.end() || it->second.state != ConnectionState::Connected)
            return SendResult::NotConnected;
        Conn& c = it->second;
        const ChannelSpec& spec = config_.channels[channel];
        if (size > spec.max_message_bytes)
            return SendResult::TooLarge;
        detail::Admission a = detail::admit(spec, c.est[channel], size);
        if (a != detail::Admission::Send)
        {
            refresh(c);
            a = detail::admit(spec, c.est[channel], size);
        }
        detail::count_send(counters_.channels[channel], a, size);
        switch (a)
        {
        case detail::Admission::Send:
        {
            Direction& d = c.link->dir[c.side];
            Msg m;
            m.lane = channel;
            m.reliable = spec.delivery != Delivery::Transient;
            const auto* p = static_cast<const std::uint8_t*>(data);
            m.bytes.assign(p, p + size);
            d.queued[channel].push_back(std::move(m));
            d.queuedBytes[channel] += size;
            c.est[channel].queued_since_refresh += size;
            return SendResult::Sent;
        }
        case detail::Admission::DropStale:
        case detail::Admission::DropBacklog: return SendResult::Dropped;
        case detail::Admission::Backpressure: return SendResult::Backpressure;
        case detail::Admission::Overload:
            closeByAdapter(id, close_code::kOverloaded, CloseReason::Overloaded,
                "critical backlog on channel " + spec.name + " over bound", pending_);
            return SendResult::Overloaded;
        }
        return SendResult::Failed;
    }

    void flush(ConnectionId) override
    {
        if (!owner_.here())
            ++wrongThreadCalls_;
    }

    void poll(std::vector<Event>& out) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return;
        }
        if (closed_ || crashed_)
            return;
        for (Event& e : pending_)
            out.push_back(std::move(e));
        pending_.clear();
        handleNotices(out);
        std::vector<ConnectionId> ids;
        for (auto& kv : conns_)
            if (kv.second.state == ConnectionState::Connected)
                ids.push_back(kv.first);
        if (!ids.empty())
        {
            const std::size_t start = rotation_++ % ids.size();
            for (std::size_t i = 0; i < ids.size(); ++i)
                receive(ids[(start + i) % ids.size()], config_.max_messages_per_poll, out);
        }
        for (auto& kv : conns_)
            refresh(kv.second);
    }

    bool close(ConnectionId id, int code, const char* detailText, bool flush) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return false;
        }
        if (!detail::valid_close_code(code) || closed_ || crashed_)
            return false;
        auto it = conns_.find(id);
        if (it == conns_.end())
            return false;
        closeLink(it->second, code, detailText ? detailText : "", flush);
        conns_.erase(it);
        return true;
    }

    ShutdownReport shutdown(std::uint32_t flushTimeoutMs) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return {};
        }
        if (closed_ || crashed_)
            return {};
        return closeAll(flushTimeoutMs);
    }

    bool status(ConnectionId id, ConnectionStatus& out) const override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return false;
        }
        auto it = conns_.find(id);
        if (it == conns_.end())
            return false;
        const Conn& c = it->second;
        out.state = c.state;
        out.incoming = c.incoming;
        out.remote_address = c.address;
        out.ping_ms = int(2 * net_->link.latency_ms);
        out.quality_local = out.quality_remote = float(1.0 - net_->link.transient_loss);
        out.out_bytes_per_s = out.in_bytes_per_s = 0.f;
        out.channels.clear();
        for (const auto& e : c.est)
        {
            ChannelBacklog b = e.last; // plus what this side queued since, so producers can pace
            b.queued_bytes += std::uint32_t(e.queued_since_refresh);
            out.channels.push_back(b);
        }
        return true;
    }

    TransportCounters counters() const override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            TransportCounters only;
            only.wrong_thread_calls = wrongThreadCalls_.load();
            return only;
        }
        TransportCounters c = counters_;
        c.wrong_thread_calls = wrongThreadCalls_.load();
        return c;
    }
    const TransportConfig& config() const override { return config_; }
    std::int64_t now_us() const override { return net_->now; }

private:
    struct Conn
    {
        ConnectionId id = kNoConnection;
        std::shared_ptr<Link> link;
        int side = 0;
        bool incoming = false;
        ConnectionState state = ConnectionState::Connecting;
        std::string address;
        std::deque<Msg> inbox;
        std::uint64_t inboxBytes = 0;
        std::vector<detail::ChannelEstimate> est;
    };

    bool wrongThread(std::string* error)
    {
        ++wrongThreadCalls_;
        return fail(error, "called off the owner thread");
    }
    static bool fail(std::string* error, const std::string& what)
    {
        if (error)
            *error = what;
        return false;
    }

    ConnectionId addConn(const std::shared_ptr<Link>& link, int side, bool incoming, const std::string& address)
    {
        const ConnectionId id = nextId_++;
        Conn& c = conns_[id];
        c.id = id;
        c.link = link;
        c.side = side;
        c.incoming = incoming;
        c.address = address;
        c.est.resize(config_.channels.size());
        return id;
    }

    Conn* findByLink(const Link* link)
    {
        for (auto& kv : conns_)
            if (kv.second.link.get() == link)
                return &kv.second;
        return nullptr;
    }

    // Exact figures straight from the simulated link.
    void refresh(Conn& c)
    {
        const Direction& d = c.link->dir[c.side];
        const double bytesPerMs = config_.send_rate_bytes_per_s / 1000.0;
        for (std::size_t i = 0; i < c.est.size(); ++i)
        {
            std::uint64_t ahead = 0; // bytes that go before a message queued now on lane i
            for (std::size_t j = 0; j < c.est.size(); ++j)
                if (config_.channels[j].priority <= config_.channels[i].priority)
                    ahead += d.queuedBytes[j];
            ChannelBacklog& b = c.est[i].last;
            b.queued_bytes = std::uint32_t(d.queuedBytes[i]);
            b.unacked_bytes = std::uint32_t(d.unacked[i]);
            b.queue_ms = std::uint32_t(ahead / bytesPerMs + 0.5);
            c.est[i].queued_since_refresh = 0;
        }
    }

    void handleNotices(std::vector<Event>& out)
    {
        std::vector<Notice> list;
        list.swap(notices_);
        for (Notice& n : list)
            handleNotice(n, out);
    }

    void handleNotice(Notice& n, std::vector<Event>& out)
    {
        if (n.kind == Notice::Incoming)
        {
            acceptIncoming(n.link);
            return;
        }
        Conn* c = findByLink(n.link.get());
        if (!c)
        {
            if (n.kind != Notice::ConnectTimeout)
                ++counters_.stale_notifications;
            return;
        }
        const ConnectionId id = c->id;
        switch (n.kind)
        {
        case Notice::Connected:
            if (c->state == ConnectionState::Connecting)
            {
                c->state = ConnectionState::Connected;
                if (c->incoming)
                    ++counters_.connections_accepted;
                Event e;
                e.type = EventType::Connected;
                e.connection = id;
                e.incoming = c->incoming;
                out.push_back(std::move(e));
            }
            break;
        case Notice::ConnectTimeout:
            if (c->state == ConnectionState::Connecting && !c->incoming)
                disconnect(id, CloseReason::ConnectFailed, 0, "connect timed out", out);
            break;
        case Notice::Closed:
        case Notice::TimedOut:
        {
            const bool wasConnected = c->state == ConnectionState::Connected;
            if (wasConnected)
                receive(id, 0xffffffffu, out);
            if (!conns_.count(id))
                return;
            CloseReason reason = n.kind == Notice::TimedOut ? CloseReason::Timeout
                                                            : detail::reason_from_peer_code(n.code);
            if (!wasConnected && reason != CloseReason::Refused)
                reason = CloseReason::ConnectFailed;
            if (wasConnected || !conns_[id].incoming)
                disconnect(id, reason, n.kind == Notice::TimedOut ? 0 : n.code, n.detail, out);
            else
                dropConn(id);
            break;
        }
        default: break;
        }
    }

    void acceptIncoming(const std::shared_ptr<Link>& link)
    {
        if (link->endpoint[0] == 0 || !link->open[0])
            return; // requester already gone
        std::uint32_t open = 0;
        for (const auto& kv : conns_)
            open += kv.second.incoming ? 1u : 0u;
        const int requester = link->endpoint[0];
        FakeNetwork::State* net = net_.get(); // the network owns its timed callbacks
        auto refuse = [&](int code, const char* why) {
            ++counters_.connections_refused;
            const std::string w = why;
            net_->at(net_->now + net_->latencyUs(),
                [net, link, requester, code, w] { net->notify(requester, {Notice::Closed, link, code, w}); });
        };
        if (closed_)
            return refuse(close_code::kShutdown, "shutting down");
        if (open >= config_.max_connections)
            return refuse(close_code::kServerFull, "server full");
        link->endpoint[1] = id_;
        link->open[1] = true;
        link->dir[1].init(config_.channels.size());
        link->id[1] = addConn(link, 1, true, "fake:" + std::to_string(requester));
        const int me = id_;
        net_->at(net_->now + net_->latencyUs(), [net, link, requester] { net->notify(requester, {Notice::Connected, link}); });
        net_->at(net_->now + 2 * net_->latencyUs(), [net, link, me] { net->notify(me, {Notice::Connected, link}); });
    }

    void receive(ConnectionId id, std::uint32_t budget, std::vector<Event>& out)
    {
        auto it = conns_.find(id);
        if (it == conns_.end())
            return;
        Conn& c = it->second;
        while (budget > 0 && !c.inbox.empty())
        {
            Msg m = std::move(c.inbox.front());
            c.inbox.pop_front();
            c.inboxBytes -= m.bytes.size();
            --budget;
            const auto check = detail::check_inbound(config_, m.lane, m.reliable, std::uint32_t(m.bytes.size()));
            if (check != detail::InboundCheck::Ok)
            {
                ++counters_.protocol_violations;
                closeByAdapter(id, close_code::kProtocolViolation, CloseReason::ProtocolViolation,
                    std::string(detail::to_string(check)) + " (lane " + std::to_string(m.lane) + ", " +
                        std::to_string(m.bytes.size()) + " bytes)",
                    out);
                return;
            }
            ChannelCounters& cc = counters_.channels[m.lane];
            ++cc.received_messages;
            cc.received_bytes += m.bytes.size();
            Event e;
            e.type = EventType::Message;
            e.connection = id;
            e.channel = m.lane;
            e.payload = std::move(m.bytes);
            e.received_us = m.at_us;
            out.push_back(std::move(e));
        }
    }

    void closeLink(Conn& c, int code, const std::string& why, bool flush)
    {
        Link& l = *c.link;
        l.open[c.side] = false;
        Direction& d = l.dir[c.side];
        d.closeCode = code;
        d.closeDetail = why;
        if (flush && !d.empty())
        {
            d.linger = true; // State::step sends the close when the queues are empty
            return;
        }
        for (std::size_t i = 0; i < d.queued.size(); ++i)
        {
            d.queued[i].clear();
            d.queuedBytes[i] = 0;
        }
        d.closeSent = true;
        const int peer = l.endpoint[1 - c.side];
        if (peer)
        {
            FakeNetwork::State* net = net_.get(); // the network owns its timed callbacks
            auto link = c.link;
            net_->at(net_->now + net_->latencyUs(),
                [net, link, peer, code, why] { net->notify(peer, {Notice::Closed, link, code, why}); });
        }
    }

    void disconnect(ConnectionId id, CloseReason reason, int code, const std::string& why, std::vector<Event>& out)
    {
        Event e;
        e.type = EventType::Disconnected;
        e.connection = id;
        e.incoming = conns_[id].incoming;
        e.reason = reason;
        e.code = code;
        e.detail = why;
        out.push_back(std::move(e));
        if (reason == CloseReason::ConnectFailed && !conns_[id].incoming)
            ++counters_.connections_failed;
        dropConn(id);
    }

    void dropConn(ConnectionId id)
    {
        auto it = conns_.find(id);
        if (it == conns_.end())
            return;
        it->second.link->open[it->second.side] = false;
        conns_.erase(it);
    }

    void closeByAdapter(ConnectionId id, int code, CloseReason reason, const std::string& why, std::vector<Event>& out)
    {
        auto it = conns_.find(id);
        if (it == conns_.end())
            return;
        closeLink(it->second, code, why, false);
        Event e;
        e.type = EventType::Disconnected;
        e.connection = id;
        e.incoming = it->second.incoming;
        e.reason = reason;
        e.code = code;
        e.detail = why;
        out.push_back(std::move(e));
        conns_.erase(it);
    }

    std::uint64_t reliableBacklog(const Conn& c) const
    {
        const Direction& d = c.link->dir[c.side];
        std::uint64_t b = 0;
        for (std::size_t i = 0; i < d.queuedBytes.size(); ++i)
            if (config_.channels[i].delivery != Delivery::Transient)
                b += d.queuedBytes[i] + d.unacked[i];
        return b;
    }

    ShutdownReport closeAll(std::uint32_t flushTimeoutMs)
    {
        ShutdownReport report;
        closed_ = true;
        if (!listenName_.empty())
            net_->listeners.erase(listenName_);
        listenName_.clear();
        // Answer requests already waiting (refused with kShutdown) and drop
        // peers that closed, as the GNS backend does.
        {
            std::vector<Event> discarded;
            handleNotices(discarded);
        }
        // Virtual time: the transport drives the network while it waits.
        std::uint32_t waited = 0;
        for (; waited < flushTimeoutMs; ++waited)
        {
            std::uint64_t left = 0;
            for (auto& kv : conns_)
                if (kv.second.state == ConnectionState::Connected)
                    left += reliableBacklog(kv.second);
            if (left == 0)
                break;
            net_->step();
            std::vector<Event> discarded;
            handleNotices(discarded); // peers that close meanwhile; new requests are refused
        }
        for (auto& kv : conns_)
        {
            if (kv.second.state == ConnectionState::Connected)
            {
                const std::uint64_t left = reliableBacklog(kv.second);
                report.unflushed_bytes += left;
                if (left)
                    report.flushed = false;
            }
            closeLink(kv.second, close_code::kShutdown, "shutdown", false);
            ++report.connections_closed;
        }
        conns_.clear();
        pending_.clear();
        notices_.clear();
        report.waited_ms = waited;
        return report;
    }

    std::shared_ptr<FakeNetwork::State> net_;
    int id_;
    detail::OwnerThread owner_;
    TransportConfig config_;
    bool closed_ = false;
    bool crashed_ = false;
    std::string listenName_;
    ConnectionId nextId_ = 1;
    std::map<ConnectionId, Conn> conns_;
    std::vector<Notice> notices_;
    std::vector<Event> pending_;
    std::size_t rotation_ = 0;
    TransportCounters counters_;
    // Bumped by calls from other threads, so atomic; everything else is owner-only.
    mutable std::atomic<std::uint64_t> wrongThreadCalls_{0};

    friend struct coop::net::FakeNetwork::State;
};
} // namespace

void FakeNetwork::State::notify(int endpointId, Notice n)
{
    if (FakeTransport* t = endpoint(endpointId))
    {
        if (t->crashed())
            return;
        if (n.kind == Notice::Incoming && !t->acceptsRequests())
        {
            // A closed listener still answers, like the GNS backend during shutdown.
            if (n.link->endpoint[0])
            {
                const int requester = n.link->endpoint[0];
                notify(requester, {Notice::Closed, n.link, close_code::kShutdown, "shutting down"});
            }
            return;
        }
        t->addNotice(std::move(n));
    }
}

void FakeNetwork::State::transmit(Link& l, int s)
{
    FakeTransport* sender = endpoint(l.endpoint[s]);
    if (!sender || sender->crashed())
        return;
    Direction& d = l.dir[s];
    if (d.queued.empty() || (!l.open[s] && !d.linger))
        return;
    const auto& channels = sender->config().channels;
    const double rate = sender->config().send_rate_bytes_per_s;
    d.tokens = std::min(d.tokens + rate / 1000.0, rate / 50.0); // 20 ms burst
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    while (d.tokens > 0)
    {
        // Lowest priority value first; round-robin among equals.
        int best = -1;
        const std::size_t n = d.queued.size();
        for (std::size_t k = 0; k < n; ++k)
        {
            const std::size_t i = (d.rr + k) % n;
            if (d.queued[i].empty())
                continue;
            if (best < 0 || channels[i].priority < channels[std::size_t(best)].priority)
                best = int(i);
        }
        if (best < 0)
            break;
        d.rr = std::size_t(best) + 1;
        Msg m = std::move(d.queued[std::size_t(best)].front());
        d.queued[std::size_t(best)].pop_front();
        const std::uint32_t size = std::uint32_t(m.bytes.size());
        d.queuedBytes[std::size_t(best)] -= size;
        d.tokens -= size;
        if (!m.reliable && unit(rng) < link.transient_loss)
            continue;
        if (m.reliable)
            d.unacked[std::size_t(best)] += size;
        m.at_us = now + latencyUs();
        d.flight.push_back(std::move(m));
    }
}

void FakeNetwork::State::deliver(Link& l, int s)
{
    Direction& d = l.dir[s];
    const int r = 1 - s;
    FakeTransport* receiver = endpoint(l.endpoint[r]);
    std::deque<Msg> held;
    bool blocked[kMaxChannels] = {};
    while (!d.flight.empty() && d.flight.front().at_us <= now)
    {
        Msg m = std::move(d.flight.front());
        d.flight.pop_front();
        if (m.reliable && blocked[m.lane % kMaxChannels])
        {
            held.push_back(std::move(m));
            continue;
        }
        const std::uint8_t lane = m.lane;
        const bool reliable = m.reliable;
        const std::uint32_t size = std::uint32_t(m.bytes.size());
        const auto result = receiver && l.open[r] ? receiver->deliver(l.id[r], m) : FakeTransport::Accept::Vanished;
        if (result == FakeTransport::Accept::Accepted)
        {
            if (reliable)
                d.acks.push_back({now + latencyUs(), lane, size});
        }
        else if (result == FakeTransport::Accept::Full && reliable)
        {
            blocked[lane % kMaxChannels] = true; // not acknowledged: retried, holds its channel
            held.push_back(std::move(m));
        }
        // Unreliable that did not fit, or anything for a gone/crashed peer, is lost.
    }
    while (!held.empty())
    {
        d.flight.push_front(std::move(held.back()));
        held.pop_back();
    }
    while (!d.acks.empty() && d.acks.front().at_us <= now)
    {
        d.unacked[d.acks.front().lane] -= d.acks.front().bytes;
        d.acks.pop_front();
    }
    // A lingering close goes once everything queued has left.
    if (d.linger && !d.closeSent && d.empty())
    {
        d.closeSent = true;
        const int peer = l.endpoint[r];
        auto self = std::shared_ptr<Link>();
        for (auto& w : links)
            if (auto p = w.lock(); p && p.get() == &l)
                self = p;
        if (peer && self)
        {
            const int code = d.closeCode;
            const std::string why = d.closeDetail;
            FakeNetwork::State* net = this;
            at(now + latencyUs(), [net, self, peer, code, why] { net->notify(peer, {Notice::Closed, self, code, why}); });
        }
    }
}

void FakeNetwork::State::step()
{
    now += 1000;
    while (!timed.empty() && timed.top().at <= now)
    {
        auto fn = timed.top().fn;
        timed.pop();
        fn();
    }
    for (std::size_t i = 0; i < links.size();)
    {
        std::shared_ptr<Link> l = links[i].lock();
        if (!l)
        {
            links[i] = links.back();
            links.pop_back();
            continue;
        }
        for (int s = 0; s < 2; ++s)
        {
            transmit(*l, s);
            deliver(*l, s);
        }
        // A crashed side: the other side times out after its own timeout.
        if (l->silentSince >= 0)
            for (int s = 0; s < 2; ++s)
            {
                FakeTransport* t = endpoint(l->endpoint[s]);
                if (!t || t->crashed() || !l->open[s] || l->timeoutSent[s])
                    continue;
                if (now - l->silentSince >= std::int64_t(t->config().connected_timeout_ms) * 1000)
                {
                    l->timeoutSent[s] = true;
                    t->addNotice({Notice::TimedOut, l, 0, "timed out"});
                }
            }
        ++i;
    }
}

FakeNetwork::FakeNetwork(const FakeLinkConfig& link) : state_(std::make_shared<State>(link)) {}

FakeNetwork::~FakeNetwork() = default;

std::unique_ptr<Transport> FakeNetwork::create(const TransportConfig& config, std::string* error)
{
    const std::string problem = validate(config);
    if (!problem.empty())
    {
        if (error)
            *error = problem;
        return nullptr;
    }
    const int id = state_->nextEndpoint++;
    auto t = std::make_unique<FakeTransport>(state_, id, config);
    state_->endpoints[id] = t.get();
    return t;
}

void FakeNetwork::advance(std::uint32_t ms)
{
    for (std::uint32_t i = 0; i < ms; ++i)
        state_->step();
}

std::int64_t FakeNetwork::now_us() const { return state_->now; }

void FakeNetwork::set_link(const FakeLinkConfig& link)
{
    state_->link.latency_ms = link.latency_ms;
    state_->link.transient_loss = link.transient_loss;
}

void FakeNetwork::crash(Transport& transport)
{
    for (auto& kv : state_->endpoints)
        if (kv.second == &transport)
            kv.second->crash();
}

} // namespace coop::net
