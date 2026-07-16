add_library(wiretone_capture STATIC)

add_library(WireTone::Capture ALIAS wiretone_capture)

target_sources(
    wiretone_capture
    PRIVATE
        src/lifecycle.cpp
        include/wiretone/capture/lifecycle.hpp
)

target_include_directories(
    wiretone_capture
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(wiretone_capture PUBLIC cxx_std_20)
wiretone_enable_warnings(wiretone_capture)
