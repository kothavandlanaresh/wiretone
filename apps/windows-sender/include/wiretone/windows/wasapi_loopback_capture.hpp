#pragma once

#include "wiretone/capture/lifecycle.hpp"

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
    default_render_endpoint_failed,
    endpoint_id_failed,
    endpoint_property_store_failed,
    endpoint_name_failed,
    audio_client_activation_failed,
    mix_format_failed,
    loopback_initialization_failed,
    capture_service_failed,
    buffer_size_failed,
    stream_start_failed,
    stream_stop_failed,
};

struct WasapiCaptureSnapshot {
    capture::CaptureState state{capture::CaptureState::idle};
    WasapiCaptureError error{WasapiCaptureError::none};
    std::int32_t native_result{0};
    std::string endpoint_name{};
    std::string endpoint_id{};
    WasapiMixFormat mix_format{};
    std::uint32_t endpoint_buffer_frames{0};
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
    [[nodiscard]] bool stop() noexcept;

    [[nodiscard]] WasapiCaptureSnapshot snapshot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string_view to_string(WasapiSampleKind kind) noexcept;
[[nodiscard]] std::string_view to_string(WasapiCaptureError error) noexcept;

} // namespace wiretone::windows
