#include "wiretone/playback/pcm_queue.hpp"

#include <algorithm>
#include <cstring>

namespace wiretone::playback {

PcmPlaybackEnqueueResult PcmPlaybackQueue::enqueue(
    const PcmPlaybackFrameView& frame) noexcept {
    PcmPlaybackEnqueueResult result{};

    if (frame.samples.size() != logical_pcm_frame_samples) {
        result.error = PcmPlaybackQueueError::invalid_sample_count;
        return result;
    }
    if (frame.sequence_number == 0U) {
        result.error = PcmPlaybackQueueError::zero_sequence_number;
        return result;
    }
    if ((frame.flags & ~pcm_frame_known_flag_mask) != 0U) {
        result.error = PcmPlaybackQueueError::unknown_flags;
        return result;
    }

    const std::uint64_t write = write_position_.load(std::memory_order_relaxed);
    const std::uint64_t read = read_position_.load(std::memory_order_acquire);
    const std::uint64_t queued = write - read;
    if (queued >= pcm_playback_queue_capacity) {
        dropped_logical_frames_.fetch_add(1U, std::memory_order_relaxed);
        result.error = PcmPlaybackQueueError::queue_full;
        result.queued_logical_frames = pcm_playback_queue_capacity;
        return result;
    }

    Slot& slot = slots_[static_cast<std::size_t>(write % pcm_playback_queue_capacity)];
    std::copy(frame.samples.begin(), frame.samples.end(), slot.samples.begin());
    slot.sequence_number = frame.sequence_number;
    slot.flags = frame.flags;

    last_enqueued_sequence_.store(frame.sequence_number, std::memory_order_relaxed);
    enqueued_logical_frames_.fetch_add(1U, std::memory_order_relaxed);
    write_position_.store(write + 1U, std::memory_order_release);

    result.queued_logical_frames = static_cast<std::uint32_t>(queued + 1U);
    return result;
}

PcmPlaybackRenderResult PcmPlaybackQueue::render(
    std::span<std::int16_t> output,
    std::uint32_t frame_count) noexcept {
    PcmPlaybackRenderResult result{};
    result.requested_frames = frame_count;

    if (frame_count == 0U) {
        result.error = PcmPlaybackQueueError::zero_output_frame_count;
        return result;
    }

    const std::size_t expected_samples =
        static_cast<std::size_t>(frame_count) * pcm_channel_count;
    if (output.size() != expected_samples) {
        result.error = PcmPlaybackQueueError::invalid_output_sample_count;
        return result;
    }

    std::fill(output.begin(), output.end(), static_cast<std::int16_t>(0));

    std::uint32_t output_frame_offset = 0U;
    std::uint64_t read = read_position_.load(std::memory_order_relaxed);
    std::uint32_t slot_offset = read_offset_frames_.load(std::memory_order_relaxed);

    while (output_frame_offset < frame_count) {
        const std::uint64_t write = write_position_.load(std::memory_order_acquire);
        if (read == write) {
            break;
        }

        const Slot& slot =
            slots_[static_cast<std::size_t>(read % pcm_playback_queue_capacity)];
        const std::uint32_t remaining_in_slot =
            logical_pcm_frame_frames - slot_offset;
        const std::uint32_t remaining_in_output = frame_count - output_frame_offset;
        const std::uint32_t copy_frames =
            std::min(remaining_in_slot, remaining_in_output);

        const std::size_t source_sample_offset =
            static_cast<std::size_t>(slot_offset) * pcm_channel_count;
        const std::size_t destination_sample_offset =
            static_cast<std::size_t>(output_frame_offset) * pcm_channel_count;
        const std::size_t copy_samples =
            static_cast<std::size_t>(copy_frames) * pcm_channel_count;

        std::memcpy(
            output.data() + static_cast<std::ptrdiff_t>(destination_sample_offset),
            slot.samples.data() + static_cast<std::ptrdiff_t>(source_sample_offset),
            copy_samples * sizeof(std::int16_t));

        output_frame_offset += copy_frames;
        slot_offset += copy_frames;
        result.queue_frames += copy_frames;

        if (slot_offset != logical_pcm_frame_frames) {
            continue;
        }

        const std::uint64_t completed_sequence = slot.sequence_number;
        const std::uint32_t completed_flags = slot.flags;
        ++read;
        slot_offset = 0U;

        consumed_logical_frames_.fetch_add(1U, std::memory_order_relaxed);
        last_consumed_sequence_.store(completed_sequence, std::memory_order_relaxed);
        last_consumed_flags_.store(completed_flags, std::memory_order_relaxed);
        if ((completed_flags & pcm_frame_flag_discontinuity) != 0U) {
            discontinuity_logical_frames_.fetch_add(1U, std::memory_order_relaxed);
        }

        ++result.completed_logical_frames;
        result.last_completed_sequence = completed_sequence;
        result.last_completed_flags = completed_flags;

        read_offset_frames_.store(0U, std::memory_order_relaxed);
        read_position_.store(read, std::memory_order_release);
    }

    read_offset_frames_.store(slot_offset, std::memory_order_relaxed);
    result.silence_frames = frame_count - result.queue_frames;

    queue_output_frames_.fetch_add(result.queue_frames, std::memory_order_relaxed);
    if (result.silence_frames != 0U) {
        silence_output_frames_.fetch_add(result.silence_frames, std::memory_order_relaxed);
        underrun_callbacks_.fetch_add(1U, std::memory_order_relaxed);
    }

    return result;
}

PcmPlaybackDiscardResult PcmPlaybackQueue::discard_pending() noexcept {
    PcmPlaybackDiscardResult result{};
    const std::uint64_t write = write_position_.load(std::memory_order_acquire);
    const std::uint64_t read = read_position_.load(std::memory_order_relaxed);
    const std::uint32_t offset = read_offset_frames_.load(std::memory_order_relaxed);

    const std::uint64_t queued = write - read;
    result.discarded_logical_frames = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(queued, pcm_playback_queue_capacity));
    result.discarded_partial_frames = offset != 0U ? 1U : 0U;

    read_offset_frames_.store(0U, std::memory_order_relaxed);
    read_position_.store(write, std::memory_order_release);

    discarded_logical_frames_.fetch_add(
        result.discarded_logical_frames,
        std::memory_order_relaxed);
    discarded_partial_frames_.fetch_add(
        result.discarded_partial_frames,
        std::memory_order_relaxed);
    return result;
}

void PcmPlaybackQueue::reset() noexcept {
    write_position_.store(0U, std::memory_order_relaxed);
    read_position_.store(0U, std::memory_order_relaxed);
    read_offset_frames_.store(0U, std::memory_order_relaxed);
    enqueued_logical_frames_.store(0U, std::memory_order_relaxed);
    consumed_logical_frames_.store(0U, std::memory_order_relaxed);
    queue_output_frames_.store(0U, std::memory_order_relaxed);
    silence_output_frames_.store(0U, std::memory_order_relaxed);
    underrun_callbacks_.store(0U, std::memory_order_relaxed);
    dropped_logical_frames_.store(0U, std::memory_order_relaxed);
    discarded_logical_frames_.store(0U, std::memory_order_relaxed);
    discarded_partial_frames_.store(0U, std::memory_order_relaxed);
    discontinuity_logical_frames_.store(0U, std::memory_order_relaxed);
    last_enqueued_sequence_.store(0U, std::memory_order_relaxed);
    last_consumed_sequence_.store(0U, std::memory_order_relaxed);
    last_consumed_flags_.store(0U, std::memory_order_relaxed);
}

PcmPlaybackQueueSnapshot PcmPlaybackQueue::snapshot() const noexcept {
    PcmPlaybackQueueSnapshot result{};
    const std::uint64_t write = write_position_.load(std::memory_order_acquire);
    const std::uint64_t read = read_position_.load(std::memory_order_acquire);
    const std::uint32_t offset = read_offset_frames_.load(std::memory_order_relaxed);
    const std::uint64_t queued = std::min<std::uint64_t>(
        write - read,
        pcm_playback_queue_capacity);

    result.queued_logical_frames = static_cast<std::uint32_t>(queued);
    if (queued != 0U) {
        const std::uint64_t queued_frames =
            queued * logical_pcm_frame_frames - offset;
        result.queued_pcm_frames = static_cast<std::uint32_t>(queued_frames);
    }
    result.enqueued_logical_frames =
        enqueued_logical_frames_.load(std::memory_order_relaxed);
    result.consumed_logical_frames =
        consumed_logical_frames_.load(std::memory_order_relaxed);
    result.queue_output_frames =
        queue_output_frames_.load(std::memory_order_relaxed);
    result.silence_output_frames =
        silence_output_frames_.load(std::memory_order_relaxed);
    result.underrun_callbacks =
        underrun_callbacks_.load(std::memory_order_relaxed);
    result.dropped_logical_frames =
        dropped_logical_frames_.load(std::memory_order_relaxed);
    result.discarded_logical_frames =
        discarded_logical_frames_.load(std::memory_order_relaxed);
    result.discarded_partial_frames =
        discarded_partial_frames_.load(std::memory_order_relaxed);
    result.discontinuity_logical_frames =
        discontinuity_logical_frames_.load(std::memory_order_relaxed);
    result.last_enqueued_sequence =
        last_enqueued_sequence_.load(std::memory_order_relaxed);
    result.last_consumed_sequence =
        last_consumed_sequence_.load(std::memory_order_relaxed);
    result.last_consumed_flags =
        last_consumed_flags_.load(std::memory_order_relaxed);
    return result;
}

std::string_view to_string(PcmPlaybackQueueError error) noexcept {
    switch (error) {
    case PcmPlaybackQueueError::none:
        return "none";
    case PcmPlaybackQueueError::invalid_sample_count:
        return "invalid_sample_count";
    case PcmPlaybackQueueError::zero_sequence_number:
        return "zero_sequence_number";
    case PcmPlaybackQueueError::unknown_flags:
        return "unknown_flags";
    case PcmPlaybackQueueError::queue_full:
        return "queue_full";
    case PcmPlaybackQueueError::zero_output_frame_count:
        return "zero_output_frame_count";
    case PcmPlaybackQueueError::invalid_output_sample_count:
        return "invalid_output_sample_count";
    }

    return "unknown_pcm_playback_queue_error";
}

} // namespace wiretone::playback
