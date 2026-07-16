cmake_minimum_required(VERSION 3.22.1)

project(WireToneAndroidNative LANGUAGES CXX)

get_filename_component(
    WIRETONE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../.."
    ABSOLUTE
)

list(APPEND CMAKE_MODULE_PATH "${WIRETONE_ROOT}/cmake")
include(WireToneWarnings)

add_subdirectory(
    "${WIRETONE_ROOT}/native/core"
    "${CMAKE_CURRENT_BINARY_DIR}/wiretone-core"
)

add_subdirectory(
    "${WIRETONE_ROOT}/native/protocol"
    "${CMAKE_CURRENT_BINARY_DIR}/wiretone-protocol"
)

add_library(wiretone_native SHARED native_bridge.cpp)

target_link_libraries(
    wiretone_native
    PRIVATE
        WireTone::Core
        WireTone::Protocol
)
target_compile_features(wiretone_native PRIVATE cxx_std_20)

target_compile_options(
    wiretone_native
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
