#include "wiretone/playback/lifecycle.hpp"

#include <iostream>
#include <string_view>

namespace {

int fail(std::string_view message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    using namespace wiretone::playback;

    PlaybackLifecycle lifecycle;
    if (lifecycle.state() != PlaybackState::idle) {
        return fail("playback lifecycle did not begin idle");
    }
    if (lifecycle.start() != PlaybackTransitionError::not_ready) {
        return fail("start before prepare was not rejected");
    }
    if (lifecycle.stop() != PlaybackTransitionError::not_running) {
        return fail("stop before running was not rejected");
    }
    if (lifecycle.prepare() != PlaybackTransitionError::none ||
        lifecycle.state() != PlaybackState::ready) {
        return fail("prepare did not enter ready state");
    }
    if (lifecycle.prepare() != PlaybackTransitionError::not_idle) {
        return fail("second prepare was not rejected");
    }
    if (lifecycle.start() != PlaybackTransitionError::none ||
        lifecycle.state() != PlaybackState::running) {
        return fail("start did not enter running state");
    }
    if (lifecycle.stop() != PlaybackTransitionError::none ||
        lifecycle.state() != PlaybackState::ready) {
        return fail("stop did not return to ready state");
    }

    lifecycle.disconnect();
    if (lifecycle.state() != PlaybackState::disconnected ||
        lifecycle.start() != PlaybackTransitionError::terminal_state) {
        return fail("disconnected state accepted a transition");
    }
    lifecycle.reset();
    if (lifecycle.state() != PlaybackState::idle) {
        return fail("reset did not return disconnected state to idle");
    }

    lifecycle.fail();
    if (lifecycle.state() != PlaybackState::failed ||
        lifecycle.prepare() != PlaybackTransitionError::terminal_state) {
        return fail("failed state accepted a transition");
    }
    lifecycle.reset();

    if (to_string(PlaybackState::running) != "running" ||
        to_string(PlaybackTransitionError::not_ready) != "not_ready") {
        return fail("playback lifecycle diagnostic text changed unexpectedly");
    }

    std::cout << "PASS: WireTone playback lifecycle tests\n";
    return 0;
}
