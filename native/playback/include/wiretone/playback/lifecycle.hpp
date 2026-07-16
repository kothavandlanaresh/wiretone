#pragma once

#include <string_view>

namespace wiretone::playback {

enum class PlaybackState {
    idle = 0,
    ready,
    running,
    disconnected,
    failed,
};

enum class PlaybackTransitionError {
    none = 0,
    not_idle,
    not_ready,
    not_running,
    terminal_state,
};

class PlaybackLifecycle {
public:
    [[nodiscard]] PlaybackState state() const noexcept;

    [[nodiscard]] PlaybackTransitionError prepare() noexcept;
    [[nodiscard]] PlaybackTransitionError start() noexcept;
    [[nodiscard]] PlaybackTransitionError stop() noexcept;

    void disconnect() noexcept;
    void fail() noexcept;
    void reset() noexcept;

private:
    PlaybackState state_{PlaybackState::idle};
};

[[nodiscard]] std::string_view to_string(PlaybackState state) noexcept;
[[nodiscard]] std::string_view to_string(PlaybackTransitionError error) noexcept;

} // namespace wiretone::playback
