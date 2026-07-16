#include "wiretone/protocol/control_payload.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using wiretone::protocol::Codec;
using wiretone::protocol::ControlPayloadError;
using wiretone::protocol::ErrorPayloadView;
using wiretone::protocol::HeartbeatPayload;
using wiretone::protocol::ReceiverReportPayload;
using wiretone::protocol::StreamStartPayload;
using wiretone::protocol::StreamStopPayload;

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

template <typename Result>
void expect_error(
    const Result& result,
    ControlPayloadError expected,
    std::string_view message) {
    if (result.error != expected) {
        std::cerr << "FAIL: " << message << " (expected "
                  << wiretone::protocol::to_string(expected) << ", received "
                  << wiretone::protocol::to_string(result.error) << ")\n";
        ++failures;
    }
}

void test_stream_start_exact_bytes_and_round_trip() {
    StreamStartPayload input{};
    input.codec = Codec::opus;
    input.channels = 2;
    input.frame_duration_ms = 20;
    input.sample_rate = 48'000;
    input.target_bitrate = 144'000;
    input.pre_skip_samples = 312;

    std::array<std::byte, wiretone::protocol::stream_start_payload_size> encoded{};
    expect(
        wiretone::protocol::encode_stream_start(input, encoded) ==
            ControlPayloadError::none,
        "valid stream-start payload did not encode");

    const std::array<std::uint8_t, wiretone::protocol::stream_start_payload_size> expected{
        0x02, 0x02, 0x00, 0x14,
        0x00, 0x00, 0xBB, 0x80,
        0x00, 0x02, 0x32, 0x80,
        0x01, 0x38, 0x00, 0x00,
    };

    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (std::to_integer<std::uint8_t>(encoded[index]) != expected[index]) {
            fail("stream-start bytes do not match the locked layout");
            break;
        }
    }

    const auto parsed = wiretone::protocol::parse_stream_start(encoded);
    expect(parsed.ok(), "valid stream-start payload did not parse");
    if (!parsed.ok()) {
        return;
    }

    expect(parsed.value.codec == Codec::opus, "stream-start codec changed");
    expect(parsed.value.channels == 2U, "stream-start channel count changed");
    expect(parsed.value.frame_duration_ms == 20U, "stream-start duration changed");
    expect(parsed.value.sample_rate == 48'000U, "stream-start sample rate changed");
    expect(parsed.value.target_bitrate == 144'000U, "stream-start bitrate changed");
    expect(parsed.value.pre_skip_samples == 312U, "stream-start pre-skip changed");
}

void test_stream_start_rejections() {
    StreamStartPayload value{};

    value.codec = static_cast<Codec>(99);
    expect(
        wiretone::protocol::validate_stream_start(value) ==
            ControlPayloadError::unsupported_codec,
        "unknown codec accepted");

    value = {};
    value.channels = 1;
    expect(
        wiretone::protocol::validate_stream_start(value) ==
            ControlPayloadError::unsupported_channel_count,
        "unsupported channel count accepted");

    value = {};
    value.frame_duration_ms = 10;
    expect(
        wiretone::protocol::validate_stream_start(value) ==
            ControlPayloadError::unsupported_frame_duration,
        "unsupported frame duration accepted");

    value = {};
    value.sample_rate = 44'100;
    expect(
        wiretone::protocol::validate_stream_start(value) ==
            ControlPayloadError::unsupported_sample_rate,
        "unsupported sample rate accepted");

    value = {};
    value.target_bitrate = 1;
    expect(
        wiretone::protocol::validate_stream_start(value) ==
            ControlPayloadError::invalid_pcm_target_bitrate,
        "PCM target bitrate accepted");

    value = {};
    value.pre_skip_samples = 1;
    expect(
        wiretone::protocol::validate_stream_start(value) ==
            ControlPayloadError::invalid_pcm_pre_skip,
        "PCM pre-skip accepted");

    value = {};
    value.codec = Codec::opus;
    value.target_bitrate = 0;
    expect(
        wiretone::protocol::validate_stream_start(value) ==
            ControlPayloadError::invalid_opus_target_bitrate,
        "zero Opus target bitrate accepted");

    std::array<std::byte, wiretone::protocol::stream_start_payload_size> bytes{};
    value.target_bitrate = 144'000;
    expect(
        wiretone::protocol::encode_stream_start(value, bytes) == ControlPayloadError::none,
        "valid Opus payload did not encode for rejection tests");

    bytes[14] = std::byte{1};
    expect_error(
        wiretone::protocol::parse_stream_start(bytes),
        ControlPayloadError::reserved_field_nonzero,
        "non-zero reserved field accepted");

    expect_error(
        wiretone::protocol::parse_stream_start(
            std::span<const std::byte>{bytes}.first(bytes.size() - 1U)),
        ControlPayloadError::invalid_payload_size,
        "short stream-start payload accepted");

    std::array<std::byte, wiretone::protocol::stream_start_payload_size - 1U> short_output{};
    expect(
        wiretone::protocol::encode_stream_start(value, short_output) ==
            ControlPayloadError::output_too_small,
        "short stream-start output accepted");
}

void test_stream_stop() {
    std::array<std::byte, wiretone::protocol::stream_stop_payload_size> encoded{};
    const StreamStopPayload input{200};
    expect(
        wiretone::protocol::encode_stream_stop(input, encoded) == ControlPayloadError::none,
        "stream-stop payload did not encode");
    expect(std::to_integer<std::uint8_t>(encoded[0]) == 200U, "stream-stop reason changed");

    const auto parsed = wiretone::protocol::parse_stream_stop(encoded);
    expect(parsed.ok(), "stream-stop payload did not parse");
    expect(parsed.value.reason == 200U, "unknown stream-stop reason was not preserved");

    const std::array<std::byte, 0> empty{};
    expect_error(
        wiretone::protocol::parse_stream_stop(empty),
        ControlPayloadError::invalid_payload_size,
        "empty stream-stop payload accepted");
}

void test_heartbeat() {
    const HeartbeatPayload input{0x0102030405060708ULL};
    std::array<std::byte, wiretone::protocol::heartbeat_payload_size> encoded{};
    expect(
        wiretone::protocol::encode_heartbeat(input, encoded) == ControlPayloadError::none,
        "heartbeat payload did not encode");

    const std::array<std::uint8_t, wiretone::protocol::heartbeat_payload_size> expected{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    };
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (std::to_integer<std::uint8_t>(encoded[index]) != expected[index]) {
            fail("heartbeat bytes do not match network order");
            break;
        }
    }

    const auto parsed = wiretone::protocol::parse_heartbeat(encoded);
    expect(parsed.ok(), "heartbeat payload did not parse");
    expect(
        parsed.value.sender_monotonic_time_us == input.sender_monotonic_time_us,
        "heartbeat timestamp changed");
}

void test_receiver_report() {
    const ReceiverReportPayload input{
        0x01020304U,
        0x11121314U,
        0x21222324U,
        0x31323334U,
        0x4142U,
        0x5152U,
    };

    std::array<std::byte, wiretone::protocol::receiver_report_payload_size> encoded{};
    expect(
        wiretone::protocol::encode_receiver_report(input, encoded) ==
            ControlPayloadError::none,
        "receiver report did not encode");

    const std::array<std::uint8_t, wiretone::protocol::receiver_report_payload_size> expected{
        0x01, 0x02, 0x03, 0x04,
        0x11, 0x12, 0x13, 0x14,
        0x21, 0x22, 0x23, 0x24,
        0x31, 0x32, 0x33, 0x34,
        0x41, 0x42, 0x51, 0x52,
    };
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (std::to_integer<std::uint8_t>(encoded[index]) != expected[index]) {
            fail("receiver-report bytes do not match the locked layout");
            break;
        }
    }

    const auto parsed = wiretone::protocol::parse_receiver_report(encoded);
    expect(parsed.ok(), "receiver report did not parse");
    expect(
        parsed.value.highest_sequence_number_received ==
            input.highest_sequence_number_received,
        "highest received sequence changed");
    expect(
        parsed.value.cumulative_missing_datagrams == input.cumulative_missing_datagrams,
        "missing datagram count changed");
    expect(
        parsed.value.cumulative_late_datagrams == input.cumulative_late_datagrams,
        "late datagram count changed");
    expect(
        parsed.value.estimated_network_jitter_us == input.estimated_network_jitter_us,
        "jitter estimate changed");
    expect(parsed.value.buffered_audio_ms == input.buffered_audio_ms, "buffer level changed");
    expect(
        parsed.value.cumulative_playback_underruns ==
            input.cumulative_playback_underruns,
        "underrun count changed");
}

void test_error_payload() {
    constexpr std::string_view message = "bad packet: \xE2\x82\xAC";
    const ErrorPayloadView input{0x1234U, message};
    std::vector<std::byte> encoded(wiretone::protocol::encoded_error_payload_size(message));

    expect(
        wiretone::protocol::encode_error_payload(input, encoded) ==
            ControlPayloadError::none,
        "error payload did not encode");
    expect(std::to_integer<std::uint8_t>(encoded[0]) == 0x12U, "error code high byte changed");
    expect(std::to_integer<std::uint8_t>(encoded[1]) == 0x34U, "error code low byte changed");

    const auto parsed = wiretone::protocol::parse_error_payload(encoded);
    expect(parsed.ok(), "error payload did not parse");
    expect(parsed.value.code == input.code, "error code changed");
    expect(parsed.value.message == message, "error diagnostic message changed");

    std::string oversized(wiretone::protocol::error_message_maximum_size + 1U, 'x');
    expect(
        wiretone::protocol::encode_error_payload(
            ErrorPayloadView{1U, oversized},
            encoded) == ControlPayloadError::error_message_too_large,
        "oversized error message accepted");

    const std::array<std::byte, 4> invalid_utf8{
        std::byte{0x00},
        std::byte{0x01},
        std::byte{0xC0},
        std::byte{0xAF},
    };
    expect_error(
        wiretone::protocol::parse_error_payload(invalid_utf8),
        ControlPayloadError::invalid_utf8,
        "invalid UTF-8 diagnostic accepted");

    const std::array<std::byte, 1> too_short{std::byte{0}};
    expect_error(
        wiretone::protocol::parse_error_payload(too_short),
        ControlPayloadError::invalid_payload_size,
        "short error payload accepted");
}

} // namespace

int main() {
    test_stream_start_exact_bytes_and_round_trip();
    test_stream_start_rejections();
    test_stream_stop();
    test_heartbeat();
    test_receiver_report();
    test_error_payload();

    if (failures != 0) {
        std::cerr << failures << " control payload test(s) failed\n";
        return 1;
    }

    std::cout << "PASS: WireTone typed control payload tests\n";
    return 0;
}
