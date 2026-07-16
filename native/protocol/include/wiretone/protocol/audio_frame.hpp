#pragma once

#include "wiretone/protocol/packet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wiretone::protocol {

inline constexpr std::size_t maximum_audio_frame_payload_size = 3'840;
inline constexpr std::size_t maximum_audio_fragment_count =
    (maximum_audio_frame_payload_size + maximum_payload_size - 1U) /
    maximum_payload_size;
inline constexpr std::size_t maximum_in_flight_audio_frames = 8;
inline constexpr std::uint64_t audio_frame_reassembly_timeout_ms = 250;

static_assert(maximum_audio_fragment_count == 4U);

struct AudioFrameView {
    std::uint32_t stream_id{0};
    std::uint32_t first_sequence_number{0};
    std::uint64_t timestamp_samples{0};
    std::uint32_t frame_id{0};
    std::uint16_t flags{0};
    std::span<const std::byte> payload{};
};

struct EncodedAudioDatagram {
    std::array<std::byte, maximum_datagram_size> storage{};
    std::size_t size{0};

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return std::span<const std::byte>(storage).first(size);
    }
};

enum class AudioFrameError {
    none = 0,
    zero_stream_id,
    zero_frame_id,
    unknown_flags,
    payload_too_large,
    empty_payload_without_silence,
    silence_payload_present,
    header_encoding_failed,
    malformed_datagram,
    not_audio_packet,
    fragment_count_exceeds_limit,
    noncanonical_fragment_size,
    duplicate_fragment,
    inconsistent_fragment_metadata,
    reassembly_window_full,
    non_monotonic_time,
};

struct AudioFragmentationResult {
    AudioFrameError error{AudioFrameError::none};
    ProtocolError protocol_error{ProtocolError::none};
    std::array<EncodedAudioDatagram, maximum_audio_fragment_count> datagrams{};
    std::size_t datagram_count{0};
    std::uint32_t next_sequence_number{0};

    [[nodiscard]] bool ok() const noexcept {
        return error == AudioFrameError::none;
    }
};

[[nodiscard]] AudioFragmentationResult fragment_audio_frame(
    const AudioFrameView& frame) noexcept;

struct ReassembledAudioFrame {
    std::uint32_t stream_id{0};
    std::uint32_t first_sequence_number{0};
    std::uint64_t timestamp_samples{0};
    std::uint32_t frame_id{0};
    std::uint16_t flags{0};
    std::array<std::byte, maximum_audio_frame_payload_size> storage{};
    std::size_t payload_size{0};

    [[nodiscard]] std::span<const std::byte> payload() const noexcept {
        return std::span<const std::byte>(storage).first(payload_size);
    }
};

struct AudioReassemblyResult {
    AudioFrameError error{AudioFrameError::none};
    ProtocolError protocol_error{ProtocolError::none};
    std::size_t expired_frame_count{0};
    bool frame_completed{false};
    ReassembledAudioFrame frame{};

    [[nodiscard]] bool ok() const noexcept {
        return error == AudioFrameError::none;
    }
};

class AudioFrameReassembler {
public:
    [[nodiscard]] AudioReassemblyResult accept_datagram(
        std::span<const std::byte> datagram,
        std::uint64_t arrival_time_ms) noexcept;

    [[nodiscard]] AudioReassemblyResult expire_incomplete(
        std::uint64_t now_ms) noexcept;

    [[nodiscard]] std::size_t in_flight_frame_count() const noexcept;

private:
    struct Slot {
        bool active{false};
        std::uint32_t stream_id{0};
        std::uint32_t frame_id{0};
        std::uint32_t first_sequence_number{0};
        std::uint64_t timestamp_samples{0};
        std::uint16_t flags{0};
        std::uint8_t fragment_count{0};
        std::array<bool, maximum_audio_fragment_count> received{};
        std::array<std::uint16_t, maximum_audio_fragment_count> fragment_sizes{};
        std::array<std::byte, maximum_audio_frame_payload_size> storage{};
        std::size_t received_count{0};
        std::uint64_t last_arrival_time_ms{0};
    };

    [[nodiscard]] AudioReassemblyResult advance_time(
        std::uint64_t now_ms) noexcept;

    std::array<Slot, maximum_in_flight_audio_frames> slots_{};
    bool has_observed_time_{false};
    std::uint64_t last_observed_time_ms_{0};
};

[[nodiscard]] std::string_view to_string(AudioFrameError error) noexcept;

} // namespace wiretone::protocol
