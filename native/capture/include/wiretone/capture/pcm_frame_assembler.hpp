#pragma once

#include "wiretone/capture/captured_packet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wiretone::capture {

inline constexpr std::uint32_t logical_pcm_frame_samples = 960U;
inline constexpr std::size_t logical_pcm_frame_bytes =
    normalized_pcm_size_for_frames(logical_pcm_frame_samples);

struct PcmFrameView {
    std::span<const std::byte> data{};
    std::uint64_t sequence_number{0};
    std::uint32_t flags{0};
    std::uint64_t device_position_frames{0};
    std::uint64_t qpc_position_100ns{0};
};

using PcmFrameSink = void (*)(const PcmFrameView& frame, void* context) noexcept;

enum class PcmFrameAssemblerError {
    none = 0,
    zero_frame_count,
    unknown_flags,
    invalid_pcm_size,
    sink_missing,
};

struct PcmFrameAssemblyResult {
    PcmFrameAssemblerError error{PcmFrameAssemblerError::none};
    std::uint32_t input_frames{0};
    std::uint32_t completed_frames{0};
    std::uint32_t dropped_partial_frames{0};

    [[nodiscard]] bool ok() const noexcept {
        return error == PcmFrameAssemblerError::none;
    }
};

class PcmFrameAssembler {
public:
    [[nodiscard]] PcmFrameAssemblyResult accept_packet(
        std::span<const std::byte> normalized_pcm,
        std::uint32_t frame_count,
        std::uint32_t flags,
        std::uint64_t device_position_frames,
        std::uint64_t qpc_position_100ns,
        PcmFrameSink sink,
        void* sink_context) noexcept;

    [[nodiscard]] bool discard_partial() noexcept;
    [[nodiscard]] bool reset() noexcept;

    [[nodiscard]] std::uint32_t pending_frame_count() const noexcept;
    [[nodiscard]] std::uint64_t next_sequence_number() const noexcept;

private:
    void clear_partial() noexcept;

    std::array<std::byte, logical_pcm_frame_bytes> storage_{};
    std::uint32_t pending_frames_{0};
    std::uint32_t pending_flags_{0};
    bool pending_all_silent_{true};
    std::uint64_t pending_device_position_frames_{0};
    std::uint64_t pending_qpc_position_100ns_{0};
    std::uint64_t next_sequence_number_{1};
};

[[nodiscard]] std::string_view to_string(PcmFrameAssemblerError error) noexcept;

} // namespace wiretone::capture
