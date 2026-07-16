add_library(wiretone_playback STATIC)

add_library(WireTone::Playback ALIAS wiretone_playback)

target_sources(
    wiretone_playback
    PRIVATE
        src/lifecycle.cpp
        src/pcm_queue.cpp
        include/wiretone/playback/lifecycle.hpp
        include/wiretone/playback/pcm_queue.hpp
)

target_include_directories(
    wiretone_playback
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(wiretone_playback PUBLIC cxx_std_20)
wiretone_enable_warnings(wiretone_playback)
