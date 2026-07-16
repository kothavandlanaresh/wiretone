#pragma once

#include "wiretone/capture/captured_packet.hpp"
#include "wiretone/capture/lifecycle.hpp"
#include "wiretone/capture/pcm_frame_assembler.hpp"
#include "wiretone/capture/recovery.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace wiretone::windows {

enum class WasapiSampleKind {
    unknown = 0,
    integer_pcm,
    floating_point,
};

struct WasapiMixFormat {
    std::uint32_t sample_rate{0};
    std::uint16_t channel_count{0};
    std::uint16_t container_bits_per_sample{0};
    std::uint16_t valid_bits_per_sample{0};
    std::uint32_t channel_mask{0};
    WasapiSampleKind sample_kind{WasapiSampleKind::unknown};
    bool extensible{false};
};

enum class WasapiCaptureError {
    none = 0,
    invalid_state,
    com_initialization_failed,
    device_enumerator_creation_failed,
    endpoint_notification_client_creation_failed,
    endpoint_notification_registration_failed,
    default_render_endpoint_failed,
    endpoint_id_failed,
    endpoint_property_store_failed,
    endpoint_name_failed,
    audio_client_activation_failed,
    mix_format_failed,
    loopback_initialization_failed,
    capture_service_failed,
    buffer_size_failed,
    unsupported_mix_format,
    stream_start_failed,
    stream_stop_failed,
    next_packet_size_failed,
    capture_buffer_failed,
    capture_buffer_release_failed,
    packet_size_mismatch,
    packet_frame_count_exceeds_buffer,
    unsupported_buffer_flags,
    packet_conversion_failed,
    frame_assembly_failed,
    device_invalidated,
    recovery_restart_failed,
    recovery_unsupported_mix_format,
};

struct WasapiCaptureSnapshot {
    capture::CaptureState state{capture::CaptureState::idle};
    WasapiCaptureError error{WasapiCaptureError::none};
    capture::CapturedPacketError conversion_error{capture::CapturedPacketError::none};
    capture::PcmFrameAssemblerError frame_assembler_error{
        capture::PcmFrameAssemblerError::none};
    std::int32_t native_result{0};
    std::string endpoint_name{};
    std::string endpoint_id{};
    WasapiMixFormat mix_format{};
    std::uint32_t endpoint_buffer_frames{0};
    bool normalization_supported{false};
    std::uint64_t packets_drained{0};
    std::uint64_t frames_drained{0};
    std::uint64_t pcm_bytes_produced{0};
    std::uint64_t silent_packets{0};
    std::uint64_t discontinuity_packets{0};
    std::uint64_t timestamp_error_packets{0};
    std::uint64_t empty_poll_count{0};
    std::uint64_t last_device_position_frames{0};
    std::uint64_t last_qpc_position_100ns{0};
    std::uint64_t completed_pcm_frames{0};
    std::uint64_t silent_pcm_frames{0};
    std::uint64_t discontinuity_pcm_frames{0};
    std::uint64_t timestamp_error_pcm_frames{0};
    std::uint64_t dropped_partial_pcm_frames{0};
    std::uint64_t last_completed_pcm_sequence{0};
    std::uint64_t last_completed_device_position_frames{0};
    std::uint64_t last_completed_qpc_position_100ns{0};
    capture::CaptureRecoverySnapshot recovery{};
    std::uint64_t recovery_discontinuity_pcm_frames{0};
};

class WasapiLoopbackCapture {
public:
    WasapiLoopbackCapture();
    ~WasapiLoopbackCapture();

    WasapiLoopbackCapture(const WasapiLoopbackCapture&) = delete;
    WasapiLoopbackCapture& operator=(const WasapiLoopbackCapture&) = delete;
    WasapiLoopbackCapture(WasapiLoopbackCapture&&) = delete;
    WasapiLoopbackCapture& operator=(WasapiLoopbackCapture&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    [[nodiscard]] bool start() noexcept;
    [[nodiscard]] bool drain_available() noexcept;
    [[nodiscard]] bool request_simulated_device_invalidation_for_probe() noexcept;
    [[nodiscard]] bool stop() noexcept;

    [[nodiscard]] WasapiCaptureSnapshot snapshot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string_view to_string(WasapiSampleKind kind) noexcept;
[[nodiscard]] std::string_view to_string(WasapiCaptureError error) noexcept;

} // namespace wiretone::windows
