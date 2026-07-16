#include "wiretone/protocol/packet.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using wiretone::protocol::PacketHeader;
using wiretone::protocol::PacketType;
using wiretone::protocol::ProtocolError;

int failures = 0;

void fail(std::string_view message) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void expect(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

PacketHeader make_audio_header() {
    PacketHeader header{};
    header.type = PacketType::audio;
    header.flags = wiretone::protocol::flag_discontinuity;
    header.stream_id = 0x01020304U;
    header.sequence_number = 0x11223344U;
    header.timestamp_samples = 0x0102030405060708ULL;
    header.payload_size = 4U;
    header.fragment_index = 1U;
    header.fragment_count = 3U;
    header.frame_id = 0xA1B2C3D4U;
    return header;
}

std::vector<std::byte> make_audio_datagram() {
    const auto header = make_audio_header();
    std::vector<std::byte> datagram(
        wiretone::protocol::packet_header_size + header.payload_size,
        std::byte{0});

    const auto encoded = wiretone::protocol::encode_header(header, datagram);
    expect(encoded == ProtocolError::none, "valid audio header did not encode");

    const std::array<std::byte, 4> payload{
        std::byte{0xDE},
        std::byte{0xAD},
        std::byte{0xBE},
        std::byte{0xEF},
    };
    std::copy(
        payload.begin(),
        payload.end(),
        datagram.begin() + static_cast<std::ptrdiff_t>(wiretone::protocol::packet_header_size));

    return datagram;
}

void expect_parse_error(
    std::span<const std::byte> datagram,
    ProtocolError expected,
    std::string_view message) {
    const auto parsed = wiretone::protocol::parse_datagram(datagram);
    if (parsed.error != expected) {
        std::cerr << "FAIL: " << message << " (expected "
                  << wiretone::protocol::to_string(expected) << ", received "
                  << wiretone::protocol::to_string(parsed.error) << ")\n";
        ++failures;
    }
}

void test_exact_encoding() {
    const auto header = make_audio_header();
    std::array<std::byte, wiretone::protocol::packet_header_size> encoded{};

    expect(
        wiretone::protocol::encode_header(header, encoded) == ProtocolError::none,
        "exact-encoding header failed validation");

    const std::array<std::uint8_t, wiretone::protocol::packet_header_size> expected{
        0x57, 0x54, 0x50, 0x4B,
        0x01, 0x03, 0x00, 0x01,
        0x01, 0x02, 0x03, 0x04,
        0x11, 0x22, 0x33, 0x44,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x00, 0x04, 0x01, 0x03,
        0xA1, 0xB2, 0xC3, 0xD4,
    };

    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (std::to_integer<std::uint8_t>(encoded[index]) != expected[index]) {
            fail("header bytes do not match the locked network-order layout");
            return;
        }
    }
}

void test_round_trip() {
    const auto datagram = make_audio_datagram();
    const auto parsed = wiretone::protocol::parse_datagram(datagram);

    expect(parsed.ok(), "valid datagram did not parse");
    if (!parsed.ok()) {
        return;
    }

    expect(parsed.header.version == wiretone::protocol::protocol_version, "version changed");
    expect(parsed.header.type == PacketType::audio, "packet type changed");
    expect(parsed.header.flags == wiretone::protocol::flag_discontinuity, "flags changed");
    expect(parsed.header.stream_id == 0x01020304U, "stream id changed");
    expect(parsed.header.sequence_number == 0x11223344U, "sequence changed");
    expect(parsed.header.timestamp_samples == 0x0102030405060708ULL, "timestamp changed");
    expect(parsed.header.fragment_index == 1U, "fragment index changed");
    expect(parsed.header.fragment_count == 3U, "fragment count changed");
    expect(parsed.header.frame_id == 0xA1B2C3D4U, "frame id changed");
    expect(parsed.payload.size() == 4U, "payload size changed");
    expect(std::to_integer<std::uint8_t>(parsed.payload.front()) == 0xDEU, "payload changed");
}

void test_rejections() {
    const auto valid = make_audio_datagram();

    expect_parse_error(
        std::span<const std::byte>(valid).first(wiretone::protocol::packet_header_size - 1U),
        ProtocolError::datagram_too_small,
        "short datagram was accepted");

    std::vector<std::byte> oversized(wiretone::protocol::maximum_datagram_size + 1U);
    expect_parse_error(oversized, ProtocolError::datagram_too_large, "oversized datagram accepted");

    auto invalid_magic = valid;
    invalid_magic[0] = std::byte{'X'};
    expect_parse_error(invalid_magic, ProtocolError::invalid_magic, "invalid magic accepted");

    auto unsupported_version = valid;
    unsupported_version[4] = std::byte{2};
    expect_parse_error(
        unsupported_version,
        ProtocolError::unsupported_version,
        "unsupported version accepted");

    auto unknown_type = valid;
    unknown_type[5] = std::byte{99};
    expect_parse_error(unknown_type, ProtocolError::unknown_packet_type, "unknown type accepted");

    auto unknown_flags = valid;
    unknown_flags[6] = std::byte{0x80};
    expect_parse_error(unknown_flags, ProtocolError::unknown_flags, "unknown flags accepted");

    auto zero_stream = valid;
    std::fill(zero_stream.begin() + 8, zero_stream.begin() + 12, std::byte{0});
    expect_parse_error(zero_stream, ProtocolError::zero_stream_id, "zero stream id accepted");

    auto zero_fragment_count = valid;
    zero_fragment_count[27] = std::byte{0};
    expect_parse_error(
        zero_fragment_count,
        ProtocolError::invalid_fragment_count,
        "zero fragment count accepted");

    auto invalid_fragment_index = valid;
    invalid_fragment_index[26] = std::byte{3};
    expect_parse_error(
        invalid_fragment_index,
        ProtocolError::invalid_fragment_index,
        "out-of-range fragment index accepted");

    auto mismatched_payload = valid;
    mismatched_payload.pop_back();
    expect_parse_error(
        mismatched_payload,
        ProtocolError::payload_length_mismatch,
        "payload length mismatch accepted");

    PacketHeader heartbeat{};
    heartbeat.type = PacketType::heartbeat;
    heartbeat.stream_id = 1U;
    heartbeat.payload_size = 8U;
    heartbeat.fragment_count = 2U;
    std::array<std::byte, wiretone::protocol::packet_header_size> heartbeat_bytes{};
    expect(
        wiretone::protocol::encode_header(heartbeat, heartbeat_bytes) ==
            ProtocolError::fragmented_control_packet,
        "fragmented control header encoded");

    heartbeat.fragment_count = 1U;
    heartbeat.frame_id = 1U;
    expect(
        wiretone::protocol::encode_header(heartbeat, heartbeat_bytes) ==
            ProtocolError::control_packet_has_frame_id,
        "control frame id encoded");

    PacketHeader stream_start{};
    stream_start.type = PacketType::stream_start;
    stream_start.stream_id = 1U;
    stream_start.payload_size = 15U;
    expect(
        wiretone::protocol::validate_header(stream_start) == ProtocolError::invalid_payload_size,
        "wrong stream-start payload size accepted");

    PacketHeader zero_frame_audio{};
    zero_frame_audio.type = PacketType::audio;
    zero_frame_audio.stream_id = 1U;
    zero_frame_audio.payload_size = 1U;
    expect(
        wiretone::protocol::validate_header(zero_frame_audio) ==
            ProtocolError::audio_packet_has_zero_frame_id,
        "zero audio frame id accepted");

    PacketHeader empty_audio{};
    empty_audio.type = PacketType::audio;
    empty_audio.stream_id = 1U;
    empty_audio.frame_id = 1U;
    empty_audio.payload_size = 0U;
    expect(
        wiretone::protocol::validate_header(empty_audio) == ProtocolError::audio_payload_missing,
        "empty non-silence audio payload accepted");

    empty_audio.flags = wiretone::protocol::flag_silence;
    expect(
        wiretone::protocol::validate_header(empty_audio) == ProtocolError::none,
        "explicit silence audio header rejected");

    std::array<std::byte, wiretone::protocol::packet_header_size - 1U> short_output{};
    expect(
        wiretone::protocol::encode_header(make_audio_header(), short_output) ==
            ProtocolError::output_too_small,
        "short encode buffer accepted");
}

} // namespace

int main() {
    test_exact_encoding();
    test_round_trip();
    test_rejections();

    if (failures != 0) {
        std::cerr << failures << " protocol test(s) failed\n";
        return 1;
    }

    std::cout << "PASS: WireTone protocol header and rejection tests\n";
    return 0;
}
