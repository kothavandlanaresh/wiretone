#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wiretone::playback {

inline constexpr std::uint32_t pcm_sample_rate = 48'000U;
inline constexpr std::uint32_t pcm_channel_count = 2U;
inline constexpr std::uint32_t logical_pcm_frame_frames = 960U;
inline constexpr std::size_t logical_pcm_frame_samples =
    static_cast<std::size_t>(logical_pcm_frame_frames) * pcm_channel_count;
inline constexpr std::uint32_t pcm_playback_queue_capacity = 32U;

inline constexpr std::uint32_t pcm_frame_flag_discontinuity = 1U << 0U;
inline constexpr std::uint32_t pcm_frame_known_flag_mask =
    pcm_frame_flag_discontinuity;

struct PcmPlaybackFrameView {
    std::span<const std::int16_t> samples{};
    std::uint64_t sequence_number{0};
    std::uint32_t flags{0};
};

enum class PcmPlaybackQueueError {
    none = 0,
    invalid_sample_count,
    zero_sequence_number,
    unknown_flags,
    queue_full,
    zero_output_frame_count,
    invalid_output_sample_count,
};

struct PcmPlaybackEnqueueResult {
    PcmPlaybackQueueError error{PcmPlaybackQueueError::none};
    std::uint32_t queued_logical_frames{0};

    [[nodiscard]] bool ok() const noexcept {
        return error == PcmPlaybackQueueError::none;
    }
};

struct PcmPlaybackRenderResult {
    PcmPlaybackQueueError error{PcmPlaybackQueueError::none};
    std::uint32_t requested_frames{0};
    std::uint32_t queue_frames{0};
    std::uint32_t silence_frames{0};
    std::uint32_t completed_logical_frames{0};
    std::uint64_t last_completed_sequence{0};
    std::uint32_t last_completed_flags{0};

    [[nodiscard]] bool ok() const noexcept {
        return error == PcmPlaybackQueueError::none;
    }
};

struct PcmPlaybackDiscardResult {
    std::uint32_t discarded_logical_frames{0};
    std::uint32_t discarded_partial_frames{0};
};

struct PcmPlaybackQueueSnapshot {
    std::uint32_t queued_logical_frames{0};
    std::uint32_t queued_pcm_frames{0};
    std::uint64_t enqueued_logical_frames{0};
    std::uint64_t consumed_logical_frames{0};
    std::uint64_t queue_output_frames{0};
    std::uint64_t silence_output_frames{0};
    std::uint64_t underrun_callbacks{0};
    std::uint64_t dropped_logical_frames{0};
    std::uint64_t discarded_logical_frames{0};
    std::uint64_t discarded_partial_frames{0};
    std::uint64_t discontinuity_logical_frames{0};
    std::uint64_t last_enqueued_sequence{0};
    std::uint64_t last_consumed_sequence{0};
    std::uint32_t last_consumed_flags{0};
};

class PcmPlaybackQueue {
public:
    [[nodiscard]] PcmPlaybackEnqueueResult enqueue(
        const PcmPlaybackFrameView& frame) noexcept;

    [[nodiscard]] PcmPlaybackRenderResult render(
        std::span<std::int16_t> output,
        std::uint32_t frame_count) noexcept;

    [[nodiscard]] PcmPlaybackDiscardResult discard_pending() noexcept;
    void reset() noexcept;

    [[nodiscard]] PcmPlaybackQueueSnapshot snapshot() const noexcept;

private:
    struct Slot {
        std::array<std::int16_t, logical_pcm_frame_samples> samples{};
        std::uint64_t sequence_number{0};
        std::uint32_t flags{0};
    };

    std::array<Slot, pcm_playback_queue_capacity> slots_{};
    std::atomic<std::uint64_t> write_position_{0};
    std::atomic<std::uint64_t> read_position_{0};
    std::atomic<std::uint32_t> read_offset_frames_{0};

    std::atomic<std::uint64_t> enqueued_logical_frames_{0};
    std::atomic<std::uint64_t> consumed_logical_frames_{0};
    std::atomic<std::uint64_t> queue_output_frames_{0};
    std::atomic<std::uint64_t> silence_output_frames_{0};
    std::atomic<std::uint64_t> underrun_callbacks_{0};
    std::atomic<std::uint64_t> dropped_logical_frames_{0};
    std::atomic<std::uint64_t> discarded_logical_frames_{0};
    std::atomic<std::uint64_t> discarded_partial_frames_{0};
    std::atomic<std::uint64_t> discontinuity_logical_frames_{0};
    std::atomic<std::uint64_t> last_enqueued_sequence_{0};
    std::atomic<std::uint64_t> last_consumed_sequence_{0};
    std::atomic<std::uint32_t> last_consumed_flags_{0};
};

[[nodiscard]] std::string_view to_string(PcmPlaybackQueueError error) noexcept;

} // namespace wiretone::playback
