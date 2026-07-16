#include "wiretone/protocol/packet.hpp"

#include <algorithm>
#include <limits>

namespace wiretone::protocol {
namespace {

constexpr std::size_t magic_offset = 0;
constexpr std::size_t version_offset = 4;
constexpr std::size_t type_offset = 5;
constexpr std::size_t flags_offset = 6;
constexpr std::size_t stream_id_offset = 8;
constexpr std::size_t sequence_number_offset = 12;
constexpr std::size_t timestamp_samples_offset = 16;
constexpr std::size_t payload_size_offset = 24;
constexpr std::size_t fragment_index_offset = 26;
constexpr std::size_t fragment_count_offset = 27;
constexpr std::size_t frame_id_offset = 28;

[[nodiscard]] constexpr bool is_known_packet_type(std::uint8_t value) noexcept {
    return value >= static_cast<std::uint8_t>(PacketType::stream_start) &&
           value <= static_cast<std::uint8_t>(PacketType::error);
}

[[nodiscard]] constexpr bool is_control_packet(PacketType type) noexcept {
    return type != PacketType::audio;
}

[[nodiscard]] constexpr bool valid_payload_size(
    PacketType type,
    std::uint16_t payload_size) noexcept {
    switch (type) {
    case PacketType::stream_start:
        return payload_size == 16;
    case PacketType::stream_stop:
        return payload_size == 1;
    case PacketType::audio:
        return payload_size <= maximum_payload_size;
    case PacketType::heartbeat:
        return payload_size == 8;
    case PacketType::receiver_report:
        return payload_size == 20;
    case PacketType::error:
        return payload_size >= 2 && payload_size <= 258;
    }

    return false;
}

void write_u16(std::span<std::byte> output, std::size_t offset, std::uint16_t value) noexcept {
    output[offset] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    output[offset + 1] = static_cast<std::byte>(value & 0xFFU);
}

void write_u32(std::span<std::byte> output, std::size_t offset, std::uint32_t value) noexcept {
    output[offset] = static_cast<std::byte>((value >> 24U) & 0xFFU);
    output[offset + 1] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    output[offset + 2] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    output[offset + 3] = static_cast<std::byte>(value & 0xFFU);
}

void write_u64(std::span<std::byte> output, std::size_t offset, std::uint64_t value) noexcept {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        const auto shift = static_cast<unsigned int>((sizeof(value) - 1U - index) * 8U);
        output[offset + index] = static_cast<std::byte>((value >> shift) & 0xFFU);
    }
}

[[nodiscard]] std::uint16_t read_u16(
    std::span<const std::byte> input,
    std::size_t offset) noexcept {
    const auto high = std::to_integer<std::uint16_t>(input[offset]);
    const auto low = std::to_integer<std::uint16_t>(input[offset + 1]);
    return static_cast<std::uint16_t>((high << 8U) | low);
}

[[nodiscard]] std::uint32_t read_u32(
    std::span<const std::byte> input,
    std::size_t offset) noexcept {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = static_cast<std::uint32_t>(
            (value << 8U) | std::to_integer<std::uint32_t>(input[offset + index]));
    }
    return value;
}

[[nodiscard]] std::uint64_t read_u64(
    std::span<const std::byte> input,
    std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = static_cast<std::uint64_t>(
            (value << 8U) | std::to_integer<std::uint64_t>(input[offset + index]));
    }
    return value;
}

} // namespace

ProtocolError validate_header(const PacketHeader& header) noexcept {
    if (header.version != protocol_version) {
        return ProtocolError::unsupported_version;
    }

    const auto type_value = static_cast<std::uint8_t>(header.type);
    if (!is_known_packet_type(type_value)) {
        return ProtocolError::unknown_packet_type;
    }

    if ((header.flags & static_cast<std::uint16_t>(~known_flag_mask)) != 0U) {
        return ProtocolError::unknown_flags;
    }

    if (header.stream_id == 0U) {
        return ProtocolError::zero_stream_id;
    }

    if (header.fragment_count == 0U) {
        return ProtocolError::invalid_fragment_count;
    }

    if (header.fragment_index >= header.fragment_count) {
        return ProtocolError::invalid_fragment_index;
    }

    if (is_control_packet(header.type)) {
        if (header.flags != 0U) {
            return ProtocolError::unknown_flags;
        }

        if (header.fragment_count != 1U || header.fragment_index != 0U) {
            return ProtocolError::fragmented_control_packet;
        }

        if (header.frame_id != 0U) {
            return ProtocolError::control_packet_has_frame_id;
        }
    }

    if (!valid_payload_size(header.type, header.payload_size)) {
        return ProtocolError::invalid_payload_size;
    }

    if (header.type == PacketType::audio &&
        header.payload_size == 0U &&
        (header.flags & flag_silence) == 0U) {
        return ProtocolError::audio_payload_missing;
    }

    return ProtocolError::none;
}

ProtocolError encode_header(
    const PacketHeader& header,
    std::span<std::byte> output) noexcept {
    if (output.size() < packet_header_size) {
        return ProtocolError::output_too_small;
    }

    const auto validation = validate_header(header);
    if (validation != ProtocolError::none) {
        return validation;
    }

    std::fill_n(output.begin(), packet_header_size, std::byte{0});
    std::copy(packet_magic.begin(), packet_magic.end(), output.begin() + magic_offset);

    output[version_offset] = static_cast<std::byte>(header.version);
    output[type_offset] = static_cast<std::byte>(header.type);
    write_u16(output, flags_offset, header.flags);
    write_u32(output, stream_id_offset, header.stream_id);
    write_u32(output, sequence_number_offset, header.sequence_number);
    write_u64(output, timestamp_samples_offset, header.timestamp_samples);
    write_u16(output, payload_size_offset, header.payload_size);
    output[fragment_index_offset] = static_cast<std::byte>(header.fragment_index);
    output[fragment_count_offset] = static_cast<std::byte>(header.fragment_count);
    write_u32(output, frame_id_offset, header.frame_id);

    return ProtocolError::none;
}

ParseResult parse_datagram(std::span<const std::byte> datagram) noexcept {
    ParseResult result{};

    if (datagram.size() < packet_header_size) {
        result.error = ProtocolError::datagram_too_small;
        return result;
    }

    if (datagram.size() > maximum_datagram_size) {
        result.error = ProtocolError::datagram_too_large;
        return result;
    }

    if (!std::equal(packet_magic.begin(), packet_magic.end(), datagram.begin() + magic_offset)) {
        result.error = ProtocolError::invalid_magic;
        return result;
    }

    const auto type_value = std::to_integer<std::uint8_t>(datagram[type_offset]);
    if (!is_known_packet_type(type_value)) {
        result.error = ProtocolError::unknown_packet_type;
        return result;
    }

    result.header.version = std::to_integer<std::uint8_t>(datagram[version_offset]);
    result.header.type = static_cast<PacketType>(type_value);
    result.header.flags = read_u16(datagram, flags_offset);
    result.header.stream_id = read_u32(datagram, stream_id_offset);
    result.header.sequence_number = read_u32(datagram, sequence_number_offset);
    result.header.timestamp_samples = read_u64(datagram, timestamp_samples_offset);
    result.header.payload_size = read_u16(datagram, payload_size_offset);
    result.header.fragment_index =
        std::to_integer<std::uint8_t>(datagram[fragment_index_offset]);
    result.header.fragment_count =
        std::to_integer<std::uint8_t>(datagram[fragment_count_offset]);
    result.header.frame_id = read_u32(datagram, frame_id_offset);

    const auto validation = validate_header(result.header);
    if (validation != ProtocolError::none) {
        result.error = validation;
        return result;
    }

    const auto actual_payload_size = datagram.size() - packet_header_size;
    if (actual_payload_size != result.header.payload_size) {
        result.error = ProtocolError::payload_length_mismatch;
        return result;
    }

    result.payload = datagram.subspan(packet_header_size);
    return result;
}

std::string_view to_string(ProtocolError error) noexcept {
    switch (error) {
    case ProtocolError::none:
        return "none";
    case ProtocolError::output_too_small:
        return "output_too_small";
    case ProtocolError::datagram_too_small:
        return "datagram_too_small";
    case ProtocolError::datagram_too_large:
        return "datagram_too_large";
    case ProtocolError::invalid_magic:
        return "invalid_magic";
    case ProtocolError::unsupported_version:
        return "unsupported_version";
    case ProtocolError::unknown_packet_type:
        return "unknown_packet_type";
    case ProtocolError::unknown_flags:
        return "unknown_flags";
    case ProtocolError::zero_stream_id:
        return "zero_stream_id";
    case ProtocolError::invalid_fragment_count:
        return "invalid_fragment_count";
    case ProtocolError::invalid_fragment_index:
        return "invalid_fragment_index";
    case ProtocolError::fragmented_control_packet:
        return "fragmented_control_packet";
    case ProtocolError::control_packet_has_frame_id:
        return "control_packet_has_frame_id";
    case ProtocolError::invalid_payload_size:
        return "invalid_payload_size";
    case ProtocolError::payload_length_mismatch:
        return "payload_length_mismatch";
    case ProtocolError::audio_payload_missing:
        return "audio_payload_missing";
    }

    return "unknown_protocol_error";
}

} // namespace wiretone::protocol
