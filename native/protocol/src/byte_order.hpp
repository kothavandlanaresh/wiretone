#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace wiretone::protocol::detail {

inline void write_u16(
    std::span<std::byte> output,
    std::size_t offset,
    std::uint16_t value) noexcept {
    output[offset] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    output[offset + 1] = static_cast<std::byte>(value & 0xFFU);
}

inline void write_u32(
    std::span<std::byte> output,
    std::size_t offset,
    std::uint32_t value) noexcept {
    output[offset] = static_cast<std::byte>((value >> 24U) & 0xFFU);
    output[offset + 1] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    output[offset + 2] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    output[offset + 3] = static_cast<std::byte>(value & 0xFFU);
}

inline void write_u64(
    std::span<std::byte> output,
    std::size_t offset,
    std::uint64_t value) noexcept {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        const auto shift = static_cast<unsigned int>((sizeof(value) - 1U - index) * 8U);
        output[offset + index] = static_cast<std::byte>((value >> shift) & 0xFFU);
    }
}

[[nodiscard]] inline std::uint16_t read_u16(
    std::span<const std::byte> input,
    std::size_t offset) noexcept {
    const auto high = std::to_integer<std::uint16_t>(input[offset]);
    const auto low = std::to_integer<std::uint16_t>(input[offset + 1]);
    return static_cast<std::uint16_t>((high << 8U) | low);
}

[[nodiscard]] inline std::uint32_t read_u32(
    std::span<const std::byte> input,
    std::size_t offset) noexcept {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = static_cast<std::uint32_t>(
            (value << 8U) | std::to_integer<std::uint32_t>(input[offset + index]));
    }
    return value;
}

[[nodiscard]] inline std::uint64_t read_u64(
    std::span<const std::byte> input,
    std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = static_cast<std::uint64_t>(
            (value << 8U) | std::to_integer<std::uint64_t>(input[offset + index]));
    }
    return value;
}

} // namespace wiretone::protocol::detail
