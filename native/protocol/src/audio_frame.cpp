#include "wiretone/protocol/audio_frame.hpp"

#include <algorithm>

namespace wiretone::protocol {
namespace {

[[nodiscard]] constexpr bool has_flag(
    std::uint16_t flags,
    std::uint16_t flag) noexcept {
    return (flags & flag) != 0U;
}

[[nodiscard]] AudioFrameError validate_frame_view(
    const AudioFrameView& frame) noexcept {
    if (frame.stream_id == 0U) {
        return AudioFrameError::zero_stream_id;
    }

    if (frame.frame_id == 0U) {
        return AudioFrameError::zero_frame_id;
    }

    if ((frame.flags & static_cast<std::uint16_t>(~known_flag_mask)) != 0U) {
        return AudioFrameError::unknown_flags;
    }

    if (frame.payload.size() > maximum_audio_frame_payload_size) {
        return AudioFrameError::payload_too_large;
    }

    const bool silence = has_flag(frame.flags, flag_silence);
    if (silence && !frame.payload.empty()) {
        return AudioFrameError::silence_payload_present;
    }

    if (!silence && frame.payload.empty()) {
        return AudioFrameError::empty_payload_without_silence;
    }

    return AudioFrameError::none;
}

[[nodiscard]] AudioFrameError validate_fragment_shape(
    const ParseResult& parsed) noexcept {
    const auto& header = parsed.header;
    const bool silence = has_flag(header.flags, flag_silence);

    if (header.fragment_count > maximum_audio_fragment_count) {
        return AudioFrameError::fragment_count_exceeds_limit;
    }

    if (silence) {
        if (!parsed.payload.empty()) {
            return AudioFrameError::silence_payload_present;
        }

        if (header.fragment_count != 1U || header.fragment_index != 0U) {
            return AudioFrameError::noncanonical_fragment_size;
        }

        return AudioFrameError::none;
    }

    if (parsed.payload.empty()) {
        return AudioFrameError::empty_payload_without_silence;
    }

    const bool final_fragment =
        static_cast<std::uint16_t>(header.fragment_index) + 1U ==
        static_cast<std::uint16_t>(header.fragment_count);

    if (!final_fragment && parsed.payload.size() != maximum_payload_size) {
        return AudioFrameError::noncanonical_fragment_size;
    }

    const std::size_t offset =
        static_cast<std::size_t>(header.fragment_index) * maximum_payload_size;
    if (offset + parsed.payload.size() > maximum_audio_frame_payload_size) {
        return AudioFrameError::payload_too_large;
    }

    return AudioFrameError::none;
}


} // namespace

AudioFragmentationResult fragment_audio_frame(const AudioFrameView& frame) noexcept {
    AudioFragmentationResult result{};
    result.next_sequence_number = frame.first_sequence_number;

    const auto validation = validate_frame_view(frame);
    if (validation != AudioFrameError::none) {
        result.error = validation;
        return result;
    }

    const bool silence = has_flag(frame.flags, flag_silence);
    const std::size_t fragment_count = silence
        ? 1U
        : (frame.payload.size() + maximum_payload_size - 1U) / maximum_payload_size;

    result.datagram_count = fragment_count;

    for (std::size_t index = 0; index < fragment_count; ++index) {
        const std::size_t payload_offset = index * maximum_payload_size;
        const std::size_t remaining = silence ? 0U : frame.payload.size() - payload_offset;
        const std::size_t fragment_payload_size =
            silence ? 0U : std::min(remaining, maximum_payload_size);

        PacketHeader header{};
        header.type = PacketType::audio;
        header.flags = frame.flags;
        header.stream_id = frame.stream_id;
        header.sequence_number =
            frame.first_sequence_number + static_cast<std::uint32_t>(index);
        header.timestamp_samples = frame.timestamp_samples;
        header.payload_size = static_cast<std::uint16_t>(fragment_payload_size);
        header.fragment_index = static_cast<std::uint8_t>(index);
        header.fragment_count = static_cast<std::uint8_t>(fragment_count);
        header.frame_id = frame.frame_id;

        auto& datagram = result.datagrams[index];
        const auto header_result = encode_header(header, datagram.storage);
        if (header_result != ProtocolError::none) {
            result.error = AudioFrameError::header_encoding_failed;
            result.protocol_error = header_result;
            result.datagram_count = 0U;
            return result;
        }

        if (fragment_payload_size != 0U) {
            const auto fragment_payload = frame.payload.subspan(
                payload_offset,
                fragment_payload_size);
            std::copy(
                fragment_payload.begin(),
                fragment_payload.end(),
                datagram.storage.begin() +
                    static_cast<std::ptrdiff_t>(packet_header_size));
        }

        datagram.size = packet_header_size + fragment_payload_size;
    }

    result.next_sequence_number =
        frame.first_sequence_number + static_cast<std::uint32_t>(fragment_count);
    return result;
}

AudioReassemblyResult AudioFrameReassembler::advance_time(
    std::uint64_t now_ms) noexcept {
    AudioReassemblyResult result{};

    if (has_observed_time_ && now_ms < last_observed_time_ms_) {
        result.error = AudioFrameError::non_monotonic_time;
        return result;
    }

    has_observed_time_ = true;
    last_observed_time_ms_ = now_ms;

    for (auto& slot : slots_) {
        if (!slot.active) {
            continue;
        }

        if (now_ms - slot.last_arrival_time_ms >=
            audio_frame_reassembly_timeout_ms) {
            slot = Slot{};
            ++result.expired_frame_count;
        }
    }

    return result;
}

AudioReassemblyResult AudioFrameReassembler::expire_incomplete(
    std::uint64_t now_ms) noexcept {
    return advance_time(now_ms);
}

void AudioFrameReassembler::reset() noexcept {
    slots_ = {};
    has_observed_time_ = false;
    last_observed_time_ms_ = 0U;
}

std::size_t AudioFrameReassembler::in_flight_frame_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        slots_.begin(),
        slots_.end(),
        [](const Slot& slot) { return slot.active; }));
}

AudioReassemblyResult AudioFrameReassembler::accept_datagram(
    std::span<const std::byte> datagram,
    std::uint64_t arrival_time_ms) noexcept {
    auto result = advance_time(arrival_time_ms);
    if (!result.ok()) {
        return result;
    }

    const auto parsed = parse_datagram(datagram);
    if (!parsed.ok()) {
        result.error = AudioFrameError::malformed_datagram;
        result.protocol_error = parsed.error;
        return result;
    }

    if (parsed.header.type != PacketType::audio) {
        result.error = AudioFrameError::not_audio_packet;
        return result;
    }

    if (parsed.header.frame_id == 0U) {
        result.error = AudioFrameError::zero_frame_id;
        return result;
    }

    const auto shape_validation = validate_fragment_shape(parsed);
    if (shape_validation != AudioFrameError::none) {
        result.error = shape_validation;
        return result;
    }

    const std::uint32_t first_sequence_number =
        parsed.header.sequence_number -
        static_cast<std::uint32_t>(parsed.header.fragment_index);

    Slot* slot = nullptr;
    Slot* free_slot = nullptr;

    for (auto& candidate : slots_) {
        if (candidate.active &&
            candidate.stream_id == parsed.header.stream_id &&
            candidate.frame_id == parsed.header.frame_id) {
            slot = &candidate;
            break;
        }

        if (!candidate.active && free_slot == nullptr) {
            free_slot = &candidate;
        }
    }

    if (slot == nullptr) {
        if (free_slot == nullptr) {
            result.error = AudioFrameError::reassembly_window_full;
            return result;
        }

        slot = free_slot;
        slot->active = true;
        slot->stream_id = parsed.header.stream_id;
        slot->frame_id = parsed.header.frame_id;
        slot->first_sequence_number = first_sequence_number;
        slot->timestamp_samples = parsed.header.timestamp_samples;
        slot->flags = parsed.header.flags;
        slot->fragment_count = parsed.header.fragment_count;
        slot->last_arrival_time_ms = arrival_time_ms;
    } else if (
        slot->first_sequence_number != first_sequence_number ||
        slot->timestamp_samples != parsed.header.timestamp_samples ||
        slot->flags != parsed.header.flags ||
        slot->fragment_count != parsed.header.fragment_count) {
        result.error = AudioFrameError::inconsistent_fragment_metadata;
        return result;
    }

    const std::size_t fragment_index = parsed.header.fragment_index;
    if (slot->received[fragment_index]) {
        result.error = AudioFrameError::duplicate_fragment;
        return result;
    }

    const std::size_t payload_offset = fragment_index * maximum_payload_size;
    if (!parsed.payload.empty()) {
        std::copy(
            parsed.payload.begin(),
            parsed.payload.end(),
            slot->storage.begin() + static_cast<std::ptrdiff_t>(payload_offset));
    }

    slot->received[fragment_index] = true;
    slot->fragment_sizes[fragment_index] =
        static_cast<std::uint16_t>(parsed.payload.size());
    ++slot->received_count;
    slot->last_arrival_time_ms = arrival_time_ms;

    if (slot->received_count != slot->fragment_count) {
        return result;
    }

    const std::size_t final_fragment_index =
        static_cast<std::size_t>(slot->fragment_count - 1U);
    const std::size_t payload_size =
        final_fragment_index * maximum_payload_size +
        slot->fragment_sizes[final_fragment_index];

    result.frame_completed = true;
    result.frame.stream_id = slot->stream_id;
    result.frame.first_sequence_number = slot->first_sequence_number;
    result.frame.timestamp_samples = slot->timestamp_samples;
    result.frame.frame_id = slot->frame_id;
    result.frame.flags = slot->flags;
    result.frame.payload_size = payload_size;
    std::copy_n(
        slot->storage.begin(),
        payload_size,
        result.frame.storage.begin());

    *slot = Slot{};
    return result;
}

std::string_view to_string(AudioFrameError error) noexcept {
    switch (error) {
    case AudioFrameError::none:
        return "none";
    case AudioFrameError::zero_stream_id:
        return "zero_stream_id";
    case AudioFrameError::zero_frame_id:
        return "zero_frame_id";
    case AudioFrameError::unknown_flags:
        return "unknown_flags";
    case AudioFrameError::payload_too_large:
        return "payload_too_large";
    case AudioFrameError::empty_payload_without_silence:
        return "empty_payload_without_silence";
    case AudioFrameError::silence_payload_present:
        return "silence_payload_present";
    case AudioFrameError::header_encoding_failed:
        return "header_encoding_failed";
    case AudioFrameError::malformed_datagram:
        return "malformed_datagram";
    case AudioFrameError::not_audio_packet:
        return "not_audio_packet";
    case AudioFrameError::fragment_count_exceeds_limit:
        return "fragment_count_exceeds_limit";
    case AudioFrameError::noncanonical_fragment_size:
        return "noncanonical_fragment_size";
    case AudioFrameError::duplicate_fragment:
        return "duplicate_fragment";
    case AudioFrameError::inconsistent_fragment_metadata:
        return "inconsistent_fragment_metadata";
    case AudioFrameError::reassembly_window_full:
        return "reassembly_window_full";
    case AudioFrameError::non_monotonic_time:
        return "non_monotonic_time";
    }

    return "unknown_audio_frame_error";
}

} // namespace wiretone::protocol
