// Valid protocol messages of every type, and helpers to build raw frames.
#pragma once

#include "check.h"
#include "coop/proto/guard.h"
#include "coop/proto/messages.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace coop::test
{
using namespace coop::proto;

struct Fixture
{
    const char* name;
    Message message;
    std::uint64_t epoch;    // 0 for handshake messages
    std::uint64_t sequence;
};

Digest MakeDigest(std::uint8_t seed);
Uuid128 MakeUuid(std::uint64_t seed);

// One or more valid fixtures per message type (largest and smallest shapes).
std::vector<Fixture> AllFixtures();

std::vector<std::uint8_t> Encode(const Fixture& f);
Role ReceiverOf(MessageType type);

// A frame with the given header fields and raw body bytes (nothing checked).
std::vector<std::uint8_t> Frame(std::uint16_t type, std::uint64_t epoch, std::uint64_t sequence,
    const std::vector<std::uint8_t>& body, std::uint16_t flags = 0);
std::vector<std::uint8_t> Body(const std::function<void(Writer&)>& write);

// Decodes `bytes` as `type` would arrive (its channel, its receiver).
DecodeResult DecodeAs(MessageType type, const std::vector<std::uint8_t>& bytes, Decoded* out = nullptr);

// Rewrites the header's payload length to match the frame.
void FixLength(std::vector<std::uint8_t>& frame);
} // namespace coop::test
