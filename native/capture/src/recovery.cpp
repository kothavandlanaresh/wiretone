#include "wiretone/capture/recovery.hpp"

namespace wiretone::capture {

CaptureRecoveryState CaptureRecoveryController::state() const noexcept {
    return state_;
}

CaptureRecoverySnapshot CaptureRecoveryController::snapshot() const noexcept {
    return CaptureRecoverySnapshot{
        state_,
        last_trigger_,
        attempts_in_cycle_,
        restart_attempts_,
        successful_restarts_,
        failed_restarts_,
        device_invalidations_,
        default_device_changes_,
        discontinuity_pending_,
    };
}

CaptureRecoveryTransitionError CaptureRecoveryController::request(
    CaptureRecoveryTrigger trigger) noexcept {
    if (trigger == CaptureRecoveryTrigger::none) {
        return CaptureRecoveryTransitionError::invalid_trigger;
    }

    if (state_ == CaptureRecoveryState::failed) {
        return CaptureRecoveryTransitionError::failed_state;
    }

    last_trigger_ = trigger;
    if (trigger == CaptureRecoveryTrigger::device_invalidated) {
        ++device_invalidations_;
    } else if (trigger == CaptureRecoveryTrigger::default_device_changed) {
        ++default_device_changes_;
    }

    if (state_ == CaptureRecoveryState::stable) {
        attempts_in_cycle_ = 0U;
        state_ = CaptureRecoveryState::restart_pending;
    }

    return CaptureRecoveryTransitionError::none;
}

CaptureRecoveryTransitionError CaptureRecoveryController::begin_attempt() noexcept {
    if (state_ == CaptureRecoveryState::failed) {
        return CaptureRecoveryTransitionError::failed_state;
    }

    if (state_ != CaptureRecoveryState::restart_pending) {
        return CaptureRecoveryTransitionError::not_pending;
    }

    if (attempts_in_cycle_ >= maximum_capture_restart_attempts) {
        state_ = CaptureRecoveryState::failed;
        return CaptureRecoveryTransitionError::attempts_exhausted;
    }

    ++attempts_in_cycle_;
    ++restart_attempts_;
    state_ = CaptureRecoveryState::restarting;
    return CaptureRecoveryTransitionError::none;
}

CaptureRecoveryTransitionError CaptureRecoveryController::complete_success() noexcept {
    if (state_ == CaptureRecoveryState::failed) {
        return CaptureRecoveryTransitionError::failed_state;
    }

    if (state_ != CaptureRecoveryState::restarting) {
        return CaptureRecoveryTransitionError::not_restarting;
    }

    ++successful_restarts_;
    attempts_in_cycle_ = 0U;
    discontinuity_pending_ = true;
    state_ = CaptureRecoveryState::stable;
    return CaptureRecoveryTransitionError::none;
}

CaptureRecoveryTransitionError CaptureRecoveryController::complete_failure(
    bool retryable) noexcept {
    if (state_ == CaptureRecoveryState::failed) {
        return CaptureRecoveryTransitionError::failed_state;
    }

    if (state_ != CaptureRecoveryState::restarting) {
        return CaptureRecoveryTransitionError::not_restarting;
    }

    ++failed_restarts_;
    if (retryable && attempts_in_cycle_ < maximum_capture_restart_attempts) {
        state_ = CaptureRecoveryState::restart_pending;
    } else {
        state_ = CaptureRecoveryState::failed;
    }

    return CaptureRecoveryTransitionError::none;
}

bool CaptureRecoveryController::consume_discontinuity() noexcept {
    const bool was_pending = discontinuity_pending_;
    discontinuity_pending_ = false;
    return was_pending;
}

void CaptureRecoveryController::reset() noexcept {
    state_ = CaptureRecoveryState::stable;
    last_trigger_ = CaptureRecoveryTrigger::none;
    attempts_in_cycle_ = 0U;
    restart_attempts_ = 0U;
    successful_restarts_ = 0U;
    failed_restarts_ = 0U;
    device_invalidations_ = 0U;
    default_device_changes_ = 0U;
    discontinuity_pending_ = false;
}

std::string_view to_string(CaptureRecoveryState state) noexcept {
    switch (state) {
    case CaptureRecoveryState::stable:
        return "stable";
    case CaptureRecoveryState::restart_pending:
        return "restart_pending";
    case CaptureRecoveryState::restarting:
        return "restarting";
    case CaptureRecoveryState::failed:
        return "failed";
    }

    return "unknown_capture_recovery_state";
}

std::string_view to_string(CaptureRecoveryTrigger trigger) noexcept {
    switch (trigger) {
    case CaptureRecoveryTrigger::none:
        return "none";
    case CaptureRecoveryTrigger::device_invalidated:
        return "device_invalidated";
    case CaptureRecoveryTrigger::default_device_changed:
        return "default_device_changed";
    }

    return "unknown_capture_recovery_trigger";
}

std::string_view to_string(CaptureRecoveryTransitionError error) noexcept {
    switch (error) {
    case CaptureRecoveryTransitionError::none:
        return "none";
    case CaptureRecoveryTransitionError::invalid_trigger:
        return "invalid_trigger";
    case CaptureRecoveryTransitionError::not_pending:
        return "not_pending";
    case CaptureRecoveryTransitionError::not_restarting:
        return "not_restarting";
    case CaptureRecoveryTransitionError::attempts_exhausted:
        return "attempts_exhausted";
    case CaptureRecoveryTransitionError::failed_state:
        return "failed_state";
    }

    return "unknown_capture_recovery_transition_error";
}

} // namespace wiretone::capture
