#include "aaudio_output.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace wiretone::android {
namespace {

inline constexpr std::int32_t requested_sample_rate = 48'000;
inline constexpr std::int32_t requested_channel_count = 2;
inline constexpr std::uint32_t local_test_logical_frames = 24U;
inline constexpr double local_test_frequency_hz = 440.0;
inline constexpr double local_test_amplitude = 0.08;
inline constexpr std::uint64_t stop_wait_timeout_ns = 250'000'000ULL;
inline constexpr std::uint32_t maximum_stop_state_changes = 4U;
inline constexpr double pi = 3.14159265358979323846;

[[nodiscard]] bool is_stopped_state(aaudio_stream_state_t state) noexcept {
    return state == AAUDIO_STREAM_STATE_OPEN ||
        state == AAUDIO_STREAM_STATE_PAUSED ||
        state == AAUDIO_STREAM_STATE_FLUSHED ||
        state == AAUDIO_STREAM_STATE_STOPPED ||
        state == AAUDIO_STREAM_STATE_CLOSED ||
        state == AAUDIO_STREAM_STATE_DISCONNECTED;
}

[[nodiscard]] double fade_gain(
    std::uint64_t sample_index,
    std::uint64_t total_samples) noexcept {
    constexpr std::uint64_t fade_samples = 480U;
    if (sample_index < fade_samples) {
        return static_cast<double>(sample_index) /
            static_cast<double>(fade_samples);
    }
    const std::uint64_t samples_remaining = total_samples - sample_index - 1U;
    if (samples_remaining < fade_samples) {
        return static_cast<double>(samples_remaining) /
            static_cast<double>(fade_samples);
    }
    return 1.0;
}

} // namespace

AndroidAudioOutput::~AndroidAudioOutput() {
    close();
}

bool AndroidAudioOutput::open() noexcept {
    synchronize_async_error();
    if (lifecycle_.state() != playback::PlaybackState::idle || stream_ != nullptr) {
        return fail(AndroidAudioOutputError::invalid_state, AAUDIO_ERROR_INVALID_STATE);
    }

    clear_error();
    queue_.reset();
    next_local_test_sequence_ = 1U;
    local_test_batches_ = 0U;
    local_test_logical_frames_ = 0U;
    callback_count_.store(0U, std::memory_order_relaxed);
    rendered_frames_.store(0U, std::memory_order_relaxed);
    disconnect_events_.store(0U, std::memory_order_relaxed);
    async_error_.store(AAUDIO_OK, std::memory_order_relaxed);

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || builder == nullptr) {
        return fail(
            AndroidAudioOutputError::builder_creation_failed,
            result != AAUDIO_OK ? result : AAUDIO_ERROR_INTERNAL);
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, requested_sample_rate);
    AAudioStreamBuilder_setChannelCount(builder, requested_channel_count);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_MEDIA);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);
    AAudioStreamBuilder_setDataCallback(builder, &AndroidAudioOutput::data_callback, this);
    AAudioStreamBuilder_setErrorCallback(builder, &AndroidAudioOutput::error_callback, this);

    result = AAudioStreamBuilder_openStream(builder, &stream_);
    (void)AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK || stream_ == nullptr) {
        stream_ = nullptr;
        return fail(
            AndroidAudioOutputError::stream_open_failed,
            result != AAUDIO_OK ? result : AAUDIO_ERROR_INTERNAL);
    }

    sample_rate_ = AAudioStream_getSampleRate(stream_);
    channel_count_ = AAudioStream_getChannelCount(stream_);
    format_ = AAudioStream_getFormat(stream_);
    sharing_mode_ = AAudioStream_getSharingMode(stream_);
    performance_mode_ = AAudioStream_getPerformanceMode(stream_);
    frames_per_burst_ = AAudioStream_getFramesPerBurst(stream_);
    buffer_capacity_frames_ = AAudioStream_getBufferCapacityInFrames(stream_);
    buffer_size_frames_ = AAudioStream_getBufferSizeInFrames(stream_);
    device_id_ = AAudioStream_getDeviceId(stream_);

    requested_contract_granted_ =
        sample_rate_ == requested_sample_rate &&
        channel_count_ == requested_channel_count &&
        format_ == AAUDIO_FORMAT_PCM_I16;
    if (!requested_contract_granted_) {
        (void)AAudioStream_close(stream_);
        stream_ = nullptr;
        return fail(
            AndroidAudioOutputError::unsupported_playback_contract,
            AAUDIO_ERROR_ILLEGAL_ARGUMENT);
    }

    if (lifecycle_.prepare() != playback::PlaybackTransitionError::none) {
        (void)AAudioStream_close(stream_);
        stream_ = nullptr;
        return fail(AndroidAudioOutputError::invalid_state, AAUDIO_ERROR_INVALID_STATE);
    }

    return true;
}

bool AndroidAudioOutput::start() noexcept {
    synchronize_async_error();
    if (lifecycle_.state() != playback::PlaybackState::ready || stream_ == nullptr) {
        return fail(AndroidAudioOutputError::invalid_state, AAUDIO_ERROR_INVALID_STATE);
    }

    const aaudio_result_t result = AAudioStream_requestStart(stream_);
    if (result != AAUDIO_OK) {
        return fail(AndroidAudioOutputError::stream_start_failed, result);
    }

    if (lifecycle_.start() != playback::PlaybackTransitionError::none) {
        return fail(AndroidAudioOutputError::invalid_state, AAUDIO_ERROR_INVALID_STATE);
    }

    clear_error();
    return true;
}

bool AndroidAudioOutput::stop() noexcept {
    synchronize_async_error();
    if (lifecycle_.state() != playback::PlaybackState::running || stream_ == nullptr) {
        return fail(AndroidAudioOutputError::invalid_state, AAUDIO_ERROR_INVALID_STATE);
    }

    const aaudio_result_t result = AAudioStream_requestStop(stream_);
    if (result != AAUDIO_OK) {
        return fail(AndroidAudioOutputError::stream_stop_failed, result);
    }
    if (!wait_for_stopped()) {
        return fail(AndroidAudioOutputError::stream_stop_timeout, native_result_);
    }

    (void)queue_.discard_pending();
    if (lifecycle_.stop() != playback::PlaybackTransitionError::none) {
        return fail(AndroidAudioOutputError::invalid_state, AAUDIO_ERROR_INVALID_STATE);
    }

    clear_error();
    return true;
}

bool AndroidAudioOutput::queue_local_test_signal() noexcept {
    synchronize_async_error();
    if (lifecycle_.state() != playback::PlaybackState::ready || stream_ == nullptr) {
        return fail(AndroidAudioOutputError::invalid_state, AAUDIO_ERROR_INVALID_STATE);
    }

    (void)queue_.discard_pending();
    constexpr std::uint64_t total_samples =
        static_cast<std::uint64_t>(local_test_logical_frames) *
        playback::logical_pcm_frame_frames;
    std::array<std::int16_t, playback::logical_pcm_frame_samples> samples{};

    for (std::uint32_t frame_index = 0U;
         frame_index < local_test_logical_frames;
         ++frame_index) {
        for (std::uint32_t sample_in_frame = 0U;
             sample_in_frame < playback::logical_pcm_frame_frames;
             ++sample_in_frame) {
            const std::uint64_t sample_index =
                static_cast<std::uint64_t>(frame_index) *
                    playback::logical_pcm_frame_frames +
                sample_in_frame;
            const double phase =
                2.0 * pi * local_test_frequency_hz *
                static_cast<double>(sample_index) /
                static_cast<double>(playback::pcm_sample_rate);
            const double scaled =
                std::sin(phase) * local_test_amplitude *
                fade_gain(sample_index, total_samples);
            const auto value = static_cast<std::int16_t>(
                std::lround(scaled * 32'767.0));
            const std::size_t output_index =
                static_cast<std::size_t>(sample_in_frame) *
                playback::pcm_channel_count;
            samples[output_index] = value;
            samples[output_index + 1U] = value;
        }

        const std::uint32_t flags = frame_index == 0U
            ? playback::pcm_frame_flag_discontinuity
            : 0U;
        const auto queued = queue_.enqueue(playback::PcmPlaybackFrameView{
            samples,
            next_local_test_sequence_,
            flags,
        });
        if (!queued.ok()) {
            return fail(
                AndroidAudioOutputError::local_test_queue_failed,
                AAUDIO_ERROR_WOULD_BLOCK,
                queued.error);
        }
        ++next_local_test_sequence_;
        ++local_test_logical_frames_;
    }

    ++local_test_batches_;
    clear_error();
    return true;
}

void AndroidAudioOutput::clear_queue() noexcept {
    if (lifecycle_.state() == playback::PlaybackState::ready) {
        (void)queue_.discard_pending();
    }
}

void AndroidAudioOutput::close() noexcept {
    if (stream_ != nullptr) {
        if (lifecycle_.state() == playback::PlaybackState::running) {
            if (AAudioStream_requestStop(stream_) == AAUDIO_OK) {
                (void)wait_for_stopped();
            }
        }
        (void)AAudioStream_close(stream_);
        stream_ = nullptr;
    }

    queue_.reset();
    lifecycle_.reset();
    sample_rate_ = 0;
    channel_count_ = 0;
    format_ = AAUDIO_FORMAT_UNSPECIFIED;
    sharing_mode_ = AAUDIO_SHARING_MODE_SHARED;
    performance_mode_ = AAUDIO_PERFORMANCE_MODE_NONE;
    frames_per_burst_ = 0;
    buffer_capacity_frames_ = 0;
    buffer_size_frames_ = 0;
    device_id_ = AAUDIO_UNSPECIFIED;
    requested_contract_granted_ = false;
    next_local_test_sequence_ = 1U;
    local_test_batches_ = 0U;
    local_test_logical_frames_ = 0U;
    async_error_.store(AAUDIO_OK, std::memory_order_relaxed);
    clear_error();
}

AndroidAudioOutputSnapshot AndroidAudioOutput::snapshot() const noexcept {
    AndroidAudioOutputSnapshot result{};
    result.state = lifecycle_.state();
    result.error = error_;
    result.queue_error = queue_error_.load(std::memory_order_relaxed);
    result.native_result = native_result_;

    const aaudio_result_t async_error = async_error_.load(std::memory_order_relaxed);
    if (async_error != AAUDIO_OK) {
        result.native_result = async_error;
        if (async_error == AAUDIO_ERROR_DISCONNECTED) {
            result.state = playback::PlaybackState::disconnected;
            result.error = AndroidAudioOutputError::stream_disconnected;
        } else {
            result.state = playback::PlaybackState::failed;
            result.error = AndroidAudioOutputError::callback_failed;
        }
    }

    result.sample_rate = sample_rate_;
    result.channel_count = channel_count_;
    result.format = format_;
    result.sharing_mode = sharing_mode_;
    result.performance_mode = performance_mode_;
    result.frames_per_burst = frames_per_burst_;
    result.buffer_capacity_frames = buffer_capacity_frames_;
    result.buffer_size_frames = buffer_size_frames_;
    result.device_id = device_id_;
    result.requested_contract_granted = requested_contract_granted_;
    result.callback_count = callback_count_.load(std::memory_order_relaxed);
    result.rendered_frames = rendered_frames_.load(std::memory_order_relaxed);
    result.disconnect_events = disconnect_events_.load(std::memory_order_relaxed);
    result.underrun_count = stream_ != nullptr ? AAudioStream_getXRunCount(stream_) : 0;
    result.queue = queue_.snapshot();
    result.local_test_batches = local_test_batches_;
    result.local_test_logical_frames = local_test_logical_frames_;
    return result;
}

aaudio_data_callback_result_t AndroidAudioOutput::data_callback(
    AAudioStream*,
    void* user_data,
    void* audio_data,
    std::int32_t frame_count) noexcept {
    auto* output = static_cast<AndroidAudioOutput*>(user_data);
    if (output == nullptr || audio_data == nullptr || frame_count <= 0) {
        if (output != nullptr) {
            output->async_error_.store(AAUDIO_ERROR_INVALID_STATE, std::memory_order_relaxed);
        }
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    const std::size_t sample_count =
        static_cast<std::size_t>(frame_count) * playback::pcm_channel_count;
    auto samples = std::span<std::int16_t>(
        static_cast<std::int16_t*>(audio_data),
        sample_count);
    const auto rendered = output->queue_.render(
        samples,
        static_cast<std::uint32_t>(frame_count));
    if (!rendered.ok()) {
        output->queue_error_.store(rendered.error, std::memory_order_relaxed);
        output->async_error_.store(AAUDIO_ERROR_INVALID_STATE, std::memory_order_relaxed);
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    output->callback_count_.fetch_add(1U, std::memory_order_relaxed);
    output->rendered_frames_.fetch_add(
        static_cast<std::uint64_t>(frame_count),
        std::memory_order_relaxed);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AndroidAudioOutput::error_callback(
    AAudioStream*,
    void* user_data,
    aaudio_result_t error) noexcept {
    auto* output = static_cast<AndroidAudioOutput*>(user_data);
    if (output == nullptr) {
        return;
    }

    output->async_error_.store(error, std::memory_order_relaxed);
    if (error == AAUDIO_ERROR_DISCONNECTED) {
        output->disconnect_events_.fetch_add(1U, std::memory_order_relaxed);
    }
}

bool AndroidAudioOutput::wait_for_stopped() noexcept {
    aaudio_stream_state_t state = AAudioStream_getState(stream_);
    for (std::uint32_t attempt = 0U;
         attempt < maximum_stop_state_changes;
         ++attempt) {
        if (is_stopped_state(state)) {
            return true;
        }

        aaudio_stream_state_t next_state = state;
        const aaudio_result_t result = AAudioStream_waitForStateChange(
            stream_,
            state,
            &next_state,
            static_cast<std::int64_t>(stop_wait_timeout_ns));
        if (result != AAUDIO_OK) {
            native_result_ = result;
            return false;
        }
        state = next_state;
    }

    native_result_ = AAUDIO_ERROR_TIMEOUT;
    return false;
}

bool AndroidAudioOutput::fail(
    AndroidAudioOutputError error,
    aaudio_result_t result,
    playback::PcmPlaybackQueueError queue_error) noexcept {
    error_ = error;
    queue_error_.store(queue_error, std::memory_order_relaxed);
    native_result_ = result;
    if (error == AndroidAudioOutputError::stream_disconnected) {
        lifecycle_.disconnect();
    } else {
        lifecycle_.fail();
    }
    return false;
}

void AndroidAudioOutput::synchronize_async_error() noexcept {
    const aaudio_result_t result = async_error_.load(std::memory_order_relaxed);
    if (result == AAUDIO_OK) {
        return;
    }

    (void)queue_.discard_pending();
    native_result_ = result;
    if (result == AAUDIO_ERROR_DISCONNECTED) {
        error_ = AndroidAudioOutputError::stream_disconnected;
        lifecycle_.disconnect();
    } else {
        error_ = AndroidAudioOutputError::callback_failed;
        lifecycle_.fail();
    }
}

void AndroidAudioOutput::clear_error() noexcept {
    error_ = AndroidAudioOutputError::none;
    queue_error_.store(
        playback::PcmPlaybackQueueError::none,
        std::memory_order_relaxed);
    native_result_ = AAUDIO_OK;
}

std::string_view to_string(AndroidAudioOutputError error) noexcept {
    switch (error) {
    case AndroidAudioOutputError::none:
        return "none";
    case AndroidAudioOutputError::invalid_state:
        return "invalid_state";
    case AndroidAudioOutputError::builder_creation_failed:
        return "builder_creation_failed";
    case AndroidAudioOutputError::stream_open_failed:
        return "stream_open_failed";
    case AndroidAudioOutputError::unsupported_callback_format:
        return "unsupported_callback_format";
    case AndroidAudioOutputError::unsupported_playback_contract:
        return "unsupported_playback_contract";
    case AndroidAudioOutputError::stream_start_failed:
        return "stream_start_failed";
    case AndroidAudioOutputError::stream_stop_failed:
        return "stream_stop_failed";
    case AndroidAudioOutputError::stream_stop_timeout:
        return "stream_stop_timeout";
    case AndroidAudioOutputError::local_test_queue_failed:
        return "local_test_queue_failed";
    case AndroidAudioOutputError::callback_failed:
        return "callback_failed";
    case AndroidAudioOutputError::stream_disconnected:
        return "stream_disconnected";
    }

    return "unknown_android_audio_output_error";
}

std::string_view format_to_string(aaudio_format_t format) noexcept {
    switch (format) {
    case AAUDIO_FORMAT_UNSPECIFIED:
        return "unspecified";
    case AAUDIO_FORMAT_PCM_I16:
        return "pcm_i16";
    case AAUDIO_FORMAT_PCM_FLOAT:
        return "pcm_float";
    case AAUDIO_FORMAT_PCM_I24_PACKED:
        return "pcm_i24_packed";
    case AAUDIO_FORMAT_PCM_I32:
        return "pcm_i32";
    case AAUDIO_FORMAT_IEC61937:
        return "iec61937";
    default:
        return "unknown_format";
    }
}

std::string_view sharing_mode_to_string(aaudio_sharing_mode_t mode) noexcept {
    switch (mode) {
    case AAUDIO_SHARING_MODE_EXCLUSIVE:
        return "exclusive";
    case AAUDIO_SHARING_MODE_SHARED:
        return "shared";
    default:
        return "unknown_sharing_mode";
    }
}

std::string_view performance_mode_to_string(aaudio_performance_mode_t mode) noexcept {
    switch (mode) {
    case AAUDIO_PERFORMANCE_MODE_NONE:
        return "none";
    case AAUDIO_PERFORMANCE_MODE_POWER_SAVING:
        return "power_saving";
    case AAUDIO_PERFORMANCE_MODE_LOW_LATENCY:
        return "low_latency";
    default:
        return "unknown_performance_mode";
    }
}

} // namespace wiretone::android
