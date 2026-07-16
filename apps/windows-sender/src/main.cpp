#include "wiretone/core/version.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include "wiretone/capture/captured_packet.hpp"
#include "wiretone/capture/lifecycle.hpp"
#include "wiretone/capture/recovery.hpp"
#include "wiretone/windows/wasapi_loopback_capture.hpp"
#endif

namespace {

#ifdef _WIN32
void print_failure(const wiretone::windows::WasapiCaptureSnapshot& snapshot) {
    std::cerr << "WASAPI loopback probe: FAIL\n";
    std::cerr << "State: " << wiretone::capture::to_string(snapshot.state) << '\n';
    std::cerr << "Error: " << wiretone::windows::to_string(snapshot.error) << '\n';
    std::cerr << "Conversion error: "
              << wiretone::capture::to_string(snapshot.conversion_error) << '\n';
    std::cerr << "Frame assembler error: "
              << wiretone::capture::to_string(snapshot.frame_assembler_error) << '\n';
    std::cerr << "Recovery state: "
              << wiretone::capture::to_string(snapshot.recovery.state) << '\n';
    std::cerr << "Recovery trigger: "
              << wiretone::capture::to_string(snapshot.recovery.last_trigger) << '\n';
    std::cerr << "HRESULT: 0x"
              << std::hex << std::uppercase
              << static_cast<std::uint32_t>(snapshot.native_result)
              << std::dec << std::nouppercase << '\n';
}
#endif

} // namespace

int main() {
    std::cout << wiretone::core::product_name() << " sender shell "
              << wiretone::core::version_string() << '\n';

#ifdef _WIN32
    wiretone::windows::WasapiLoopbackCapture capture;

    if (!capture.initialize()) {
        print_failure(capture.snapshot());
        return 1;
    }

    const auto initialized = capture.snapshot();
    std::cout << "Default render endpoint: "
              << (initialized.endpoint_name.empty() ? "<unnamed>" : initialized.endpoint_name)
              << '\n';
    std::cout << "Endpoint ID: " << initialized.endpoint_id << '\n';
    std::cout << "Mix format: "
              << initialized.mix_format.sample_rate << " Hz, "
              << initialized.mix_format.channel_count << " channels, "
              << initialized.mix_format.container_bits_per_sample << " container bits, "
              << initialized.mix_format.valid_bits_per_sample << " valid bits, "
              << wiretone::windows::to_string(initialized.mix_format.sample_kind)
              << (initialized.mix_format.extensible ? ", extensible" : "")
              << '\n';
    std::cout << "Endpoint buffer: "
              << initialized.endpoint_buffer_frames << " frames\n";
    std::cout << "48 kHz stereo float normalization: "
              << (initialized.normalization_supported ? "SUPPORTED" : "UNSUPPORTED")
              << '\n';

    if (!capture.start()) {
        print_failure(capture.snapshot());
        return 1;
    }
    std::cout << "Loopback start: PASS\n";
    std::cout << "Draining loopback packets for up to 3 seconds. "
                 "Keep audio playing through the default endpoint.\n";

    const auto initial_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < initial_deadline) {
        if (!capture.drain_available()) {
            print_failure(capture.snapshot());
            return 1;
        }

        if (capture.snapshot().completed_pcm_frames != 0U) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto before_recovery = capture.snapshot();
    if (before_recovery.completed_pcm_frames == 0U) {
        std::cerr << "Phase 2.3 PCM frame assembly: FAIL\n";
        std::cerr << "No complete 960-frame logical PCM frame was observed. "
                     "Keep audio playing through the default endpoint and rerun.\n";
        return 3;
    }

    if (!capture.request_simulated_device_invalidation_for_probe()) {
        print_failure(capture.snapshot());
        return 1;
    }
    std::cout << "Simulated device invalidation: REQUESTED\n";
    std::cout << "Recovering capture and waiting up to 3 seconds for a post-recovery frame.\n";

    const auto recovery_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < recovery_deadline) {
        if (!capture.drain_available()) {
            print_failure(capture.snapshot());
            return 1;
        }

        const auto current = capture.snapshot();
        if (current.recovery.successful_restarts >
                before_recovery.recovery.successful_restarts &&
            current.completed_pcm_frames > before_recovery.completed_pcm_frames &&
            current.recovery_discontinuity_pcm_frames >
                before_recovery.recovery_discontinuity_pcm_frames) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!capture.stop()) {
        print_failure(capture.snapshot());
        return 1;
    }
    const auto drained = capture.snapshot();
    std::cout << "Loopback stop: PASS\n";
    std::cout << "Recovered render endpoint: "
              << (drained.endpoint_name.empty() ? "<unnamed>" : drained.endpoint_name)
              << '\n';
    std::cout << "Recovered endpoint ID: " << drained.endpoint_id << '\n';
    std::cout << "Packets drained: " << drained.packets_drained << '\n';
    std::cout << "Frames drained: " << drained.frames_drained << '\n';
    std::cout << "PCM bytes produced: " << drained.pcm_bytes_produced << '\n';
    std::cout << "Silent packets: " << drained.silent_packets << '\n';
    std::cout << "Discontinuity packets: " << drained.discontinuity_packets << '\n';
    std::cout << "Timestamp-error packets: " << drained.timestamp_error_packets << '\n';
    std::cout << "Empty polls: " << drained.empty_poll_count << '\n';
    std::cout << "Completed 20 ms PCM frames: "
              << drained.completed_pcm_frames << '\n';
    std::cout << "Discontinuity PCM frames: "
              << drained.discontinuity_pcm_frames << '\n';
    std::cout << "Dropped partial PCM frames: "
              << drained.dropped_partial_pcm_frames << '\n';
    std::cout << "Last completed PCM sequence: "
              << drained.last_completed_pcm_sequence << '\n';
    std::cout << "Recovery attempts: " << drained.recovery.restart_attempts << '\n';
    std::cout << "Successful recoveries: "
              << drained.recovery.successful_restarts << '\n';
    std::cout << "Failed recoveries: " << drained.recovery.failed_restarts << '\n';
    std::cout << "Device invalidations: "
              << drained.recovery.device_invalidations << '\n';
    std::cout << "Default-device changes: "
              << drained.recovery.default_device_changes << '\n';
    std::cout << "Recovery discontinuity PCM frames: "
              << drained.recovery_discontinuity_pcm_frames << '\n';

    if (drained.recovery.successful_restarts <=
            before_recovery.recovery.successful_restarts) {
        std::cerr << "Phase 2.4 capture recovery: FAIL\n";
        std::cerr << "The simulated invalidation did not complete a successful restart.\n";
        return 4;
    }

    if (drained.completed_pcm_frames <= before_recovery.completed_pcm_frames) {
        std::cerr << "Phase 2.4 capture recovery: FAIL\n";
        std::cerr << "No complete PCM frame was produced after recovery.\n";
        return 5;
    }

    if (drained.recovery_discontinuity_pcm_frames <=
        before_recovery.recovery_discontinuity_pcm_frames) {
        std::cerr << "Phase 2.4 capture recovery: FAIL\n";
        std::cerr << "The first post-recovery PCM frame was not marked discontinuous.\n";
        return 6;
    }

    std::cout << "Phase 2.4 default-device invalidation and capture recovery: PASS\n";
    std::cout << "Recovered PCM remains local in memory; UDP is not implemented yet.\n";
    return 0;
#else
    std::cout << "Phase 2.4 WASAPI recovery probe is available only on Windows.\n";
    return 0;
#endif
}
