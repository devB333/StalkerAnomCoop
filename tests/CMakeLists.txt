# Project tests (S04-T02 onward). Configured by tools/Build-Protocol.ps1.
cmake_minimum_required(VERSION 3.15)
project(coop_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

add_subdirectory(../protocol protocol)

# Game-side code compiled against coop_net alone: proves the public headers
# need no transport library. It has no GNS include path; if transport.h or
# fake_network.h pulled in a library header, this target would not compile.
add_library(coop_net_isolation STATIC transport/isolation_check.cpp)
target_link_libraries(coop_net_isolation PRIVATE coop_net)
target_compile_options(coop_net_isolation PRIVATE /W4 /WX /permissive- /EHsc)

add_executable(transport-tests
    harness/check.cpp
    transport/rig.cpp
    transport/test_policy.cpp
    transport/test_scenarios.cpp
    transport/test_races.cpp)
target_include_directories(transport-tests PRIVATE harness transport ../protocol/src/net)
target_compile_options(transport-tests PRIVATE /W4 /WX /permissive- /EHsc)
target_compile_definitions(transport-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(transport-tests PRIVATE coop_net_gns coop_net_isolation winmm)

add_executable(protocol-tests
    harness/check.cpp
    transport/rig.cpp
    protocol/fixtures.cpp
    protocol/test_codec.cpp
    protocol/test_guard.cpp
    protocol/test_input_queue.cpp
    protocol/test_snapshot.cpp
    protocol/test_prediction.cpp
    protocol/test_fuzz.cpp
    protocol/test_wire.cpp
    session/test_crypto.cpp
    session/test_session.cpp
    session/test_liveness.cpp
    store/test_store.cpp)
target_include_directories(protocol-tests PRIVATE harness transport protocol session)
target_compile_options(protocol-tests PRIVATE /W4 /WX /permissive- /EHsc)
target_compile_definitions(protocol-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(protocol-tests PRIVATE coop_session coop_store coop_protocol coop_net_gns winmm)

# Run next to the library they were built against.
add_custom_command(TARGET transport-tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GameNetworkingSockets::shared> $<TARGET_FILE_DIR:transport-tests>)
