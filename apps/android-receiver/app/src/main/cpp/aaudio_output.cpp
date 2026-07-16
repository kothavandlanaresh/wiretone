#include "aaudio_output.hpp"

#include <cstring>

namespace wiretone::android {
namespace {

inline constexpr std::int32_t requested_sample_rate = 48'000;
inline constexpr std::int32_t requested_channel_count = 2;

[[nodiscard]] std::size_t bytes_per_sample(aaudio_format_t format) noexcept {
    switch (format) {
    case AAUDIO_FORMAT_PCM_I16:
        return sizeof(std::int16_t);
    case AAUDIO_FORMAT_PCM_FLOAT:
        return sizeof(float);
    case AAUDIO_FORMAT_PCM_I24_PACKED:
        return 3U;
    case AAUDIO_FORMAT_PCM_I32:
        return sizeof(std::int32_t);
    default:
        return 0U;
    }
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
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MOVIE);
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

    const std::size_t sample_bytes = bytes_per_sample(format_);
    if (channel_count_ <= 0 || sample_bytes == 0U) {
        (void)AAudioStream_close(stream_);
        stream_ = nullptr;
        return fail(AndroidAudioOutputError::unsupported_callback_format, AAUDIO_ERROR_ILLEGAL_ARGUMENT);
    }
    callback_bytes_per_frame_ =
        static_cast<std::size_t>(channel_count_) * sample_bytes;
    requested_contract_granted_ =
        sample_rate_ == requested_sample_rate &&
        channel_count_ == requested_channel_count &&
        format_ == AAUDIO_FORMAT_PCM_I16;

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

    if (lifecycle_.stop() != playback::PlaybackTransitionError::none) {
        return fail(AndroidAudioOutputError::invalid_state, AAUDIO_ERROR_INVALID_STATE);
    }

    clear_error();
    return true;
}

void AndroidAudioOutput::close() noexcept {
    if (stream_ != nullptr) {
        if (lifecycle_.state() == playback::PlaybackState::running) {
            (void)AAudioStream_requestStop(stream_);
        }
        (void)AAudioStream_close(stream_);
        stream_ = nullptr;
    }

    lifecycle_.reset();
    callback_bytes_per_frame_ = 0U;
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
    async_error_.store(AAUDIO_OK, std::memory_order_relaxed);
    clear_error();
}

AndroidAudioOutputSnapshot AndroidAudioOutput::snapshot() const noexcept {
    AndroidAudioOutputSnapshot result{};
    result.state = lifecycle_.state();
    result.error = error_;
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
    return result;
}

aaudio_data_callback_result_t AndroidAudioOutput::data_callback(
    AAudioStream*,
    void* user_data,
    void* audio_data,
    std::int32_t frame_count) noexcept {
    auto* output = static_cast<AndroidAudioOutput*>(user_data);
    if (output == nullptr || audio_data == nullptr || frame_count <= 0 ||
        output->callback_bytes_per_frame_ == 0U) {
        if (output != nullptr) {
            output->async_error_.store(AAUDIO_ERROR_INVALID_STATE, std::memory_order_relaxed);
        }
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    const std::size_t byte_count =
        static_cast<std::size_t>(frame_count) * output->callback_bytes_per_frame_;
    std::memset(audio_data, 0, byte_count);
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

bool AndroidAudioOutput::fail(
    AndroidAudioOutputError error,
    aaudio_result_t result) noexcept {
    error_ = error;
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
    case AndroidAudioOutputError::stream_start_failed:
        return "stream_start_failed";
    case AndroidAudioOutputError::stream_stop_failed:
        return "stream_stop_failed";
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
