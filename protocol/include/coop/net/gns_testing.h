// Test-only controls of the GameNetworkingSockets backend. Not for game code.
#pragma once

namespace coop::net
{
// The library's simulated impairments, applied to what each process sends.
// Process-global in the library (FakePacket*_Send, steamnetworkingtypes.h
// :1415-1481), held in static globals that survive the library's Kill/Init
// (csteamnetworkingsockets.cpp:43-48), so a test must reset them itself. Needs
// a live GNS transport; false otherwise.
struct GnsImpairment
{
    float loss_pct = 0.f;
    float reorder_pct = 0.f;
    int reorder_ms = 0;
    float dup_pct = 0.f;
    int dup_ms = 0;
    int lag_ms = 0;
};
bool set_gns_impairment(const GnsImpairment& impairment);

// Live GNS transports in this process (the library is up while this is > 0).
int gns_library_users();
} // namespace coop::net
