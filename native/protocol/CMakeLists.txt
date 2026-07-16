add_library(wiretone_protocol STATIC)

add_library(WireTone::Protocol ALIAS wiretone_protocol)

target_sources(
    wiretone_protocol
    PRIVATE
        src/byte_order.hpp
        src/control_payload.cpp
        src/packet.cpp
        include/wiretone/protocol/control_payload.hpp
        include/wiretone/protocol/packet.hpp
)

target_include_directories(
    wiretone_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(wiretone_protocol PUBLIC cxx_std_20)
wiretone_enable_warnings(wiretone_protocol)
