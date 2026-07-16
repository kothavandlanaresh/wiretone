#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wiretone::capture {

inline constexpr std::uint32_t normalized_sample_rate = 48'000;
inline constexpr std::uint16_t normalized_channel_count = 2;
inline constexpr std::size_t normalized_bytes_per_sample = 2;
inline constexpr std::size_t normalized_bytes_per_frame =
    normalized_channel_count * normalized_bytes_per_sample;
inline constexpr std::size_t float32_stereo_bytes_per_frame = 8;

inline constexpr std::uint32_t captured_packet_flag_silence = 0x0000'0001U;
inline constexpr std::uint32_t captured_packet_flag_discontinuity = 0x0000'0002U;
inline constexpr std::uint32_t captured_packet_flag_timestamp_error = 0x0000'0004U;
inline constexpr std::uint32_t captured_packet_known_flag_mask =
    captured_packet_flag_silence |
    captured_packet_flag_discontinuity |
    captured_packet_flag_timestamp_error;

struct CapturedPacketView {
    std::span<const std::byte> data{};
    std::uint32_t frame_count{0};
    std::uint32_t flags{0};
    std::uint64_t device_position_frames{0};
    std::uint64_t qpc_position_100ns{0};
};

enum class CapturedPacketError {
    none = 0,
    zero_frame_count,
    unknown_flags,
    output_too_small,
    data_missing,
    invalid_data_size,
    non_finite_sample,
};

struct CapturedPacketConversionResult {
    CapturedPacketError error{CapturedPacketError::none};
    std::size_t output_size{0};
    std::uint32_t frame_count{0};
    std::uint32_t flags{0};
    std::uint64_t device_position_frames{0};
    std::uint64_t qpc_position_100ns{0};

    [[nodiscard]] bool ok() const noexcept {
        return error == CapturedPacketError::none;
    }
};

[[nodiscard]] constexpr std::size_t normalized_pcm_size_for_frames(
    std::uint32_t frame_count) noexcept {
    return static_cast<std::size_t>(frame_count) * normalized_bytes_per_frame;
}

[[nodiscard]] CapturedPacketConversionResult convert_float32_stereo_to_pcm_s16le(
    const CapturedPacketView& packet,
    std::span<std::byte> output) noexcept;

[[nodiscard]] std::string_view to_string(CapturedPacketError error) noexcept;

} // namespace wiretone::capture
