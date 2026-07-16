add_executable(wiretone_sender src/main.cpp)

target_link_libraries(
    wiretone_sender
    PRIVATE
        WireTone::Core
        WireTone::Capture
)

target_compile_features(wiretone_sender PRIVATE cxx_std_20)

if(WIN32)
    target_sources(
        wiretone_sender
        PRIVATE
            src/wasapi_loopback_capture.cpp
            include/wiretone/windows/wasapi_loopback_capture.hpp
    )

    target_include_directories(
        wiretone_sender
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(
        wiretone_sender
        PRIVATE
            ole32
            propsys
            ksuser
            uuid
    )
endif()

wiretone_enable_warnings(wiretone_sender)
