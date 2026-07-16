#include "wiretone/core/version.hpp"

#include <iomanip>
#include <iostream>

#ifdef _WIN32
#include "wiretone/capture/lifecycle.hpp"
#include "wiretone/windows/wasapi_loopback_capture.hpp"
#endif

namespace {

#ifdef _WIN32
void print_failure(const wiretone::windows::WasapiCaptureSnapshot& snapshot) {
    std::cerr << "WASAPI loopback probe: FAIL\n";
    std::cerr << "State: " << wiretone::capture::to_string(snapshot.state) << '\n';
    std::cerr << "Error: " << wiretone::windows::to_string(snapshot.error) << '\n';
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

    if (!capture.start()) {
        print_failure(capture.snapshot());
        return 1;
    }
    std::cout << "Loopback start: PASS\n";

    if (!capture.stop()) {
        print_failure(capture.snapshot());
        return 1;
    }
    std::cout << "Loopback stop: PASS\n";
    std::cout << "Phase 2.1 WASAPI capture boundary: PASS\n";
    std::cout << "Audio packet draining and sample conversion are not implemented yet.\n";
    return 0;
#else
    std::cout << "Phase 2.1 WASAPI probe is available only on Windows.\n";
    return 0;
#endif
}
