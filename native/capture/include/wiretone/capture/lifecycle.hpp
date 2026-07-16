#pragma once

#include <string_view>

namespace wiretone::capture {

enum class CaptureState {
    idle = 0,
    ready,
    running,
    failed,
};

enum class CaptureTransitionError {
    none = 0,
    not_idle,
    not_ready,
    not_running,
    failed_state,
};

class CaptureLifecycle {
public:
    [[nodiscard]] CaptureState state() const noexcept;

    [[nodiscard]] CaptureTransitionError prepare() noexcept;
    [[nodiscard]] CaptureTransitionError start() noexcept;
    [[nodiscard]] CaptureTransitionError stop() noexcept;

    void fail() noexcept;
    void reset() noexcept;

private:
    CaptureState state_{CaptureState::idle};
};

[[nodiscard]] std::string_view to_string(CaptureState state) noexcept;
[[nodiscard]] std::string_view to_string(CaptureTransitionError error) noexcept;

} // namespace wiretone::capture
