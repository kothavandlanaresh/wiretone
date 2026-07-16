#include "wiretone/core/version.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include "wiretone/capture/captured_packet.hpp"
#include "wiretone/capture/lifecycle.hpp"
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
    std::cout << "Draining loopback packets for up to 2 seconds. "
                 "Play audio through the default endpoint now.\n";

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!capture.drain_available()) {
            print_failure(capture.snapshot());
            return 1;
        }

        if (capture.snapshot().packets_drained != 0U) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto drained = capture.snapshot();

    if (!capture.stop()) {
        print_failure(capture.snapshot());
        return 1;
    }
    std::cout << "Loopback stop: PASS\n";
    std::cout << "Packets drained: " << drained.packets_drained << '\n';
    std::cout << "Frames drained: " << drained.frames_drained << '\n';
    std::cout << "PCM bytes produced: " << drained.pcm_bytes_produced << '\n';
    std::cout << "Silent packets: " << drained.silent_packets << '\n';
    std::cout << "Discontinuity packets: " << drained.discontinuity_packets << '\n';
    std::cout << "Timestamp-error packets: " << drained.timestamp_error_packets << '\n';
    std::cout << "Empty polls: " << drained.empty_poll_count << '\n';
    std::cout << "Last device position: "
              << drained.last_device_position_frames << " frames\n";
    std::cout << "Last QPC position: "
              << drained.last_qpc_position_100ns << " x 100 ns\n";

    if (drained.packets_drained == 0U) {
        std::cerr << "Phase 2.2 packet drain: FAIL\n";
        std::cerr << "No loopback packet was observed. Play audio through the default "
                     "render endpoint and rerun the sender shell.\n";
        return 2;
    }

    std::cout << "Phase 2.2 WASAPI packet drain and PCM normalization: PASS\n";
    std::cout << "Normalized PCM remains local in scratch memory; UDP is not implemented yet.\n";
    return 0;
#else
    std::cout << "Phase 2.2 WASAPI probe is available only on Windows.\n";
    return 0;
#endif
}
