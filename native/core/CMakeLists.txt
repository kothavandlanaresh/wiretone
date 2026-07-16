add_library(wiretone_core STATIC)

add_library(WireTone::Core ALIAS wiretone_core)

target_sources(
    wiretone_core
    PRIVATE
        src/version.cpp
        include/wiretone/core/version.hpp
)

target_include_directories(
    wiretone_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(wiretone_core PUBLIC cxx_std_20)
wiretone_enable_warnings(wiretone_core)
