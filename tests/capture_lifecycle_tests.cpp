#include "wiretone/capture/lifecycle.hpp"

#include <iostream>
#include <string_view>

namespace {

int fail(std::string_view message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    using namespace wiretone::capture;

    CaptureLifecycle lifecycle;
    if (lifecycle.state() != CaptureState::idle) {
        return fail("capture lifecycle did not begin idle");
    }

    if (lifecycle.start() != CaptureTransitionError::not_ready) {
        return fail("start before prepare was not rejected");
    }

    if (lifecycle.stop() != CaptureTransitionError::not_running) {
        return fail("stop before running was not rejected");
    }

    if (lifecycle.prepare() != CaptureTransitionError::none ||
        lifecycle.state() != CaptureState::ready) {
        return fail("prepare did not enter ready state");
    }

    if (lifecycle.prepare() != CaptureTransitionError::not_idle) {
        return fail("second prepare was not rejected");
    }

    if (lifecycle.start() != CaptureTransitionError::none ||
        lifecycle.state() != CaptureState::running) {
        return fail("start did not enter running state");
    }

    if (lifecycle.start() != CaptureTransitionError::not_ready) {
        return fail("second start was not rejected");
    }

    if (lifecycle.stop() != CaptureTransitionError::none ||
        lifecycle.state() != CaptureState::ready) {
        return fail("stop did not return to ready state");
    }

    lifecycle.fail();
    if (lifecycle.state() != CaptureState::failed) {
        return fail("fail did not enter failed state");
    }

    if (lifecycle.prepare() != CaptureTransitionError::failed_state ||
        lifecycle.start() != CaptureTransitionError::failed_state ||
        lifecycle.stop() != CaptureTransitionError::failed_state) {
        return fail("failed state accepted a lifecycle transition");
    }

    lifecycle.reset();
    if (lifecycle.state() != CaptureState::idle) {
        return fail("reset did not return to idle state");
    }

    if (to_string(CaptureState::running) != "running" ||
        to_string(CaptureTransitionError::not_ready) != "not_ready") {
        return fail("capture lifecycle diagnostic text changed unexpectedly");
    }

    std::cout << "PASS: WireTone capture lifecycle tests\n";
    return 0;
}
