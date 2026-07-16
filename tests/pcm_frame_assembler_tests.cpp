#include "wiretone/capture/pcm_frame_assembler.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

namespace {

int fail(std::string_view message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

struct CapturedFrame {
    std::array<std::byte, wiretone::capture::logical_pcm_frame_bytes> data{};
    std::uint64_t sequence_number{0};
    std::uint32_t flags{0};
    std::uint64_t device_position_frames{0};
    std::uint64_t qpc_position_100ns{0};
};

struct FrameCollector {
    std::array<CapturedFrame, 4> frames{};
    std::size_t count{0};
};

void collect_frame(
    const wiretone::capture::PcmFrameView& frame,
    void* context) noexcept {
    auto& collector = *static_cast<FrameCollector*>(context);
    if (collector.count >= collector.frames.size()) {
        return;
    }

    auto& destination = collector.frames[collector.count];
    for (std::size_t index = 0; index < destination.data.size(); ++index) {
        destination.data[index] = frame.data[index];
    }
    destination.sequence_number = frame.sequence_number;
    destination.flags = frame.flags;
    destination.device_position_frames = frame.device_position_frames;
    destination.qpc_position_100ns = frame.qpc_position_100ns;
    ++collector.count;
}

[[nodiscard]] std::array<std::byte, wiretone::capture::logical_pcm_frame_bytes>
make_frame_bytes(std::byte seed) {
    std::array<std::byte, wiretone::capture::logical_pcm_frame_bytes> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::byte>(
            (std::to_integer<unsigned int>(seed) +
             static_cast<unsigned int>(index)) &
            0xFFU);
    }
    return result;
}

[[nodiscard]] bool frame_matches(
    const CapturedFrame& frame,
    std::span<const std::byte> expected) {
    if (expected.size() != frame.data.size()) {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (frame.data[index] != expected[index]) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    using namespace wiretone::capture;

    {
        PcmFrameAssembler assembler;
        FrameCollector collector;
        const auto input = make_frame_bytes(std::byte{0x11});

        const auto result = assembler.accept_packet(
            input,
            logical_pcm_frame_samples,
            0U,
            10'000U,
            20'000U,
            collect_frame,
            &collector);

        if (!result.ok() || result.completed_frames != 1U ||
            collector.count != 1U ||
            assembler.pending_frame_count() != 0U) {
            return fail("exact 960-frame packet was not assembled");
        }
        if (!frame_matches(collector.frames[0], input) ||
            collector.frames[0].sequence_number != 1U ||
            collector.frames[0].device_position_frames != 10'000U ||
            collector.frames[0].qpc_position_100ns != 20'000U ||
            collector.frames[0].flags != 0U) {
            return fail("exact frame bytes or metadata changed");
        }
    }

    {
        PcmFrameAssembler assembler;
        FrameCollector collector;
        const auto complete = make_frame_bytes(std::byte{0x22});
        const auto first = std::span<const std::byte>(complete).first(
            normalized_pcm_size_for_frames(480U));
        const auto second = std::span<const std::byte>(complete).subspan(
            normalized_pcm_size_for_frames(480U));

        const auto first_result = assembler.accept_packet(
            first,
            480U,
            0U,
            5'000U,
            9'000U,
            collect_frame,
            &collector);
        const auto second_result = assembler.accept_packet(
            second,
            480U,
            0U,
            5'480U,
            109'000U,
            collect_frame,
            &collector);

        if (!first_result.ok() || first_result.completed_frames != 0U ||
            !second_result.ok() || second_result.completed_frames != 1U ||
            collector.count != 1U ||
            !frame_matches(collector.frames[0], complete)) {
            return fail("split packets did not form one exact frame");
        }
        if (collector.frames[0].device_position_frames != 5'000U ||
            collector.frames[0].qpc_position_100ns != 9'000U) {
            return fail("split-frame starting timestamp changed");
        }
    }

    {
        PcmFrameAssembler assembler;
        FrameCollector collector;
        std::array<std::byte, logical_pcm_frame_bytes * 2U> input{};
        for (std::size_t index = 0; index < input.size(); ++index) {
            input[index] = static_cast<std::byte>(index & 0xFFU);
        }

        const auto result = assembler.accept_packet(
            input,
            logical_pcm_frame_samples * 2U,
            0U,
            100U,
            200U,
            collect_frame,
            &collector);

        if (!result.ok() || result.completed_frames != 2U ||
            collector.count != 2U ||
            collector.frames[0].sequence_number != 1U ||
            collector.frames[1].sequence_number != 2U) {
            return fail("combined packet did not emit two frames");
        }
        if (collector.frames[1].device_position_frames != 1'060U ||
            collector.frames[1].qpc_position_100ns != 200'200U) {
            return fail("second combined-frame timestamp offset changed");
        }
        if (!frame_matches(
                collector.frames[0],
                std::span<const std::byte>(input).first(logical_pcm_frame_bytes)) ||
            !frame_matches(
                collector.frames[1],
                std::span<const std::byte>(input).subspan(logical_pcm_frame_bytes))) {
            return fail("combined-frame byte order changed");
        }
    }

    {
        PcmFrameAssembler assembler;
        FrameCollector collector;
        std::array<std::byte, normalized_pcm_size_for_frames(300U)> partial{};
        partial.fill(std::byte{0x41});
        const auto complete = make_frame_bytes(std::byte{0x55});

        const auto partial_result = assembler.accept_packet(
            partial,
            300U,
            0U,
            1U,
            2U,
            collect_frame,
            &collector);
        const auto discontinuity_result = assembler.accept_packet(
            complete,
            logical_pcm_frame_samples,
            captured_packet_flag_discontinuity,
            8'000U,
            9'000U,
            collect_frame,
            &collector);

        if (!partial_result.ok() ||
            !discontinuity_result.ok() ||
            discontinuity_result.dropped_partial_frames != 1U ||
            discontinuity_result.completed_frames != 1U ||
            collector.count != 1U) {
            return fail("discontinuity did not discard the partial frame");
        }
        if ((collector.frames[0].flags & captured_packet_flag_discontinuity) == 0U ||
            !frame_matches(collector.frames[0], complete)) {
            return fail("discontinuity was not carried to the next completed frame");
        }
    }

    {
        PcmFrameAssembler assembler;
        FrameCollector collector;
        std::array<std::byte, normalized_pcm_size_for_frames(480U)> zeros{};

        const auto first = assembler.accept_packet(
            zeros,
            480U,
            captured_packet_flag_silence,
            100U,
            200U,
            collect_frame,
            &collector);
        const auto second = assembler.accept_packet(
            zeros,
            480U,
            captured_packet_flag_silence |
                captured_packet_flag_timestamp_error,
            580U,
            100'200U,
            collect_frame,
            &collector);

        if (!first.ok() || !second.ok() || collector.count != 1U) {
            return fail("silent split frame was not emitted");
        }
        const auto flags = collector.frames[0].flags;
        if ((flags & captured_packet_flag_silence) == 0U ||
            (flags & captured_packet_flag_timestamp_error) == 0U) {
            return fail("silence or timestamp error was not propagated");
        }
    }

    {
        PcmFrameAssembler assembler;
        FrameCollector collector;
        std::array<std::byte, normalized_pcm_size_for_frames(480U)> zeros{};
        std::array<std::byte, normalized_pcm_size_for_frames(480U)> audible{};
        audible.fill(std::byte{0x33});

        (void)assembler.accept_packet(
            zeros,
            480U,
            captured_packet_flag_silence,
            0U,
            0U,
            collect_frame,
            &collector);
        (void)assembler.accept_packet(
            audible,
            480U,
            0U,
            480U,
            100'000U,
            collect_frame,
            &collector);

        if (collector.count != 1U ||
            (collector.frames[0].flags & captured_packet_flag_silence) != 0U) {
            return fail("mixed silent and audible input was marked fully silent");
        }
    }

    {
        PcmFrameAssembler assembler;
        FrameCollector collector;
        const auto first_frame = make_frame_bytes(std::byte{0x71});
        const auto second_frame = make_frame_bytes(std::byte{0x72});
        std::array<std::byte, normalized_pcm_size_for_frames(240U)> partial{};

        const auto first_result = assembler.accept_packet(
            first_frame,
            logical_pcm_frame_samples,
            0U,
            1'000U,
            2'000U,
            collect_frame,
            &collector);
        const auto partial_result = assembler.accept_packet(
            partial,
            240U,
            0U,
            1'960U,
            202'000U,
            collect_frame,
            &collector);

        if (!first_result.ok() || !partial_result.ok() ||
            collector.count != 1U || assembler.pending_frame_count() != 240U ||
            assembler.next_sequence_number() != 2U) {
            return fail("partial discard setup was not assembled correctly");
        }

        if (!assembler.discard_partial() || assembler.pending_frame_count() != 0U ||
            assembler.next_sequence_number() != 2U) {
            return fail("partial discard did not preserve sequence continuity");
        }

        const auto second_result = assembler.accept_packet(
            second_frame,
            logical_pcm_frame_samples,
            captured_packet_flag_discontinuity,
            3'000U,
            4'000U,
            collect_frame,
            &collector);
        if (!second_result.ok() || collector.count != 2U ||
            collector.frames[1].sequence_number != 2U ||
            (collector.frames[1].flags & captured_packet_flag_discontinuity) == 0U) {
            return fail("post-discard frame did not preserve sequence or discontinuity");
        }
    }

    {
        PcmFrameAssembler assembler;
        FrameCollector collector;
        std::array<std::byte, normalized_pcm_size_for_frames(100U)> partial{};

        const auto good = assembler.accept_packet(
            partial,
            100U,
            0U,
            0U,
            0U,
            collect_frame,
            &collector);
        if (!good.ok() || assembler.pending_frame_count() != 100U) {
            return fail("partial state was not retained before reset");
        }
        if (!assembler.reset() || assembler.pending_frame_count() != 0U ||
            assembler.next_sequence_number() != 1U) {
            return fail("reset did not deterministically clear assembler state");
        }

        const auto zero_result = assembler.accept_packet(
            {},
            0U,
            0U,
            0U,
            0U,
            collect_frame,
            &collector);
        if (zero_result.error != PcmFrameAssemblerError::zero_frame_count) {
            return fail("zero-frame input was not rejected");
        }

        const auto unknown_flags = assembler.accept_packet(
            partial,
            100U,
            0x8000'0000U,
            0U,
            0U,
            collect_frame,
            &collector);
        if (unknown_flags.error != PcmFrameAssemblerError::unknown_flags) {
            return fail("unknown flags were not rejected");
        }

        const auto invalid_size = assembler.accept_packet(
            std::span<const std::byte>(partial).first(partial.size() - 1U),
            100U,
            0U,
            0U,
            0U,
            collect_frame,
            &collector);
        if (invalid_size.error != PcmFrameAssemblerError::invalid_pcm_size) {
            return fail("invalid PCM size was not rejected");
        }

        const auto missing_sink = assembler.accept_packet(
            partial,
            100U,
            0U,
            0U,
            0U,
            nullptr,
            nullptr);
        if (missing_sink.error != PcmFrameAssemblerError::sink_missing) {
            return fail("missing frame sink was not rejected");
        }
    }

    if (logical_pcm_frame_samples != 960U ||
        logical_pcm_frame_bytes != 3'840U ||
        to_string(PcmFrameAssemblerError::invalid_pcm_size) != "invalid_pcm_size") {
        return fail("PCM frame constants or diagnostics changed");
    }

    std::cout << "PASS: WireTone PCM frame assembler tests\n";
    return 0;
}
