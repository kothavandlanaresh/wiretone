#include "wiretone/protocol/session.hpp"

namespace wiretone::protocol {
namespace {

[[nodiscard]] constexpr bool has_flag(
    std::uint16_t flags,
    std::uint16_t flag) noexcept {
    return (flags & flag) != 0U;
}

} // namespace

void ReceiverSession::clear_frame_queue() noexcept {
    queue_head_ = 0U;
    queue_size_ = 0U;
}

void ReceiverSession::clear_stream_state() noexcept {
    state_ = ReceiverSessionState::idle;
    active_stream_id_ = 0U;
    active_format_ = StreamStartPayload{};
    reassembler_.reset();
    clear_frame_queue();
    last_sender_monotonic_time_us_ = 0U;
}

void ReceiverSession::reset() noexcept {
    clear_stream_state();
    has_observed_time_ = false;
    last_observed_time_ms_ = 0U;
    last_liveness_time_ms_ = 0U;
    counters_ = ReceiverSessionCounters{};
}

ReceiverSessionResult ReceiverSession::observe_time(std::uint64_t now_ms) noexcept {
    ReceiverSessionResult result{};

    if (has_observed_time_ && now_ms < last_observed_time_ms_) {
        result.error = ReceiverSessionError::non_monotonic_time;
        return result;
    }

    has_observed_time_ = true;
    last_observed_time_ms_ = now_ms;

    const auto expiry = reassembler_.expire_incomplete(now_ms);
    if (!expiry.ok()) {
        result.error = ReceiverSessionError::audio_reassembly_failed;
        result.audio_error = expiry.error;
        return result;
    }

    result.expired_frame_count = expiry.expired_frame_count;
    counters_.expired_frames += expiry.expired_frame_count;

    if (streaming() && now_ms - last_liveness_time_ms_ >= receiver_session_timeout_ms) {
        clear_stream_state();
        ++counters_.session_timeouts;
        result.event = ReceiverSessionEvent::session_timed_out;
    }

    return result;
}

ReceiverSessionResult ReceiverSession::advance_time(std::uint64_t now_ms) noexcept {
    return observe_time(now_ms);
}

void ReceiverSession::enqueue_frame(const ReassembledAudioFrame& frame) noexcept {
    if (queue_size_ == receiver_frame_queue_capacity) {
        queue_head_ = (queue_head_ + 1U) % receiver_frame_queue_capacity;
        --queue_size_;
        ++counters_.dropped_frames;
    }

    const std::size_t insert_index =
        (queue_head_ + queue_size_) % receiver_frame_queue_capacity;
    frame_queue_[insert_index] = frame;
    ++queue_size_;
}

ReceiverSessionResult ReceiverSession::accept_datagram(
    std::span<const std::byte> datagram,
    std::uint64_t arrival_time_ms) noexcept {
    auto result = observe_time(arrival_time_ms);
    if (!result.ok()) {
        return result;
    }

    ++counters_.datagrams_received;

    const auto parsed = parse_datagram(datagram);
    if (!parsed.ok()) {
        ++counters_.malformed_datagrams;
        result.error = ReceiverSessionError::malformed_datagram;
        result.protocol_error = parsed.error;
        return result;
    }

    if (parsed.header.type == PacketType::stream_start) {
        const auto payload = parse_stream_start(parsed.payload);
        if (!payload.ok()) {
            ++counters_.invalid_control_payloads;
            result.error = ReceiverSessionError::invalid_control_payload;
            result.control_error = payload.error;
            return result;
        }

        const bool replacing = streaming();
        clear_stream_state();
        state_ = ReceiverSessionState::streaming;
        active_stream_id_ = parsed.header.stream_id;
        active_format_ = payload.value;
        last_liveness_time_ms_ = arrival_time_ms;

        ++counters_.stream_starts;
        if (replacing) {
            ++counters_.stream_replacements;
            result.event = ReceiverSessionEvent::stream_replaced;
        } else {
            result.event = ReceiverSessionEvent::stream_started;
        }
        return result;
    }

    if (!streaming()) {
        ++counters_.rejected_before_start;
        result.error = ReceiverSessionError::stream_not_started;
        return result;
    }

    if (parsed.header.stream_id != active_stream_id_) {
        ++counters_.wrong_stream_datagrams;
        result.error = ReceiverSessionError::wrong_stream;
        return result;
    }

    switch (parsed.header.type) {
    case PacketType::stream_start:
        break;

    case PacketType::stream_stop: {
        const auto payload = parse_stream_stop(parsed.payload);
        if (!payload.ok()) {
            ++counters_.invalid_control_payloads;
            result.error = ReceiverSessionError::invalid_control_payload;
            result.control_error = payload.error;
            return result;
        }

        clear_stream_state();
        ++counters_.stream_stops;
        result.event = ReceiverSessionEvent::stream_stopped;
        return result;
    }

    case PacketType::heartbeat: {
        const auto payload = parse_heartbeat(parsed.payload);
        if (!payload.ok()) {
            ++counters_.invalid_control_payloads;
            result.error = ReceiverSessionError::invalid_control_payload;
            result.control_error = payload.error;
            return result;
        }

        last_liveness_time_ms_ = arrival_time_ms;
        last_sender_monotonic_time_us_ = payload.value.sender_monotonic_time_us;
        ++counters_.heartbeats;
        result.event = ReceiverSessionEvent::heartbeat_received;
        return result;
    }

    case PacketType::audio: {
        const auto audio = reassembler_.accept_datagram(datagram, arrival_time_ms);
        result.expired_frame_count += audio.expired_frame_count;
        counters_.expired_frames += audio.expired_frame_count;

        if (!audio.ok()) {
            result.error = ReceiverSessionError::audio_reassembly_failed;
            result.audio_error = audio.error;
            result.protocol_error = audio.protocol_error;

            if (audio.error == AudioFrameError::duplicate_fragment) {
                ++counters_.duplicate_fragments;
            } else if (audio.error == AudioFrameError::inconsistent_fragment_metadata) {
                ++counters_.inconsistent_fragments;
            }
            return result;
        }

        last_liveness_time_ms_ = arrival_time_ms;

        if (!audio.frame_completed) {
            return result;
        }

        if (has_flag(audio.frame.flags, flag_discontinuity)) {
            clear_frame_queue();
            ++counters_.discontinuities;
        }

        enqueue_frame(audio.frame);
        ++counters_.completed_frames;
        result.event = ReceiverSessionEvent::frame_queued;
        result.frame_queued = true;
        return result;
    }

    case PacketType::error: {
        const auto payload = parse_error_payload(parsed.payload);
        if (!payload.ok()) {
            ++counters_.invalid_control_payloads;
            result.error = ReceiverSessionError::invalid_control_payload;
            result.control_error = payload.error;
            return result;
        }

        last_liveness_time_ms_ = arrival_time_ms;
        ++counters_.remote_errors;
        result.event = ReceiverSessionEvent::remote_error_received;
        return result;
    }

    case PacketType::receiver_report:
        ++counters_.unexpected_packet_types;
        result.error = ReceiverSessionError::unexpected_packet_type;
        return result;
    }

    ++counters_.unexpected_packet_types;
    result.error = ReceiverSessionError::unexpected_packet_type;
    return result;
}

bool ReceiverSession::pop_frame(ReassembledAudioFrame& output) noexcept {
    if (queue_size_ == 0U) {
        return false;
    }

    output = frame_queue_[queue_head_];
    queue_head_ = (queue_head_ + 1U) % receiver_frame_queue_capacity;
    --queue_size_;
    return true;
}

ReceiverSessionState ReceiverSession::state() const noexcept {
    return state_;
}

bool ReceiverSession::streaming() const noexcept {
    return state_ == ReceiverSessionState::streaming;
}

std::uint32_t ReceiverSession::active_stream_id() const noexcept {
    return active_stream_id_;
}

const StreamStartPayload& ReceiverSession::active_format() const noexcept {
    return active_format_;
}

std::size_t ReceiverSession::queued_frame_count() const noexcept {
    return queue_size_;
}

std::uint64_t ReceiverSession::last_sender_monotonic_time_us() const noexcept {
    return last_sender_monotonic_time_us_;
}

const ReceiverSessionCounters& ReceiverSession::counters() const noexcept {
    return counters_;
}

std::string_view to_string(ReceiverSessionState state) noexcept {
    switch (state) {
    case ReceiverSessionState::idle:
        return "idle";
    case ReceiverSessionState::streaming:
        return "streaming";
    }
    return "unknown_receiver_session_state";
}

std::string_view to_string(ReceiverSessionEvent event) noexcept {
    switch (event) {
    case ReceiverSessionEvent::none:
        return "none";
    case ReceiverSessionEvent::stream_started:
        return "stream_started";
    case ReceiverSessionEvent::stream_replaced:
        return "stream_replaced";
    case ReceiverSessionEvent::stream_stopped:
        return "stream_stopped";
    case ReceiverSessionEvent::heartbeat_received:
        return "heartbeat_received";
    case ReceiverSessionEvent::frame_queued:
        return "frame_queued";
    case ReceiverSessionEvent::remote_error_received:
        return "remote_error_received";
    case ReceiverSessionEvent::session_timed_out:
        return "session_timed_out";
    }
    return "unknown_receiver_session_event";
}

std::string_view to_string(ReceiverSessionError error) noexcept {
    switch (error) {
    case ReceiverSessionError::none:
        return "none";
    case ReceiverSessionError::non_monotonic_time:
        return "non_monotonic_time";
    case ReceiverSessionError::malformed_datagram:
        return "malformed_datagram";
    case ReceiverSessionError::invalid_control_payload:
        return "invalid_control_payload";
    case ReceiverSessionError::stream_not_started:
        return "stream_not_started";
    case ReceiverSessionError::wrong_stream:
        return "wrong_stream";
    case ReceiverSessionError::unexpected_packet_type:
        return "unexpected_packet_type";
    case ReceiverSessionError::audio_reassembly_failed:
        return "audio_reassembly_failed";
    }
    return "unknown_receiver_session_error";
}

} // namespace wiretone::protocol
