# Co-op protocol libraries (S04-T02). Added by tests/CMakeLists.txt and built
# by tools/Build-Protocol.ps1 with the Stage 1 pinned compiler, /MD, C++17,
# matching the engine's projects (v143, MultiThreadedDLL, stdcpp17).
#
# coop_net      transport interface, shared policy and the in-memory fake.
#               No transport library on its include or link path.
# coop_protocol protocol v1 messages, codec and stateful guard (S04-T03);
#               uses coop_net only for the channel layout types.
# coop_session sessions bound to authenticated players (S04-T04); Windows
#               BCrypt and DPAPI for credentials.
# coop-keytool  makes a player's key file and roster line.
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

add_library(coop_session STATIC
    src/session/crypto_win.cpp
    src/session/auth.cpp
    src/session/server.cpp
    src/session/client.cpp)
target_include_directories(coop_session PUBLIC include)
target_compile_options(coop_session PRIVATE ${COOP_WARNINGS})
target_compile_definitions(coop_session PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(coop_session PUBLIC coop_protocol PRIVATE bcrypt crypt32)

add_executable(coop-keytool tools/coop_keytool.cpp)
target_compile_options(coop-keytool PRIVATE ${COOP_WARNINGS})
target_link_libraries(coop-keytool PRIVATE coop_session)
