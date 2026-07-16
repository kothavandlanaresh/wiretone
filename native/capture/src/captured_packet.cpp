#include "wiretone/capture/captured_packet.hpp"

#include <cmath>
#include <cstring>

namespace wiretone::capture {
namespace {

[[nodiscard]] bool has_flag(std::uint32_t flags, std::uint32_t flag) noexcept {
    return (flags & flag) != 0U;
}

[[nodiscard]] std::int16_t float_to_pcm_s16(float value) noexcept {
    if (value >= 1.0F) {
        return static_cast<std::int16_t>(32'767);
    }

    if (value <= -1.0F) {
        return static_cast<std::int16_t>(-32'768);
    }

    const float scale = value < 0.0F ? 32'768.0F : 32'767.0F;
    return static_cast<std::int16_t>(std::lround(value * scale));
}

void write_little_endian_s16(
    std::int16_t sample,
    std::span<std::byte> output,
    std::size_t offset) noexcept {
    const auto encoded = static_cast<std::uint16_t>(sample);
    output[offset] = static_cast<std::byte>(encoded & 0x00FFU);
    output[offset + 1U] = static_cast<std::byte>((encoded >> 8U) & 0x00FFU);
}

} // namespace

CapturedPacketConversionResult convert_float32_stereo_to_pcm_s16le(
    const CapturedPacketView& packet,
    std::span<std::byte> output) noexcept {
    CapturedPacketConversionResult result{};
    result.frame_count = packet.frame_count;
    result.flags = packet.flags;
    result.device_position_frames = packet.device_position_frames;
    result.qpc_position_100ns = packet.qpc_position_100ns;

    if (packet.frame_count == 0U) {
        result.error = CapturedPacketError::zero_frame_count;
        return result;
    }

    if ((packet.flags & ~captured_packet_known_flag_mask) != 0U) {
        result.error = CapturedPacketError::unknown_flags;
        return result;
    }

    const std::size_t required_output_size =
        normalized_pcm_size_for_frames(packet.frame_count);
    if (output.size() < required_output_size) {
        result.error = CapturedPacketError::output_too_small;
        return result;
    }

    if (has_flag(packet.flags, captured_packet_flag_silence)) {
        for (std::size_t index = 0; index < required_output_size; ++index) {
            output[index] = std::byte{0};
        }
        result.output_size = required_output_size;
        return result;
    }

    const std::size_t required_input_size =
        static_cast<std::size_t>(packet.frame_count) * float32_stereo_bytes_per_frame;
    if (packet.data.empty()) {
        result.error = CapturedPacketError::data_missing;
        return result;
    }

    if (packet.data.size() != required_input_size) {
        result.error = CapturedPacketError::invalid_data_size;
        return result;
    }

    const std::size_t sample_count =
        static_cast<std::size_t>(packet.frame_count) * normalized_channel_count;

    for (std::size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        float sample = 0.0F;
        std::memcpy(
            &sample,
            packet.data.data() +
                static_cast<std::ptrdiff_t>(sample_index * sizeof(float)),
            sizeof(sample));
        if (!std::isfinite(sample)) {
            result.error = CapturedPacketError::non_finite_sample;
            return result;
        }
    }

    for (std::size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        float sample = 0.0F;
        std::memcpy(
            &sample,
            packet.data.data() +
                static_cast<std::ptrdiff_t>(sample_index * sizeof(float)),
            sizeof(sample));
        write_little_endian_s16(
            float_to_pcm_s16(sample),
            output,
            sample_index * normalized_bytes_per_sample);
    }

    result.output_size = required_output_size;
    return result;
}

std::string_view to_string(CapturedPacketError error) noexcept {
    switch (error) {
    case CapturedPacketError::none:
        return "none";
    case CapturedPacketError::zero_frame_count:
        return "zero_frame_count";
    case CapturedPacketError::unknown_flags:
        return "unknown_flags";
    case CapturedPacketError::output_too_small:
        return "output_too_small";
    case CapturedPacketError::data_missing:
        return "data_missing";
    case CapturedPacketError::invalid_data_size:
        return "invalid_data_size";
    case CapturedPacketError::non_finite_sample:
        return "non_finite_sample";
    }

    return "unknown_captured_packet_error";
}

} // namespace wiretone::capture
