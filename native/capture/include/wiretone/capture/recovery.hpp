#pragma once

#include <cstdint>
#include <string_view>

namespace wiretone::capture {

inline constexpr std::uint32_t maximum_capture_restart_attempts = 3U;

enum class CaptureRecoveryState {
    stable = 0,
    restart_pending,
    restarting,
    failed,
};

enum class CaptureRecoveryTrigger {
    none = 0,
    device_invalidated,
    default_device_changed,
};

enum class CaptureRecoveryTransitionError {
    none = 0,
    invalid_trigger,
    not_pending,
    not_restarting,
    attempts_exhausted,
    failed_state,
};

struct CaptureRecoverySnapshot {
    CaptureRecoveryState state{CaptureRecoveryState::stable};
    CaptureRecoveryTrigger last_trigger{CaptureRecoveryTrigger::none};
    std::uint32_t attempts_in_cycle{0};
    std::uint64_t restart_attempts{0};
    std::uint64_t successful_restarts{0};
    std::uint64_t failed_restarts{0};
    std::uint64_t device_invalidations{0};
    std::uint64_t default_device_changes{0};
    bool discontinuity_pending{false};
};

class CaptureRecoveryController {
public:
    [[nodiscard]] CaptureRecoveryState state() const noexcept;
    [[nodiscard]] CaptureRecoverySnapshot snapshot() const noexcept;

    [[nodiscard]] CaptureRecoveryTransitionError request(
        CaptureRecoveryTrigger trigger) noexcept;
    [[nodiscard]] CaptureRecoveryTransitionError begin_attempt() noexcept;
    [[nodiscard]] CaptureRecoveryTransitionError complete_success() noexcept;
    [[nodiscard]] CaptureRecoveryTransitionError complete_failure(
        bool retryable) noexcept;

    [[nodiscard]] bool consume_discontinuity() noexcept;
    void reset() noexcept;

private:
    CaptureRecoveryState state_{CaptureRecoveryState::stable};
    CaptureRecoveryTrigger last_trigger_{CaptureRecoveryTrigger::none};
    std::uint32_t attempts_in_cycle_{0};
    std::uint64_t restart_attempts_{0};
    std::uint64_t successful_restarts_{0};
    std::uint64_t failed_restarts_{0};
    std::uint64_t device_invalidations_{0};
    std::uint64_t default_device_changes_{0};
    bool discontinuity_pending_{false};
};

[[nodiscard]] std::string_view to_string(CaptureRecoveryState state) noexcept;
[[nodiscard]] std::string_view to_string(CaptureRecoveryTrigger trigger) noexcept;
[[nodiscard]] std::string_view to_string(
    CaptureRecoveryTransitionError error) noexcept;

} // namespace wiretone::capture
