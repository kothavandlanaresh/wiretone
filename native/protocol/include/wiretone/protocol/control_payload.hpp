#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wiretone::protocol {

inline constexpr std::size_t stream_start_payload_size = 16;
inline constexpr std::size_t stream_stop_payload_size = 1;
inline constexpr std::size_t heartbeat_payload_size = 8;
inline constexpr std::size_t receiver_report_payload_size = 20;
inline constexpr std::size_t error_payload_minimum_size = 2;
inline constexpr std::size_t error_payload_maximum_size = 258;
inline constexpr std::size_t error_message_maximum_size = 256;

inline constexpr std::uint8_t supported_channel_count = 2;
inline constexpr std::uint16_t supported_frame_duration_ms = 20;
inline constexpr std::uint32_t supported_sample_rate = 48'000;

enum class Codec : std::uint8_t {
    pcm_s16le = 1,
    opus = 2,
};

enum class ControlPayloadError {
    none = 0,
    output_too_small,
    invalid_payload_size,
    unsupported_codec,
    unsupported_channel_count,
    unsupported_frame_duration,
    unsupported_sample_rate,
    invalid_pcm_target_bitrate,
    invalid_pcm_pre_skip,
    invalid_opus_target_bitrate,
    reserved_field_nonzero,
    error_message_too_large,
    invalid_utf8,
};

template <typename Value>
struct ControlParseResult {
    ControlPayloadError error{ControlPayloadError::none};
    Value value{};

    [[nodiscard]] bool ok() const noexcept {
        return error == ControlPayloadError::none;
    }
};

struct StreamStartPayload {
    Codec codec{Codec::pcm_s16le};
    std::uint8_t channels{supported_channel_count};
    std::uint16_t frame_duration_ms{supported_frame_duration_ms};
    std::uint32_t sample_rate{supported_sample_rate};
    std::uint32_t target_bitrate{0};
    std::uint16_t pre_skip_samples{0};
};

struct StreamStopPayload {
    std::uint8_t reason{0};
};

struct HeartbeatPayload {
    std::uint64_t sender_monotonic_time_us{0};
};

struct ReceiverReportPayload {
    std::uint32_t highest_sequence_number_received{0};
    std::uint32_t cumulative_missing_datagrams{0};
    std::uint32_t cumulative_late_datagrams{0};
    std::uint32_t estimated_network_jitter_us{0};
    std::uint16_t buffered_audio_ms{0};
    std::uint16_t cumulative_playback_underruns{0};
};

struct ErrorPayloadView {
    std::uint16_t code{0};
    std::string_view message{};
};

[[nodiscard]] ControlPayloadError validate_stream_start(
    const StreamStartPayload& payload) noexcept;

[[nodiscard]] ControlPayloadError encode_stream_start(
    const StreamStartPayload& payload,
    std::span<std::byte> output) noexcept;

[[nodiscard]] ControlParseResult<StreamStartPayload> parse_stream_start(
    std::span<const std::byte> payload) noexcept;

[[nodiscard]] ControlPayloadError encode_stream_stop(
    const StreamStopPayload& payload,
    std::span<std::byte> output) noexcept;

[[nodiscard]] ControlParseResult<StreamStopPayload> parse_stream_stop(
    std::span<const std::byte> payload) noexcept;

[[nodiscard]] ControlPayloadError encode_heartbeat(
    const HeartbeatPayload& payload,
    std::span<std::byte> output) noexcept;

[[nodiscard]] ControlParseResult<HeartbeatPayload> parse_heartbeat(
    std::span<const std::byte> payload) noexcept;

[[nodiscard]] ControlPayloadError encode_receiver_report(
    const ReceiverReportPayload& payload,
    std::span<std::byte> output) noexcept;

[[nodiscard]] ControlParseResult<ReceiverReportPayload> parse_receiver_report(
    std::span<const std::byte> payload) noexcept;

[[nodiscard]] constexpr std::size_t encoded_error_payload_size(
    std::string_view message) noexcept {
    return error_payload_minimum_size + message.size();
}

[[nodiscard]] ControlPayloadError encode_error_payload(
    const ErrorPayloadView& payload,
    std::span<std::byte> output) noexcept;

[[nodiscard]] ControlParseResult<ErrorPayloadView> parse_error_payload(
    std::span<const std::byte> payload) noexcept;

[[nodiscard]] std::string_view to_string(ControlPayloadError error) noexcept;

} // namespace wiretone::protocol
