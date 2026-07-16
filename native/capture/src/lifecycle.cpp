#include "wiretone/capture/lifecycle.hpp"

namespace wiretone::capture {

CaptureState CaptureLifecycle::state() const noexcept {
    return state_;
}

CaptureTransitionError CaptureLifecycle::prepare() noexcept {
    if (state_ == CaptureState::failed) {
        return CaptureTransitionError::failed_state;
    }

    if (state_ != CaptureState::idle) {
        return CaptureTransitionError::not_idle;
    }

    state_ = CaptureState::ready;
    return CaptureTransitionError::none;
}

CaptureTransitionError CaptureLifecycle::start() noexcept {
    if (state_ == CaptureState::failed) {
        return CaptureTransitionError::failed_state;
    }

    if (state_ != CaptureState::ready) {
        return CaptureTransitionError::not_ready;
    }

    state_ = CaptureState::running;
    return CaptureTransitionError::none;
}

CaptureTransitionError CaptureLifecycle::stop() noexcept {
    if (state_ == CaptureState::failed) {
        return CaptureTransitionError::failed_state;
    }

    if (state_ != CaptureState::running) {
        return CaptureTransitionError::not_running;
    }

    state_ = CaptureState::ready;
    return CaptureTransitionError::none;
}

void CaptureLifecycle::fail() noexcept {
    state_ = CaptureState::failed;
}

void CaptureLifecycle::reset() noexcept {
    state_ = CaptureState::idle;
}

std::string_view to_string(CaptureState state) noexcept {
    switch (state) {
    case CaptureState::idle:
        return "idle";
    case CaptureState::ready:
        return "ready";
    case CaptureState::running:
        return "running";
    case CaptureState::failed:
        return "failed";
    }

    return "unknown_capture_state";
}

std::string_view to_string(CaptureTransitionError error) noexcept {
    switch (error) {
    case CaptureTransitionError::none:
        return "none";
    case CaptureTransitionError::not_idle:
        return "not_idle";
    case CaptureTransitionError::not_ready:
        return "not_ready";
    case CaptureTransitionError::not_running:
        return "not_running";
    case CaptureTransitionError::failed_state:
        return "failed_state";
    }

    return "unknown_capture_transition_error";
}

} // namespace wiretone::capture
