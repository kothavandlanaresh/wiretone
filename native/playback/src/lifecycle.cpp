#include "wiretone/playback/lifecycle.hpp"

namespace wiretone::playback {

PlaybackState PlaybackLifecycle::state() const noexcept {
    return state_;
}

PlaybackTransitionError PlaybackLifecycle::prepare() noexcept {
    if (state_ == PlaybackState::disconnected || state_ == PlaybackState::failed) {
        return PlaybackTransitionError::terminal_state;
    }
    if (state_ != PlaybackState::idle) {
        return PlaybackTransitionError::not_idle;
    }

    state_ = PlaybackState::ready;
    return PlaybackTransitionError::none;
}

PlaybackTransitionError PlaybackLifecycle::start() noexcept {
    if (state_ == PlaybackState::disconnected || state_ == PlaybackState::failed) {
        return PlaybackTransitionError::terminal_state;
    }
    if (state_ != PlaybackState::ready) {
        return PlaybackTransitionError::not_ready;
    }

    state_ = PlaybackState::running;
    return PlaybackTransitionError::none;
}

PlaybackTransitionError PlaybackLifecycle::stop() noexcept {
    if (state_ == PlaybackState::disconnected || state_ == PlaybackState::failed) {
        return PlaybackTransitionError::terminal_state;
    }
    if (state_ != PlaybackState::running) {
        return PlaybackTransitionError::not_running;
    }

    state_ = PlaybackState::ready;
    return PlaybackTransitionError::none;
}

void PlaybackLifecycle::disconnect() noexcept {
    if (state_ != PlaybackState::idle) {
        state_ = PlaybackState::disconnected;
    }
}

void PlaybackLifecycle::fail() noexcept {
    state_ = PlaybackState::failed;
}

void PlaybackLifecycle::reset() noexcept {
    state_ = PlaybackState::idle;
}

std::string_view to_string(PlaybackState state) noexcept {
    switch (state) {
    case PlaybackState::idle:
        return "idle";
    case PlaybackState::ready:
        return "ready";
    case PlaybackState::running:
        return "running";
    case PlaybackState::disconnected:
        return "disconnected";
    case PlaybackState::failed:
        return "failed";
    }

    return "unknown_playback_state";
}

std::string_view to_string(PlaybackTransitionError error) noexcept {
    switch (error) {
    case PlaybackTransitionError::none:
        return "none";
    case PlaybackTransitionError::not_idle:
        return "not_idle";
    case PlaybackTransitionError::not_ready:
        return "not_ready";
    case PlaybackTransitionError::not_running:
        return "not_running";
    case PlaybackTransitionError::terminal_state:
        return "terminal_state";
    }

    return "unknown_playback_transition_error";
}

} // namespace wiretone::playback
