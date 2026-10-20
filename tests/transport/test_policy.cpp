// Unit tests of the library-independent policy (protocol/src/net/policy.*).
#include "check.h"
#include "policy.h"
#include "rig.h"

#include <thread>

namespace coop::test
{
using namespace coop::net;
using detail::Admission;

int game_side_drain(Transport& t, std::vector<Event>& scratch); // isolation_check.cpp

TEST("policy/validate")
{
    CHECK(validate(TestConfig()).empty());
    TransportConfig c = TestConfig();
    c.channels.clear();
    CHECK(!validate(c).empty());
    c = TestConfig();
    c.channels.resize(kMaxChannels + 1, c.channels[0]);
    CHECK(!validate(c).empty());
    c = TestConfig();
    c.channels[kSnapshot].max_message_bytes = kMaxTransientMessageBytes + 1;
    c.channels[kSnapshot].max_backlog_bytes = 64 * 1024;
    CHECK(!validate(c).empty()); // the library would send it reliably
    c = TestConfig();
    c.channels[kControl].max_backlog_bytes = c.channels[kControl].max_message_bytes - 1;
    CHECK(!validate(c).empty()); // nothing could ever be sent
    c = TestConfig();
    c.send_buffer_bytes = 100 * 1024; // channel backlogs add up to 344 KB
    CHECK(validate(c).find("exceeds send_buffer_bytes") != std::string::npos);
    c = TestConfig();
    c.recv_buffer_bytes = 64 * 1024; // == largest channel message
    CHECK(!validate(c).empty());
    c = TestConfig();
    c.channels[kControl].weight = 0;
    CHECK(!validate(c).empty());
    c = TestConfig();
    c.max_messages_per_poll = 0;
    CHECK(!validate(c).empty());
}

TEST("policy/admission")
{
    const TransportConfig c = TestConfig();
    detail::ChannelEstimate e;

    // Critical: counts queued + unacked + queued since refresh; over bound closes.
    const ChannelSpec& critical = c.channels[kControl]; // 64 KB
    e.last = {10000, 50000, 0};
    CHECK(detail::admit(critical, e, 4096) == Admission::Send);
    e.queued_since_refresh = 2000;
    CHECK(detail::admit(critical, e, 4096) == Admission::Overload); // 66096 > 65536
    e.queued_since_refresh = 0;
    CHECK(detail::admit(critical, e, 5536) == Admission::Send);       // exactly the bound
    CHECK(detail::admit(critical, e, 5537) == Admission::Overload);

    // Bulk: same accounting, refuses instead of closing.
    const ChannelSpec& bulk = c.channels[kBulk]; // 192 KB
    e.last = {150000, 40000, 0};
    CHECK(detail::admit(bulk, e, 1000) == Admission::Send);
    CHECK(detail::admit(bulk, e, 10000) == Admission::Backpressure);

    // Transient: only queued bytes count (unreliable data is never acked); the
    // age rule applies first.
    const ChannelSpec& snap = c.channels[kSnapshot]; // 16 KB, 50 ms
    e.last = {1000, 900000, 10};
    CHECK(detail::admit(snap, e, 1200) == Admission::Send);
    e.last.queue_ms = 51;
    CHECK(detail::admit(snap, e, 100) == Admission::DropStale);
    e.last = {16000, 0, 0};
    CHECK(detail::admit(snap, e, 1200) == Admission::DropBacklog);
    ChannelSpec noAge = snap;
    noAge.max_queue_ms = 0;
    e.last = {0, 0, 5000};
    CHECK(detail::admit(noAge, e, 1200) == Admission::Send);
}

TEST("policy/inbound")
{
    const TransportConfig c = TestConfig();
    using detail::InboundCheck;
    CHECK(detail::check_inbound(c, kControl, true, 4096) == InboundCheck::Ok);
    CHECK(detail::check_inbound(c, kControl, true, 4097) == InboundCheck::TooLarge);
    CHECK(detail::check_inbound(c, kControl, false, 10) == InboundCheck::WrongDelivery);
    CHECK(detail::check_inbound(c, kSnapshot, true, 10) == InboundCheck::WrongDelivery);
    CHECK(detail::check_inbound(c, kSnapshot, false, 1200) == InboundCheck::Ok);
    CHECK(detail::check_inbound(c, 5, true, 10) == InboundCheck::UnknownChannel);
    CHECK(detail::check_inbound(c, 254, true, 10) == InboundCheck::UnknownChannel);
}

TEST("policy/close-codes")
{
    CHECK(detail::reason_from_peer_code(close_code::kServerFull) == CloseReason::Refused);
    CHECK(detail::reason_from_peer_code(close_code::kShutdown) == CloseReason::PeerShutdown);
    CHECK(detail::reason_from_peer_code(close_code::kOverloaded) == CloseReason::PeerClosed);
    CHECK(detail::reason_from_peer_code(1234) == CloseReason::PeerClosed);
    CHECK(detail::valid_close_code(close_code::kNormal));
    CHECK(detail::valid_close_code(close_code::kMax));
    CHECK(!detail::valid_close_code(999));
    CHECK(!detail::valid_close_code(2000));
    CHECK(!detail::valid_close_code(0));
}

TEST("policy/owner-thread")
{
    detail::OwnerThread owner;
    CHECK(owner.here());
    bool other = true;
    std::thread([&] { other = owner.here(); }).join();
    CHECK(!other);
}

// The game-side code in isolation_check.cpp (built without any transport
// library include path) drives a transport through the interface alone.
TEST("policy/game-side-isolation")
{
    FakeNetwork net;
    auto server = net.create(TestConfig());
    auto client = net.create(TestConfig());
    REQUIRE(server && client);
    REQUIRE(server->listen("s"));
    const ConnectionId cid = client->connect("s");
    REQUIRE(cid != kNoConnection);
    std::vector<Event> scratch;
    int messages = 0;
    for (int i = 0; i < 200; ++i)
    {
        net.advance(1);
        game_side_drain(*client, scratch);
        if (i == 50)
        {
            const auto p = Payload(1, 64);
            CHECK(client->send(cid, kControl, p.data(), std::uint32_t(p.size())) == SendResult::Sent);
        }
        messages += game_side_drain(*server, scratch);
    }
    CHECK(messages == 1);
}
} // namespace coop::test
