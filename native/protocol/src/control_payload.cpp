#include "wiretone/protocol/control_payload.hpp"

#include "byte_order.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace wiretone::protocol {
namespace {

constexpr std::size_t stream_start_codec_offset = 0;
constexpr std::size_t stream_start_channels_offset = 1;
constexpr std::size_t stream_start_frame_duration_offset = 2;
constexpr std::size_t stream_start_sample_rate_offset = 4;
constexpr std::size_t stream_start_target_bitrate_offset = 8;
constexpr std::size_t stream_start_pre_skip_offset = 12;
constexpr std::size_t stream_start_reserved_offset = 14;

constexpr std::size_t receiver_report_highest_sequence_offset = 0;
constexpr std::size_t receiver_report_missing_offset = 4;
constexpr std::size_t receiver_report_late_offset = 8;
constexpr std::size_t receiver_report_jitter_offset = 12;
constexpr std::size_t receiver_report_buffered_offset = 16;
constexpr std::size_t receiver_report_underruns_offset = 18;

[[nodiscard]] constexpr bool is_known_codec(std::uint8_t value) noexcept {
    return value == static_cast<std::uint8_t>(Codec::pcm_s16le) ||
           value == static_cast<std::uint8_t>(Codec::opus);
}

[[nodiscard]] bool valid_utf8(std::string_view text) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    std::size_t index = 0;

    while (index < text.size()) {
        const auto first = bytes[index];
        if (first <= 0x7FU) {
            ++index;
            continue;
        }

        std::size_t continuation_count = 0;
        std::uint32_t code_point = 0;
        std::uint32_t minimum_code_point = 0;

        if ((first & 0xE0U) == 0xC0U) {
            continuation_count = 1;
            code_point = first & 0x1FU;
            minimum_code_point = 0x80U;
        } else if ((first & 0xF0U) == 0xE0U) {
            continuation_count = 2;
            code_point = first & 0x0FU;
            minimum_code_point = 0x800U;
        } else if ((first & 0xF8U) == 0xF0U) {
            continuation_count = 3;
            code_point = first & 0x07U;
            minimum_code_point = 0x10000U;
        } else {
            return false;
        }

        if (index + continuation_count >= text.size()) {
            return false;
        }

        for (std::size_t continuation = 1; continuation <= continuation_count; ++continuation) {
            const auto next = bytes[index + continuation];
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            code_point = static_cast<std::uint32_t>((code_point << 6U) | (next & 0x3FU));
        }

        if (code_point < minimum_code_point ||
            code_point > 0x10FFFFU ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
            return false;
        }

        index += continuation_count + 1U;
    }

    return true;
}

} // namespace

ControlPayloadError validate_stream_start(const StreamStartPayload& payload) noexcept {
    const auto codec_value = static_cast<std::uint8_t>(payload.codec);
    if (!is_known_codec(codec_value)) {
        return ControlPayloadError::unsupported_codec;
    }

    if (payload.channels != supported_channel_count) {
        return ControlPayloadError::unsupported_channel_count;
    }

    if (payload.frame_duration_ms != supported_frame_duration_ms) {
        return ControlPayloadError::unsupported_frame_duration;
    }

    if (payload.sample_rate != supported_sample_rate) {
        return ControlPayloadError::unsupported_sample_rate;
    }

    if (payload.codec == Codec::pcm_s16le) {
        if (payload.target_bitrate != 0U) {
            return ControlPayloadError::invalid_pcm_target_bitrate;
        }
        if (payload.pre_skip_samples != 0U) {
            return ControlPayloadError::invalid_pcm_pre_skip;
        }
    } else if (payload.target_bitrate == 0U) {
        return ControlPayloadError::invalid_opus_target_bitrate;
    }

    return ControlPayloadError::none;
}

ControlPayloadError encode_stream_start(
    const StreamStartPayload& payload,
    std::span<std::byte> output) noexcept {
    if (output.size() < stream_start_payload_size) {
        return ControlPayloadError::output_too_small;
    }

    const auto validation = validate_stream_start(payload);
    if (validation != ControlPayloadError::none) {
        return validation;
    }

    std::fill_n(output.begin(), stream_start_payload_size, std::byte{0});
    output[stream_start_codec_offset] = static_cast<std::byte>(payload.codec);
    output[stream_start_channels_offset] = static_cast<std::byte>(payload.channels);
    detail::write_u16(output, stream_start_frame_duration_offset, payload.frame_duration_ms);
    detail::write_u32(output, stream_start_sample_rate_offset, payload.sample_rate);
    detail::write_u32(output, stream_start_target_bitrate_offset, payload.target_bitrate);
    detail::write_u16(output, stream_start_pre_skip_offset, payload.pre_skip_samples);
    detail::write_u16(output, stream_start_reserved_offset, 0U);
    return ControlPayloadError::none;
}

ControlParseResult<StreamStartPayload> parse_stream_start(
    std::span<const std::byte> payload) noexcept {
    ControlParseResult<StreamStartPayload> result{};
    if (payload.size() != stream_start_payload_size) {
        result.error = ControlPayloadError::invalid_payload_size;
        return result;
    }

    const auto codec_value = std::to_integer<std::uint8_t>(payload[stream_start_codec_offset]);
    if (!is_known_codec(codec_value)) {
        result.error = ControlPayloadError::unsupported_codec;
        return result;
    }

    if (detail::read_u16(payload, stream_start_reserved_offset) != 0U) {
        result.error = ControlPayloadError::reserved_field_nonzero;
        return result;
    }

    result.value.codec = static_cast<Codec>(codec_value);
    result.value.channels = std::to_integer<std::uint8_t>(payload[stream_start_channels_offset]);
    result.value.frame_duration_ms =
        detail::read_u16(payload, stream_start_frame_duration_offset);
    result.value.sample_rate = detail::read_u32(payload, stream_start_sample_rate_offset);
    result.value.target_bitrate =
        detail::read_u32(payload, stream_start_target_bitrate_offset);
    result.value.pre_skip_samples = detail::read_u16(payload, stream_start_pre_skip_offset);
    result.error = validate_stream_start(result.value);
    return result;
}

ControlPayloadError encode_stream_stop(
    const StreamStopPayload& payload,
    std::span<std::byte> output) noexcept {
    if (output.size() < stream_stop_payload_size) {
        return ControlPayloadError::output_too_small;
    }
    output[0] = static_cast<std::byte>(payload.reason);
    return ControlPayloadError::none;
}

ControlParseResult<StreamStopPayload> parse_stream_stop(
    std::span<const std::byte> payload) noexcept {
    ControlParseResult<StreamStopPayload> result{};
    if (payload.size() != stream_stop_payload_size) {
        result.error = ControlPayloadError::invalid_payload_size;
        return result;
    }
    result.value.reason = std::to_integer<std::uint8_t>(payload[0]);
    return result;
}

ControlPayloadError encode_heartbeat(
    const HeartbeatPayload& payload,
    std::span<std::byte> output) noexcept {
    if (output.size() < heartbeat_payload_size) {
        return ControlPayloadError::output_too_small;
    }
    detail::write_u64(output, 0, payload.sender_monotonic_time_us);
    return ControlPayloadError::none;
}

ControlParseResult<HeartbeatPayload> parse_heartbeat(
    std::span<const std::byte> payload) noexcept {
    ControlParseResult<HeartbeatPayload> result{};
    if (payload.size() != heartbeat_payload_size) {
        result.error = ControlPayloadError::invalid_payload_size;
        return result;
    }
    result.value.sender_monotonic_time_us = detail::read_u64(payload, 0);
    return result;
}

ControlPayloadError encode_receiver_report(
    const ReceiverReportPayload& payload,
    std::span<std::byte> output) noexcept {
    if (output.size() < receiver_report_payload_size) {
        return ControlPayloadError::output_too_small;
    }

    detail::write_u32(
        output,
        receiver_report_highest_sequence_offset,
        payload.highest_sequence_number_received);
    detail::write_u32(
        output,
        receiver_report_missing_offset,
        payload.cumulative_missing_datagrams);
    detail::write_u32(
        output,
        receiver_report_late_offset,
        payload.cumulative_late_datagrams);
    detail::write_u32(
        output,
        receiver_report_jitter_offset,
        payload.estimated_network_jitter_us);
    detail::write_u16(output, receiver_report_buffered_offset, payload.buffered_audio_ms);
    detail::write_u16(
        output,
        receiver_report_underruns_offset,
        payload.cumulative_playback_underruns);
    return ControlPayloadError::none;
}

ControlParseResult<ReceiverReportPayload> parse_receiver_report(
    std::span<const std::byte> payload) noexcept {
    ControlParseResult<ReceiverReportPayload> result{};
    if (payload.size() != receiver_report_payload_size) {
        result.error = ControlPayloadError::invalid_payload_size;
        return result;
    }

    result.value.highest_sequence_number_received =
        detail::read_u32(payload, receiver_report_highest_sequence_offset);
    result.value.cumulative_missing_datagrams =
        detail::read_u32(payload, receiver_report_missing_offset);
    result.value.cumulative_late_datagrams =
        detail::read_u32(payload, receiver_report_late_offset);
    result.value.estimated_network_jitter_us =
        detail::read_u32(payload, receiver_report_jitter_offset);
    result.value.buffered_audio_ms = detail::read_u16(payload, receiver_report_buffered_offset);
    result.value.cumulative_playback_underruns =
        detail::read_u16(payload, receiver_report_underruns_offset);
    return result;
}

ControlPayloadError encode_error_payload(
    const ErrorPayloadView& payload,
    std::span<std::byte> output) noexcept {
    if (payload.message.size() > error_message_maximum_size) {
        return ControlPayloadError::error_message_too_large;
    }

    if (!valid_utf8(payload.message)) {
        return ControlPayloadError::invalid_utf8;
    }

    const auto required_size = encoded_error_payload_size(payload.message);
    if (output.size() < required_size) {
        return ControlPayloadError::output_too_small;
    }

    detail::write_u16(output, 0, payload.code);
    for (std::size_t index = 0; index < payload.message.size(); ++index) {
        output[error_payload_minimum_size + index] =
            static_cast<std::byte>(static_cast<unsigned char>(payload.message[index]));
    }
    return ControlPayloadError::none;
}

ControlParseResult<ErrorPayloadView> parse_error_payload(
    std::span<const std::byte> payload) noexcept {
    ControlParseResult<ErrorPayloadView> result{};
    if (payload.size() < error_payload_minimum_size ||
        payload.size() > error_payload_maximum_size) {
        result.error = ControlPayloadError::invalid_payload_size;
        return result;
    }

    const auto message_bytes = payload.subspan(error_payload_minimum_size);
    const auto* message_data = reinterpret_cast<const char*>(message_bytes.data());
    const std::string_view message{message_data, message_bytes.size()};
    if (!valid_utf8(message)) {
        result.error = ControlPayloadError::invalid_utf8;
        return result;
    }

    result.value.code = detail::read_u16(payload, 0);
    result.value.message = message;
    return result;
}

std::string_view to_string(ControlPayloadError error) noexcept {
    switch (error) {
    case ControlPayloadError::none:
        return "none";
    case ControlPayloadError::output_too_small:
        return "output_too_small";
    case ControlPayloadError::invalid_payload_size:
        return "invalid_payload_size";
    case ControlPayloadError::unsupported_codec:
        return "unsupported_codec";
    case ControlPayloadError::unsupported_channel_count:
        return "unsupported_channel_count";
    case ControlPayloadError::unsupported_frame_duration:
        return "unsupported_frame_duration";
    case ControlPayloadError::unsupported_sample_rate:
        return "unsupported_sample_rate";
    case ControlPayloadError::invalid_pcm_target_bitrate:
        return "invalid_pcm_target_bitrate";
    case ControlPayloadError::invalid_pcm_pre_skip:
        return "invalid_pcm_pre_skip";
    case ControlPayloadError::invalid_opus_target_bitrate:
        return "invalid_opus_target_bitrate";
    case ControlPayloadError::reserved_field_nonzero:
        return "reserved_field_nonzero";
    case ControlPayloadError::error_message_too_large:
        return "error_message_too_large";
    case ControlPayloadError::invalid_utf8:
        return "invalid_utf8";
    }

    return "unknown_control_payload_error";
}

} // namespace wiretone::protocol
