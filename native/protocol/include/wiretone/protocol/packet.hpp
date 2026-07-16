#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wiretone::protocol {

inline constexpr std::array<std::byte, 4> packet_magic{
    std::byte{'W'},
    std::byte{'T'},
    std::byte{'P'},
    std::byte{'K'},
};

inline constexpr std::uint8_t protocol_version = 1;
inline constexpr std::size_t packet_header_size = 32;
inline constexpr std::size_t maximum_datagram_size = 1200;
inline constexpr std::size_t maximum_payload_size =
    maximum_datagram_size - packet_header_size;

inline constexpr std::uint16_t flag_discontinuity = 0x0001;
inline constexpr std::uint16_t flag_silence = 0x0002;
inline constexpr std::uint16_t known_flag_mask =
    flag_discontinuity | flag_silence;

enum class PacketType : std::uint8_t {
    stream_start = 1,
    stream_stop = 2,
    audio = 3,
    heartbeat = 4,
    receiver_report = 5,
    error = 6,
};

struct PacketHeader {
    std::uint8_t version{protocol_version};
    PacketType type{PacketType::heartbeat};
    std::uint16_t flags{0};
    std::uint32_t stream_id{0};
    std::uint32_t sequence_number{0};
    std::uint64_t timestamp_samples{0};
    std::uint16_t payload_size{0};
    std::uint8_t fragment_index{0};
    std::uint8_t fragment_count{1};
    std::uint32_t frame_id{0};
};

enum class ProtocolError {
    none = 0,
    output_too_small,
    datagram_too_small,
    datagram_too_large,
    invalid_magic,
    unsupported_version,
    unknown_packet_type,
    unknown_flags,
    zero_stream_id,
    invalid_fragment_count,
    invalid_fragment_index,
    fragmented_control_packet,
    control_packet_has_frame_id,
    invalid_payload_size,
    payload_length_mismatch,
    audio_payload_missing,
};

struct ParseResult {
    ProtocolError error{ProtocolError::none};
    PacketHeader header{};
    std::span<const std::byte> payload{};

    [[nodiscard]] bool ok() const noexcept {
        return error == ProtocolError::none;
    }
};

[[nodiscard]] ProtocolError validate_header(const PacketHeader& header) noexcept;

[[nodiscard]] ProtocolError encode_header(
    const PacketHeader& header,
    std::span<std::byte> output) noexcept;

[[nodiscard]] ParseResult parse_datagram(
    std::span<const std::byte> datagram) noexcept;

[[nodiscard]] std::string_view to_string(ProtocolError error) noexcept;

} // namespace wiretone::protocol
