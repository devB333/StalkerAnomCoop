# Co-op protocol libraries (S04-T02). Added by tests/CMakeLists.txt and built
# by tools/Build-Protocol.ps1 with the Stage 1 pinned compiler, /MD, C++17,
# matching the engine's projects (v143, MultiThreadedDLL, stdcpp17).
#
# coop_net      transport interface, shared policy and the in-memory fake.
#               No transport library on its include or link path.
# coop_protocol protocol v1 messages, codec and stateful guard (S04-T03);
#               uses coop_net only for the channel layout types.
# coop_net_gns  the GameNetworkingSockets backend (src/net/gns_transport.cpp
#               is the only file that includes the library).

find_package(GameNetworkingSockets CONFIG REQUIRED)

set(COOP_WARNINGS /W4 /WX /permissive- /EHsc)

add_library(coop_net STATIC
    src/net/policy.cpp
    src/net/fake_network.cpp)
target_include_directories(coop_net PUBLIC include PRIVATE src/net)
target_compile_features(coop_net PUBLIC cxx_std_17)
target_compile_options(coop_net PRIVATE ${COOP_WARNINGS})

add_library(coop_net_gns STATIC
    src/net/gns_transport.cpp)
target_include_directories(coop_net_gns PRIVATE src/net)
target_compile_options(coop_net_gns PRIVATE ${COOP_WARNINGS})
target_compile_definitions(coop_net_gns PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(coop_net_gns PUBLIC coop_net PRIVATE GameNetworkingSockets::shared)

add_library(coop_protocol STATIC
    src/proto/messages.cpp
    src/proto/guard.cpp)
target_include_directories(coop_protocol PUBLIC include)
target_compile_options(coop_protocol PRIVATE ${COOP_WARNINGS})
target_link_libraries(coop_protocol PUBLIC coop_net)
