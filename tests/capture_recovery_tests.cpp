#include "wiretone/capture/recovery.hpp"

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

    CaptureRecoveryController recovery;
    if (recovery.state() != CaptureRecoveryState::stable) {
        return fail("recovery controller did not begin stable");
    }

    if (recovery.request(CaptureRecoveryTrigger::none) !=
        CaptureRecoveryTransitionError::invalid_trigger) {
        return fail("none trigger was not rejected");
    }

    if (recovery.begin_attempt() != CaptureRecoveryTransitionError::not_pending) {
        return fail("restart began without a pending trigger");
    }

    if (recovery.request(CaptureRecoveryTrigger::device_invalidated) !=
            CaptureRecoveryTransitionError::none ||
        recovery.state() != CaptureRecoveryState::restart_pending) {
        return fail("device invalidation did not request a restart");
    }

    if (recovery.request(CaptureRecoveryTrigger::default_device_changed) !=
        CaptureRecoveryTransitionError::none) {
        return fail("repeated trigger was not coalesced");
    }

    auto snapshot = recovery.snapshot();
    if (snapshot.device_invalidations != 1U ||
        snapshot.default_device_changes != 1U ||
        snapshot.last_trigger != CaptureRecoveryTrigger::default_device_changed) {
        return fail("recovery trigger counters were not preserved");
    }

    if (recovery.begin_attempt() != CaptureRecoveryTransitionError::none ||
        recovery.state() != CaptureRecoveryState::restarting) {
        return fail("first restart attempt did not begin");
    }

    if (recovery.complete_failure(true) != CaptureRecoveryTransitionError::none ||
        recovery.state() != CaptureRecoveryState::restart_pending) {
        return fail("retryable restart failure did not remain pending");
    }

    if (recovery.begin_attempt() != CaptureRecoveryTransitionError::none ||
        recovery.complete_success() != CaptureRecoveryTransitionError::none ||
        recovery.state() != CaptureRecoveryState::stable) {
        return fail("second restart attempt did not complete successfully");
    }

    snapshot = recovery.snapshot();
    if (snapshot.restart_attempts != 2U ||
        snapshot.successful_restarts != 1U ||
        snapshot.failed_restarts != 1U ||
        !snapshot.discontinuity_pending) {
        return fail("successful recovery counters were incorrect");
    }

    if (!recovery.consume_discontinuity() || recovery.consume_discontinuity()) {
        return fail("post-recovery discontinuity was not one-shot");
    }

    recovery.reset();
    if (recovery.request(CaptureRecoveryTrigger::device_invalidated) !=
        CaptureRecoveryTransitionError::none) {
        return fail("recovery trigger failed after reset");
    }

    for (std::uint32_t attempt = 0U;
         attempt < maximum_capture_restart_attempts;
         ++attempt) {
        if (recovery.begin_attempt() != CaptureRecoveryTransitionError::none) {
            return fail("bounded restart attempt did not begin");
        }

        const bool retryable = true;
        if (recovery.complete_failure(retryable) !=
            CaptureRecoveryTransitionError::none) {
            return fail("bounded restart failure was rejected");
        }
    }

    snapshot = recovery.snapshot();
    if (snapshot.state != CaptureRecoveryState::failed ||
        snapshot.restart_attempts != maximum_capture_restart_attempts ||
        snapshot.failed_restarts != maximum_capture_restart_attempts) {
        return fail("restart attempts were not bounded");
    }

    if (recovery.request(CaptureRecoveryTrigger::default_device_changed) !=
        CaptureRecoveryTransitionError::failed_state) {
        return fail("failed recovery state accepted another trigger");
    }

    recovery.reset();
    if (recovery.request(CaptureRecoveryTrigger::default_device_changed) !=
            CaptureRecoveryTransitionError::none ||
        recovery.begin_attempt() != CaptureRecoveryTransitionError::none ||
        recovery.complete_failure(false) != CaptureRecoveryTransitionError::none ||
        recovery.state() != CaptureRecoveryState::failed) {
        return fail("non-retryable replacement failure did not fail immediately");
    }

    if (to_string(CaptureRecoveryState::restart_pending) != "restart_pending" ||
        to_string(CaptureRecoveryTrigger::device_invalidated) != "device_invalidated" ||
        to_string(CaptureRecoveryTransitionError::attempts_exhausted) !=
            "attempts_exhausted") {
        return fail("recovery diagnostic text changed unexpectedly");
    }

    std::cout << "PASS: WireTone capture recovery tests\n";
    return 0;
}
