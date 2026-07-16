#include "wiretone/capture/pcm_frame_assembler.hpp"

#include <algorithm>
#include <cstring>

namespace wiretone::capture {
namespace {

[[nodiscard]] constexpr bool has_flag(
    std::uint32_t flags,
    std::uint32_t flag) noexcept {
    return (flags & flag) != 0U;
}

[[nodiscard]] constexpr std::uint64_t frames_to_100ns(
    std::uint64_t frame_count) noexcept {
    return (frame_count * 10'000'000ULL) / normalized_sample_rate;
}

} // namespace

PcmFrameAssemblyResult PcmFrameAssembler::accept_packet(
    std::span<const std::byte> normalized_pcm,
    std::uint32_t frame_count,
    std::uint32_t flags,
    std::uint64_t device_position_frames,
    std::uint64_t qpc_position_100ns,
    PcmFrameSink sink,
    void* sink_context) noexcept {
    PcmFrameAssemblyResult result{};
    result.input_frames = frame_count;

    if (frame_count == 0U) {
        result.error = PcmFrameAssemblerError::zero_frame_count;
        return result;
    }

    if ((flags & ~captured_packet_known_flag_mask) != 0U) {
        result.error = PcmFrameAssemblerError::unknown_flags;
        return result;
    }

    if (normalized_pcm.size() != normalized_pcm_size_for_frames(frame_count)) {
        result.error = PcmFrameAssemblerError::invalid_pcm_size;
        return result;
    }

    if (sink == nullptr) {
        result.error = PcmFrameAssemblerError::sink_missing;
        return result;
    }

    bool apply_discontinuity =
        has_flag(flags, captured_packet_flag_discontinuity);
    if (apply_discontinuity && pending_frames_ != 0U) {
        clear_partial();
        result.dropped_partial_frames = 1U;
    }

    const bool packet_silent = has_flag(flags, captured_packet_flag_silence);
    const bool packet_timestamp_error =
        has_flag(flags, captured_packet_flag_timestamp_error);

    std::uint32_t consumed_frames = 0U;
    while (consumed_frames < frame_count) {
        if (pending_frames_ == 0U) {
            pending_all_silent_ = true;
            pending_flags_ = apply_discontinuity
                ? captured_packet_flag_discontinuity
                : 0U;
            apply_discontinuity = false;
            pending_device_position_frames_ =
                device_position_frames + consumed_frames;
            pending_qpc_position_100ns_ =
                qpc_position_100ns + frames_to_100ns(consumed_frames);
        }

        if (packet_timestamp_error) {
            pending_flags_ |= captured_packet_flag_timestamp_error;
        }
        pending_all_silent_ = pending_all_silent_ && packet_silent;

        const std::uint32_t remaining_in_frame =
            logical_pcm_frame_samples - pending_frames_;
        const std::uint32_t remaining_in_packet = frame_count - consumed_frames;
        const std::uint32_t copy_frames =
            std::min(remaining_in_frame, remaining_in_packet);

        const std::size_t source_offset = normalized_pcm_size_for_frames(consumed_frames);
        const std::size_t destination_offset = normalized_pcm_size_for_frames(pending_frames_);
        const std::size_t copy_bytes = normalized_pcm_size_for_frames(copy_frames);

        std::memcpy(
            storage_.data() + static_cast<std::ptrdiff_t>(destination_offset),
            normalized_pcm.data() + static_cast<std::ptrdiff_t>(source_offset),
            copy_bytes);

        pending_frames_ += copy_frames;
        consumed_frames += copy_frames;

        if (pending_frames_ != logical_pcm_frame_samples) {
            continue;
        }

        std::uint32_t completed_flags = pending_flags_;
        if (pending_all_silent_) {
            completed_flags |= captured_packet_flag_silence;
        }

        const PcmFrameView frame{
            std::span<const std::byte>(storage_.data(), storage_.size()),
            next_sequence_number_,
            completed_flags,
            pending_device_position_frames_,
            pending_qpc_position_100ns_,
        };
        sink(frame, sink_context);

        ++next_sequence_number_;
        ++result.completed_frames;
        clear_partial();
    }

    return result;
}

bool PcmFrameAssembler::discard_partial() noexcept {
    const bool discarded_partial = pending_frames_ != 0U;
    clear_partial();
    return discarded_partial;
}

bool PcmFrameAssembler::reset() noexcept {
    const bool discarded_partial = discard_partial();
    next_sequence_number_ = 1U;
    return discarded_partial;
}

std::uint32_t PcmFrameAssembler::pending_frame_count() const noexcept {
    return pending_frames_;
}

std::uint64_t PcmFrameAssembler::next_sequence_number() const noexcept {
    return next_sequence_number_;
}

void PcmFrameAssembler::clear_partial() noexcept {
    pending_frames_ = 0U;
    pending_flags_ = 0U;
    pending_all_silent_ = true;
    pending_device_position_frames_ = 0U;
    pending_qpc_position_100ns_ = 0U;
}

std::string_view to_string(PcmFrameAssemblerError error) noexcept {
    switch (error) {
    case PcmFrameAssemblerError::none:
        return "none";
    case PcmFrameAssemblerError::zero_frame_count:
        return "zero_frame_count";
    case PcmFrameAssemblerError::unknown_flags:
        return "unknown_flags";
    case PcmFrameAssemblerError::invalid_pcm_size:
        return "invalid_pcm_size";
    case PcmFrameAssemblerError::sink_missing:
        return "sink_missing";
    }

    return "unknown_pcm_frame_assembler_error";
}

} // namespace wiretone::capture
