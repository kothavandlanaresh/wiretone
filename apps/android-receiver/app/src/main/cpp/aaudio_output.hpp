#pragma once

#include "wiretone/playback/lifecycle.hpp"

#include <aaudio/AAudio.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace wiretone::android {

enum class AndroidAudioOutputError {
    none = 0,
    invalid_state,
    builder_creation_failed,
    stream_open_failed,
    unsupported_callback_format,
    stream_start_failed,
    stream_stop_failed,
    callback_failed,
    stream_disconnected,
};

struct AndroidAudioOutputSnapshot {
    playback::PlaybackState state{playback::PlaybackState::idle};
    AndroidAudioOutputError error{AndroidAudioOutputError::none};
    std::int32_t native_result{AAUDIO_OK};
    std::int32_t sample_rate{0};
    std::int32_t channel_count{0};
    aaudio_format_t format{AAUDIO_FORMAT_UNSPECIFIED};
    aaudio_sharing_mode_t sharing_mode{AAUDIO_SHARING_MODE_SHARED};
    aaudio_performance_mode_t performance_mode{AAUDIO_PERFORMANCE_MODE_NONE};
    std::int32_t frames_per_burst{0};
    std::int32_t buffer_capacity_frames{0};
    std::int32_t buffer_size_frames{0};
    std::int32_t device_id{AAUDIO_UNSPECIFIED};
    bool requested_contract_granted{false};
    std::uint64_t callback_count{0};
    std::uint64_t rendered_frames{0};
    std::uint64_t disconnect_events{0};
    std::int32_t underrun_count{0};
};

class AndroidAudioOutput {
public:
    AndroidAudioOutput() = default;
    ~AndroidAudioOutput();

    AndroidAudioOutput(const AndroidAudioOutput&) = delete;
    AndroidAudioOutput& operator=(const AndroidAudioOutput&) = delete;
    AndroidAudioOutput(AndroidAudioOutput&&) = delete;
    AndroidAudioOutput& operator=(AndroidAudioOutput&&) = delete;

    [[nodiscard]] bool open() noexcept;
    [[nodiscard]] bool start() noexcept;
    [[nodiscard]] bool stop() noexcept;
    void close() noexcept;

    [[nodiscard]] AndroidAudioOutputSnapshot snapshot() const noexcept;

private:
    static aaudio_data_callback_result_t data_callback(
        AAudioStream* stream,
        void* user_data,
        void* audio_data,
        std::int32_t frame_count) noexcept;
    static void error_callback(
        AAudioStream* stream,
        void* user_data,
        aaudio_result_t error) noexcept;

    [[nodiscard]] bool fail(AndroidAudioOutputError error, aaudio_result_t result) noexcept;
    void synchronize_async_error() noexcept;
    void clear_error() noexcept;

    playback::PlaybackLifecycle lifecycle_{};
    AndroidAudioOutputError error_{AndroidAudioOutputError::none};
    aaudio_result_t native_result_{AAUDIO_OK};
    AAudioStream* stream_{nullptr};
    std::int32_t sample_rate_{0};
    std::int32_t channel_count_{0};
    aaudio_format_t format_{AAUDIO_FORMAT_UNSPECIFIED};
    aaudio_sharing_mode_t sharing_mode_{AAUDIO_SHARING_MODE_SHARED};
    aaudio_performance_mode_t performance_mode_{AAUDIO_PERFORMANCE_MODE_NONE};
    std::int32_t frames_per_burst_{0};
    std::int32_t buffer_capacity_frames_{0};
    std::int32_t buffer_size_frames_{0};
    std::int32_t device_id_{AAUDIO_UNSPECIFIED};
    std::size_t callback_bytes_per_frame_{0};
    bool requested_contract_granted_{false};
    std::atomic<std::uint64_t> callback_count_{0};
    std::atomic<std::uint64_t> rendered_frames_{0};
    std::atomic<std::uint64_t> disconnect_events_{0};
    std::atomic<aaudio_result_t> async_error_{AAUDIO_OK};
};

[[nodiscard]] std::string_view to_string(AndroidAudioOutputError error) noexcept;
[[nodiscard]] std::string_view format_to_string(aaudio_format_t format) noexcept;
[[nodiscard]] std::string_view sharing_mode_to_string(aaudio_sharing_mode_t mode) noexcept;
[[nodiscard]] std::string_view performance_mode_to_string(aaudio_performance_mode_t mode) noexcept;

} // namespace wiretone::android
