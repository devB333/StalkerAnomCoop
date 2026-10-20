# S04-T01 transport probe. Built by tools/Build-NetworkProbe.ps1 against the
# pinned GameNetworkingSockets install; see tools/network-probe/pins.json.
cmake_minimum_required(VERSION 3.15)
project(gns_probe CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

find_package(GameNetworkingSockets CONFIG REQUIRED)

add_executable(gns-probe probe.cpp)
target_compile_options(gns-probe PRIVATE /W4 /permissive- /EHsc)
target_compile_definitions(gns-probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(gns-probe PRIVATE GameNetworkingSockets::shared winmm)

# Run next to the library it was built against.
add_custom_command(TARGET gns-probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GameNetworkingSockets::shared> $<TARGET_FILE_DIR:gns-probe>)
