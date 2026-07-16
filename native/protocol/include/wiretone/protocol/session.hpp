#pragma once

#include "wiretone/protocol/audio_frame.hpp"
#include "wiretone/protocol/control_payload.hpp"
#include "wiretone/protocol/packet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wiretone::protocol {

inline constexpr std::size_t receiver_frame_queue_capacity = 8;
inline constexpr std::uint64_t receiver_session_timeout_ms = 3'000;

enum class ReceiverSessionState {
    idle = 0,
    streaming,
};

enum class ReceiverSessionEvent {
    none = 0,
    stream_started,
    stream_replaced,
    stream_stopped,
    heartbeat_received,
    frame_queued,
    remote_error_received,
    session_timed_out,
};

enum class ReceiverSessionError {
    none = 0,
    non_monotonic_time,
    malformed_datagram,
    invalid_control_payload,
    stream_not_started,
    wrong_stream,
    unexpected_packet_type,
    audio_reassembly_failed,
};

struct ReceiverSessionCounters {
    std::uint64_t datagrams_received{0};
    std::uint64_t malformed_datagrams{0};
    std::uint64_t invalid_control_payloads{0};
    std::uint64_t rejected_before_start{0};
    std::uint64_t wrong_stream_datagrams{0};
    std::uint64_t unexpected_packet_types{0};
    std::uint64_t duplicate_fragments{0};
    std::uint64_t inconsistent_fragments{0};
    std::uint64_t expired_frames{0};
    std::uint64_t completed_frames{0};
    std::uint64_t dropped_frames{0};
    std::uint64_t discontinuities{0};
    std::uint64_t stream_starts{0};
    std::uint64_t stream_replacements{0};
    std::uint64_t stream_stops{0};
    std::uint64_t heartbeats{0};
    std::uint64_t remote_errors{0};
    std::uint64_t session_timeouts{0};
};

struct ReceiverSessionResult {
    ReceiverSessionError error{ReceiverSessionError::none};
    ReceiverSessionEvent event{ReceiverSessionEvent::none};
    ProtocolError protocol_error{ProtocolError::none};
    ControlPayloadError control_error{ControlPayloadError::none};
    AudioFrameError audio_error{AudioFrameError::none};
    std::size_t expired_frame_count{0};
    bool frame_queued{false};

    [[nodiscard]] bool ok() const noexcept {
        return error == ReceiverSessionError::none;
    }
};

class ReceiverSession {
public:
    [[nodiscard]] ReceiverSessionResult accept_datagram(
        std::span<const std::byte> datagram,
        std::uint64_t arrival_time_ms) noexcept;

    [[nodiscard]] ReceiverSessionResult advance_time(
        std::uint64_t now_ms) noexcept;

    [[nodiscard]] bool pop_frame(ReassembledAudioFrame& output) noexcept;

    void reset() noexcept;

    [[nodiscard]] ReceiverSessionState state() const noexcept;
    [[nodiscard]] bool streaming() const noexcept;
    [[nodiscard]] std::uint32_t active_stream_id() const noexcept;
    [[nodiscard]] const StreamStartPayload& active_format() const noexcept;
    [[nodiscard]] std::size_t queued_frame_count() const noexcept;
    [[nodiscard]] std::uint64_t last_sender_monotonic_time_us() const noexcept;
    [[nodiscard]] const ReceiverSessionCounters& counters() const noexcept;

private:
    [[nodiscard]] ReceiverSessionResult observe_time(
        std::uint64_t now_ms) noexcept;

    void clear_stream_state() noexcept;
    void clear_frame_queue() noexcept;
    void enqueue_frame(const ReassembledAudioFrame& frame) noexcept;

    ReceiverSessionState state_{ReceiverSessionState::idle};
    std::uint32_t active_stream_id_{0};
    StreamStartPayload active_format_{};
    AudioFrameReassembler reassembler_{};

    std::array<ReassembledAudioFrame, receiver_frame_queue_capacity> frame_queue_{};
    std::size_t queue_head_{0};
    std::size_t queue_size_{0};

    bool has_observed_time_{false};
    std::uint64_t last_observed_time_ms_{0};
    std::uint64_t last_liveness_time_ms_{0};
    std::uint64_t last_sender_monotonic_time_us_{0};

    ReceiverSessionCounters counters_{};
};

[[nodiscard]] std::string_view to_string(ReceiverSessionState state) noexcept;
[[nodiscard]] std::string_view to_string(ReceiverSessionEvent event) noexcept;
[[nodiscard]] std::string_view to_string(ReceiverSessionError error) noexcept;

} // namespace wiretone::protocol
