#include "aaudio_output.hpp"
#include "wiretone/core/version.hpp"
#include "wiretone/playback/lifecycle.hpp"
#include "wiretone/playback/pcm_queue.hpp"

#include <jni.h>

#include <memory>
#include <mutex>
#include <sstream>
#include <string>

namespace {

std::mutex output_mutex;
std::unique_ptr<wiretone::android::AndroidAudioOutput> output;

jstring make_java_string(JNIEnv* environment, const std::string& value) {
    return environment->NewStringUTF(value.c_str());
}

wiretone::android::AndroidAudioOutput& ensure_output() {
    if (!output) {
        output = std::make_unique<wiretone::android::AndroidAudioOutput>();
    }
    return *output;
}

std::string playback_status() {
    if (!output) {
        return "Playback state: idle\nAAudio stream: not opened";
    }

    const auto snapshot = output->snapshot();
    std::ostringstream stream;
    stream << "Playback state: " << wiretone::playback::to_string(snapshot.state) << '\n'
           << "Error: " << wiretone::android::to_string(snapshot.error) << '\n'
           << "Queue error: " << wiretone::playback::to_string(snapshot.queue_error) << '\n'
           << "AAudio result: " << snapshot.native_result << '\n'
           << "Negotiated: " << snapshot.sample_rate << " Hz, "
           << snapshot.channel_count << " channels, "
           << wiretone::android::format_to_string(snapshot.format) << '\n'
           << "Sharing: " << wiretone::android::sharing_mode_to_string(snapshot.sharing_mode) << '\n'
           << "Performance: "
           << wiretone::android::performance_mode_to_string(snapshot.performance_mode) << '\n'
           << "Frames per burst: " << snapshot.frames_per_burst << '\n'
           << "Buffer: " << snapshot.buffer_size_frames << " / "
           << snapshot.buffer_capacity_frames << " frames\n"
           << "Device ID: " << snapshot.device_id << '\n'
           << "Requested 48 kHz stereo PCM16: "
           << (snapshot.requested_contract_granted ? "GRANTED" : "NOT GRANTED") << '\n'
           << "Callbacks: " << snapshot.callback_count << '\n'
           << "Rendered output frames: " << snapshot.rendered_frames << '\n'
           << "AAudio underruns: " << snapshot.underrun_count << '\n'
           << "Disconnect events: " << snapshot.disconnect_events << '\n'
           << "Queue capacity: " << wiretone::playback::pcm_playback_queue_capacity
           << " logical frames\n"
           << "Queued logical frames: " << snapshot.queue.queued_logical_frames << '\n'
           << "Queued PCM frames: " << snapshot.queue.queued_pcm_frames << '\n'
           << "Enqueued logical frames: " << snapshot.queue.enqueued_logical_frames << '\n'
           << "Consumed logical frames: " << snapshot.queue.consumed_logical_frames << '\n'
           << "PCM frames from queue: " << snapshot.queue.queue_output_frames << '\n'
           << "Silence-fill frames: " << snapshot.queue.silence_output_frames << '\n'
           << "Queue underrun callbacks: " << snapshot.queue.underrun_callbacks << '\n'
           << "Dropped newest logical frames: " << snapshot.queue.dropped_logical_frames << '\n'
           << "Discarded logical frames: " << snapshot.queue.discarded_logical_frames << '\n'
           << "Discarded partial frames: " << snapshot.queue.discarded_partial_frames << '\n'
           << "Consumed discontinuities: "
           << snapshot.queue.discontinuity_logical_frames << '\n'
           << "Last enqueued sequence: " << snapshot.queue.last_enqueued_sequence << '\n'
           << "Last consumed sequence: " << snapshot.queue.last_consumed_sequence << '\n'
           << "Last consumed flags: " << snapshot.queue.last_consumed_flags << '\n'
           << "Local test batches: " << snapshot.local_test_batches << '\n'
           << "Local test logical frames: " << snapshot.local_test_logical_frames;
    return stream.str();
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_dev_wiretone_receiver_MainActivity_nativeVersion(JNIEnv* environment, jobject) {
    const std::string value = std::string(wiretone::core::product_name()) + " native " +
                              std::string(wiretone::core::version_string());
    return make_java_string(environment, value);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_wiretone_receiver_MainActivity_nativeOpenPlayback(JNIEnv*, jobject) {
    const std::scoped_lock lock(output_mutex);
    auto& audio_output = ensure_output();
    const auto state = audio_output.snapshot().state;
    if (state == wiretone::playback::PlaybackState::ready ||
        state == wiretone::playback::PlaybackState::running) {
        return JNI_TRUE;
    }
    if (state != wiretone::playback::PlaybackState::idle) {
        audio_output.close();
    }
    return audio_output.open() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_wiretone_receiver_MainActivity_nativeStartPlayback(JNIEnv*, jobject) {
    const std::scoped_lock lock(output_mutex);
    if (!output) {
        return JNI_FALSE;
    }
    if (output->snapshot().state == wiretone::playback::PlaybackState::running) {
        return JNI_TRUE;
    }
    return output->start() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_wiretone_receiver_MainActivity_nativeStopPlayback(JNIEnv*, jobject) {
    const std::scoped_lock lock(output_mutex);
    if (!output) {
        return JNI_FALSE;
    }
    if (output->snapshot().state == wiretone::playback::PlaybackState::ready) {
        return JNI_TRUE;
    }
    return output->stop() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_wiretone_receiver_MainActivity_nativeQueueLocalTestSignal(JNIEnv*, jobject) {
    const std::scoped_lock lock(output_mutex);
    if (!output) {
        return JNI_FALSE;
    }
    return output->queue_local_test_signal() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_wiretone_receiver_MainActivity_nativeClearPlaybackQueue(JNIEnv*, jobject) {
    const std::scoped_lock lock(output_mutex);
    if (output) {
        output->clear_queue();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_dev_wiretone_receiver_MainActivity_nativeClosePlayback(JNIEnv*, jobject) {
    const std::scoped_lock lock(output_mutex);
    if (output) {
        output->close();
        output.reset();
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_wiretone_receiver_MainActivity_nativePlaybackStatus(JNIEnv* environment, jobject) {
    const std::scoped_lock lock(output_mutex);
    return make_java_string(environment, playback_status());
}
