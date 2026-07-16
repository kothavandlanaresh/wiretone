#include "wiretone/playback/pcm_queue.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <thread>

namespace {

using Frame = std::array<std::int16_t, wiretone::playback::logical_pcm_frame_samples>;

int fail(std::string_view message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

Frame make_frame(std::int16_t left, std::int16_t right) {
    Frame frame{};
    for (std::size_t index = 0; index < frame.size(); index += 2U) {
        frame[index] = left;
        frame[index + 1U] = right;
    }
    return frame;
}

bool all_samples(
    std::span<const std::int16_t> values,
    std::int16_t left,
    std::int16_t right) {
    if ((values.size() % 2U) != 0U) {
        return false;
    }
    for (std::size_t index = 0; index < values.size(); index += 2U) {
        if (values[index] != left || values[index + 1U] != right) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    using namespace wiretone::playback;

    PcmPlaybackQueue queue;
    const auto frame_one = make_frame(101, -101);
    const auto frame_two = make_frame(202, -202);

    const auto invalid_size = queue.enqueue(PcmPlaybackFrameView{
        std::span<const std::int16_t>(frame_one.data(), frame_one.size() - 1U),
        1U,
        0U,
    });
    if (invalid_size.error != PcmPlaybackQueueError::invalid_sample_count) {
        return fail("invalid logical-frame sample count was accepted");
    }
    if (queue.enqueue(PcmPlaybackFrameView{frame_one, 0U, 0U}).error !=
        PcmPlaybackQueueError::zero_sequence_number) {
        return fail("zero sequence number was accepted");
    }
    if (queue.enqueue(PcmPlaybackFrameView{frame_one, 1U, 0x80U}).error !=
        PcmPlaybackQueueError::unknown_flags) {
        return fail("unknown frame flags were accepted");
    }

    std::array<std::int16_t, 192U> short_output{};
    if (queue.render(short_output, 0U).error !=
        PcmPlaybackQueueError::zero_output_frame_count) {
        return fail("zero-frame render was accepted");
    }
    if (queue.render(short_output, 95U).error !=
        PcmPlaybackQueueError::invalid_output_sample_count) {
        return fail("mismatched output sample count was accepted");
    }

    const auto empty_render = queue.render(short_output, 96U);
    if (!empty_render.ok() || empty_render.queue_frames != 0U ||
        empty_render.silence_frames != 96U ||
        !all_samples(short_output, 0, 0)) {
        return fail("empty queue did not render exact silence");
    }
    auto snapshot = queue.snapshot();
    if (snapshot.underrun_callbacks != 1U ||
        snapshot.silence_output_frames != 96U) {
        return fail("empty queue underrun counters were incorrect");
    }

    queue.reset();
    if (!queue.enqueue(PcmPlaybackFrameView{
            frame_one,
            10U,
            pcm_frame_flag_discontinuity,
        }).ok() ||
        !queue.enqueue(PcmPlaybackFrameView{frame_two, 11U, 0U}).ok()) {
        return fail("valid FIFO frames were not enqueued");
    }

    const auto first_partial = queue.render(short_output, 96U);
    if (!first_partial.ok() || first_partial.queue_frames != 96U ||
        first_partial.silence_frames != 0U ||
        !all_samples(short_output, 101, -101)) {
        return fail("first partial callback did not preserve PCM order");
    }

    std::array<std::int16_t, logical_pcm_frame_samples> crossing_output{};
    const auto crossing = queue.render(crossing_output, logical_pcm_frame_frames);
    if (!crossing.ok() || crossing.completed_logical_frames != 1U ||
        crossing.last_completed_sequence != 10U ||
        crossing.last_completed_flags != pcm_frame_flag_discontinuity) {
        return fail("cross-frame callback metadata was incorrect");
    }
    const std::size_t first_part_samples =
        static_cast<std::size_t>(logical_pcm_frame_frames - 96U) * pcm_channel_count;
    if (!all_samples(
            std::span<const std::int16_t>(crossing_output.data(), first_part_samples),
            101,
            -101) ||
        !all_samples(
            std::span<const std::int16_t>(
                crossing_output.data() + static_cast<std::ptrdiff_t>(first_part_samples),
                crossing_output.size() - first_part_samples),
            202,
            -202)) {
        return fail("cross-frame callback changed FIFO PCM order");
    }

    std::array<std::int16_t, (logical_pcm_frame_frames - 96U) * pcm_channel_count>
        final_output{};
    const auto final_render = queue.render(final_output, logical_pcm_frame_frames - 96U);
    if (!final_render.ok() || final_render.completed_logical_frames != 1U ||
        final_render.last_completed_sequence != 11U ||
        !all_samples(final_output, 202, -202)) {
        return fail("second logical frame was not completed correctly");
    }
    snapshot = queue.snapshot();
    if (snapshot.queued_logical_frames != 0U ||
        snapshot.consumed_logical_frames != 2U ||
        snapshot.discontinuity_logical_frames != 1U ||
        snapshot.last_consumed_sequence != 11U) {
        return fail("FIFO queue snapshot counters were incorrect");
    }

    queue.reset();
    for (std::uint32_t index = 0U; index < pcm_playback_queue_capacity; ++index) {
        if (!queue.enqueue(PcmPlaybackFrameView{
                frame_one,
                static_cast<std::uint64_t>(index) + 1U,
                0U,
            }).ok()) {
            return fail("queue reached full state too early");
        }
    }
    const auto overflow = queue.enqueue(PcmPlaybackFrameView{frame_two, 100U, 0U});
    if (overflow.error != PcmPlaybackQueueError::queue_full) {
        return fail("drop-newest overflow policy did not reject newest frame");
    }
    snapshot = queue.snapshot();
    if (snapshot.queued_logical_frames != pcm_playback_queue_capacity ||
        snapshot.dropped_logical_frames != 1U ||
        snapshot.last_enqueued_sequence != pcm_playback_queue_capacity) {
        return fail("overflow snapshot did not preserve queued frames");
    }

    queue.reset();
    if (!queue.enqueue(PcmPlaybackFrameView{frame_one, 20U, 0U}).ok() ||
        !queue.enqueue(PcmPlaybackFrameView{frame_two, 21U, 0U}).ok()) {
        return fail("discard test setup failed");
    }
    if (!queue.render(short_output, 96U).ok()) {
        return fail("discard test partial render failed");
    }
    const auto discarded = queue.discard_pending();
    if (discarded.discarded_logical_frames != 2U ||
        discarded.discarded_partial_frames != 1U) {
        return fail("partial discard result was incorrect");
    }
    snapshot = queue.snapshot();
    if (snapshot.queued_logical_frames != 0U ||
        snapshot.discarded_logical_frames != 2U ||
        snapshot.discarded_partial_frames != 1U) {
        return fail("partial discard counters were incorrect");
    }

    queue.reset();
    for (std::uint32_t cycle = 0U; cycle < pcm_playback_queue_capacity + 5U; ++cycle) {
        const std::uint64_t sequence = static_cast<std::uint64_t>(cycle) + 1U;
        if (!queue.enqueue(PcmPlaybackFrameView{frame_one, sequence, 0U}).ok()) {
            return fail("ring wrap enqueue failed");
        }
        std::array<std::int16_t, logical_pcm_frame_samples> output{};
        const auto rendered = queue.render(output, logical_pcm_frame_frames);
        if (!rendered.ok() || rendered.last_completed_sequence != sequence) {
            return fail("ring wrap render changed sequence order");
        }
    }

    queue.reset();
    snapshot = queue.snapshot();
    if (snapshot.enqueued_logical_frames != 0U ||
        snapshot.consumed_logical_frames != 0U ||
        snapshot.underrun_callbacks != 0U ||
        snapshot.last_consumed_sequence != 0U) {
        return fail("queue reset did not clear counters");
    }

    constexpr std::uint64_t concurrent_frame_count = 2'000U;
    std::atomic<bool> concurrent_failed{false};
    std::thread producer([&queue, &concurrent_failed]() {
        for (std::uint64_t sequence = 1U;
             sequence <= concurrent_frame_count;
             ++sequence) {
            const auto sample = static_cast<std::int16_t>(
                static_cast<std::int32_t>(sequence % 20'000U) + 1);
            const auto frame = make_frame(sample, static_cast<std::int16_t>(-sample));
            for (;;) {
                const auto enqueued = queue.enqueue(PcmPlaybackFrameView{
                    frame,
                    sequence,
                    0U,
                });
                if (enqueued.ok()) {
                    break;
                }
                if (enqueued.error != PcmPlaybackQueueError::queue_full) {
                    concurrent_failed.store(true, std::memory_order_relaxed);
                    return;
                }
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&queue, &concurrent_failed]() {
        std::uint64_t consumed = 0U;
        std::array<std::int16_t, logical_pcm_frame_samples> output{};
        while (consumed < concurrent_frame_count) {
            const auto rendered = queue.render(output, logical_pcm_frame_frames);
            if (!rendered.ok()) {
                concurrent_failed.store(true, std::memory_order_relaxed);
                return;
            }
            if (rendered.completed_logical_frames == 0U) {
                std::this_thread::yield();
                continue;
            }
            if (rendered.completed_logical_frames != 1U ||
                rendered.last_completed_sequence != consumed + 1U) {
                concurrent_failed.store(true, std::memory_order_relaxed);
                return;
            }
            const auto sample = static_cast<std::int16_t>(
                static_cast<std::int32_t>((consumed + 1U) % 20'000U) + 1);
            if (!all_samples(output, sample, static_cast<std::int16_t>(-sample))) {
                concurrent_failed.store(true, std::memory_order_relaxed);
                return;
            }
            ++consumed;
        }
    });

    producer.join();
    consumer.join();
    if (concurrent_failed.load(std::memory_order_relaxed)) {
        return fail("concurrent SPSC FIFO validation failed");
    }
    snapshot = queue.snapshot();
    if (snapshot.consumed_logical_frames != concurrent_frame_count ||
        snapshot.last_consumed_sequence != concurrent_frame_count) {
        return fail("concurrent SPSC counters were incorrect");
    }

    if (to_string(PcmPlaybackQueueError::queue_full) != "queue_full") {
        return fail("queue diagnostic text changed unexpectedly");
    }

    std::cout << "PASS: WireTone PCM playback queue tests\n";
    return 0;
}
