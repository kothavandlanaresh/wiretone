cmake_minimum_required(VERSION 3.22.1)

project(
    WireTone
    VERSION 0.1.0
    DESCRIPTION "Private, local-first Windows-to-Android audio relay"
    LANGUAGES CXX
)

option(WIRETONE_BUILD_TESTS "Build WireTone native tests" ON)
option(WIRETONE_BUILD_WINDOWS_SENDER "Build the Windows sender shell" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(WireToneWarnings)

add_subdirectory(native/core)
add_subdirectory(native/protocol)
add_subdirectory(native/capture)
add_subdirectory(native/playback)

if(WIRETONE_BUILD_WINDOWS_SENDER)
    add_subdirectory(apps/windows-sender)
endif()

if(WIRETONE_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
