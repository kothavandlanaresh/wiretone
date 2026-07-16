#include "wiretone/protocol/session.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace wiretone::protocol;

int failures = 0;

void fail(std::string_view message) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void expect(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void expect_error(
    ReceiverSessionError actual,
    ReceiverSessionError expected,
    std::string_view message) {
    if (actual != expected) {
        std::cerr << "FAIL: " << message << " (expected "
                  << to_string(expected) << ", received "
                  << to_string(actual) << ")\n";
        ++failures;
    }
}

std::vector<std::byte> make_control_datagram(
    PacketType type,
    std::uint32_t stream_id,
    std::uint32_t sequence_number,
    std::span<const std::byte> payload) {
    PacketHeader header{};
    header.type = type;
    header.stream_id = stream_id;
    header.sequence_number = sequence_number;
    header.payload_size = static_cast<std::uint16_t>(payload.size());

    std::vector<std::byte> datagram(packet_header_size + payload.size());
    const auto encoded = encode_header(header, datagram);
    expect(encoded == ProtocolError::none, "control header did not encode");
    std::copy(payload.begin(), payload.end(), datagram.begin() + static_cast<std::ptrdiff_t>(packet_header_size));
    return datagram;
}

std::vector<std::byte> make_stream_start(
    std::uint32_t stream_id,
    std::uint32_t sequence_number = 1U) {
    StreamStartPayload value{};
    std::array<std::byte, stream_start_payload_size> payload{};
    expect(encode_stream_start(value, payload) == ControlPayloadError::none,
           "stream_start payload did not encode");
    return make_control_datagram(PacketType::stream_start, stream_id, sequence_number, payload);
}

std::vector<std::byte> make_stream_stop(
    std::uint32_t stream_id,
    std::uint32_t sequence_number = 2U) {
    StreamStopPayload value{};
    std::array<std::byte, stream_stop_payload_size> payload{};
    expect(encode_stream_stop(value, payload) == ControlPayloadError::none,
           "stream_stop payload did not encode");
    return make_control_datagram(PacketType::stream_stop, stream_id, sequence_number, payload);
}

std::vector<std::byte> make_heartbeat(
    std::uint32_t stream_id,
    std::uint64_t sender_time_us,
    std::uint32_t sequence_number = 3U) {
    HeartbeatPayload value{};
    value.sender_monotonic_time_us = sender_time_us;
    std::array<std::byte, heartbeat_payload_size> payload{};
    expect(encode_heartbeat(value, payload) == ControlPayloadError::none,
           "heartbeat payload did not encode");
    return make_control_datagram(PacketType::heartbeat, stream_id, sequence_number, payload);
}


std::vector<std::byte> make_invalid_error_payload(
    std::uint32_t stream_id,
    std::uint32_t sequence_number = 4U) {
    const std::array<std::byte, 3> payload{
        std::byte{0x00},
        std::byte{0x01},
        std::byte{0x80},
    };
    return make_control_datagram(PacketType::error, stream_id, sequence_number, payload);
}

std::array<std::byte, 240> make_audio_payload(std::byte fill) {
    std::array<std::byte, 240> payload{};
    std::fill(payload.begin(), payload.end(), fill);
    return payload;
}

AudioFragmentationResult make_audio(
    std::uint32_t stream_id,
    std::uint32_t frame_id,
    std::uint32_t sequence_number,
    std::uint64_t timestamp_samples,
    std::span<const std::byte> payload,
    std::uint16_t flags = 0U) {
    AudioFrameView frame{};
    frame.stream_id = stream_id;
    frame.frame_id = frame_id;
    frame.first_sequence_number = sequence_number;
    frame.timestamp_samples = timestamp_samples;
    frame.flags = flags;
    frame.payload = payload;
    return fragment_audio_frame(frame);
}

void test_requires_stream_start_and_rejects_wrong_stream() {
    ReceiverSession session{};
    const auto payload = make_audio_payload(std::byte{0x11});
    const auto audio = make_audio(7U, 1U, 10U, 0U, payload);
    expect(audio.ok(), "audio fixture did not encode");

    const auto before_start = session.accept_datagram(audio.datagrams[0].bytes(), 1U);
    expect_error(before_start.error, ReceiverSessionError::stream_not_started,
                 "audio before stream_start was accepted");

    const auto started = session.accept_datagram(make_stream_start(7U), 2U);
    expect(started.ok(), "stream_start was rejected");
    expect(started.event == ReceiverSessionEvent::stream_started,
           "stream_start event changed");
    expect(session.streaming(), "session did not enter streaming state");
    expect(session.active_stream_id() == 7U, "active stream id changed");

    const auto wrong_audio = make_audio(8U, 1U, 20U, 0U, payload);
    const auto wrong = session.accept_datagram(wrong_audio.datagrams[0].bytes(), 3U);
    expect_error(wrong.error, ReceiverSessionError::wrong_stream,
                 "wrong-stream audio was accepted");
    expect(session.counters().rejected_before_start == 1U,
           "before-start rejection counter changed");
    expect(session.counters().wrong_stream_datagrams == 1U,
           "wrong-stream counter changed");
}

void test_frame_queue_discontinuity_and_overflow() {
    ReceiverSession session{};
    expect(session.accept_datagram(make_stream_start(9U), 0U).ok(),
           "queue test stream_start failed");

    for (std::uint32_t index = 0U;
         index < static_cast<std::uint32_t>(receiver_frame_queue_capacity + 2U);
         ++index) {
        const auto payload = make_audio_payload(static_cast<std::byte>(index));
        const auto audio = make_audio(
            9U,
            index + 1U,
            100U + index,
            static_cast<std::uint64_t>(index) * 960U,
            payload);
        const auto accepted = session.accept_datagram(audio.datagrams[0].bytes(), index + 1U);
        expect(accepted.ok() && accepted.frame_queued,
               "valid complete frame was not queued");
    }

    expect(session.queued_frame_count() == receiver_frame_queue_capacity,
           "bounded queue capacity changed");
    expect(session.counters().dropped_frames == 2U,
           "oldest-frame drop counter changed");

    ReassembledAudioFrame popped{};
    expect(session.pop_frame(popped), "bounded queue did not pop");
    expect(popped.frame_id == 3U, "queue did not discard the two oldest frames");

    const auto discontinuity_payload = make_audio_payload(std::byte{0x7F});
    const auto discontinuity = make_audio(
        9U,
        99U,
        500U,
        99U * 960U,
        discontinuity_payload,
        flag_discontinuity);
    const auto accepted = session.accept_datagram(discontinuity.datagrams[0].bytes(), 20U);
    expect(accepted.ok() && accepted.frame_queued,
           "discontinuity frame was not queued");
    expect(session.queued_frame_count() == 1U,
           "discontinuity did not flush stale queued frames");
    expect(session.counters().discontinuities == 1U,
           "discontinuity counter changed");
}

void test_stop_replacement_and_heartbeat_liveness() {
    ReceiverSession session{};
    expect(session.accept_datagram(make_stream_start(10U), 100U).ok(),
           "initial stream_start failed");

    const auto heartbeat = session.accept_datagram(make_heartbeat(10U, 123'456U), 200U);
    expect(heartbeat.ok(), "heartbeat was rejected");
    expect(heartbeat.event == ReceiverSessionEvent::heartbeat_received,
           "heartbeat event changed");
    expect(session.last_sender_monotonic_time_us() == 123'456U,
           "heartbeat sender time changed");

    const auto replacement = session.accept_datagram(make_stream_start(11U, 4U), 300U);
    expect(replacement.ok(), "replacement stream_start failed");
    expect(replacement.event == ReceiverSessionEvent::stream_replaced,
           "replacement event changed");
    expect(session.active_stream_id() == 11U,
           "replacement did not activate new stream");

    const auto stale_stop = session.accept_datagram(make_stream_stop(10U), 301U);
    expect_error(stale_stop.error, ReceiverSessionError::wrong_stream,
                 "old stream_stop stopped replacement stream");

    const auto stopped = session.accept_datagram(make_stream_stop(11U), 302U);
    expect(stopped.ok(), "active stream_stop failed");
    expect(stopped.event == ReceiverSessionEvent::stream_stopped,
           "stream_stop event changed");
    expect(!session.streaming(), "session remained active after stream_stop");

    const auto after_stop = session.accept_datagram(make_heartbeat(11U, 9U), 303U);
    expect_error(after_stop.error, ReceiverSessionError::stream_not_started,
                 "heartbeat after stop was accepted");

    expect(session.accept_datagram(make_stream_start(12U), 1'000U).ok(),
           "timeout stream_start failed");
    const auto before_timeout = session.advance_time(3'999U);
    expect(before_timeout.ok() && session.streaming(),
           "session timed out too early");

    const auto timed_out = session.advance_time(4'000U);
    expect(timed_out.ok(), "timeout tick returned an error");
    expect(timed_out.event == ReceiverSessionEvent::session_timed_out,
           "timeout event changed");
    expect(!session.streaming(), "timed-out session remained active");
    expect(session.counters().session_timeouts == 1U,
           "timeout counter changed");
}

void test_fragment_counters_and_non_monotonic_time() {
    ReceiverSession session{};
    expect(session.accept_datagram(make_stream_start(20U), 10U).ok(),
           "fragment counter stream_start failed");

    std::array<std::byte, maximum_audio_frame_payload_size> payload{};
    const auto audio = make_audio(20U, 1U, 50U, 0U, payload);
    expect(audio.ok() && audio.datagram_count == 4U,
           "fragment counter fixture did not split");

    expect(session.accept_datagram(audio.datagrams[0].bytes(), 11U).ok(),
           "first fragment failed");
    const auto duplicate = session.accept_datagram(audio.datagrams[0].bytes(), 12U);
    expect_error(duplicate.error, ReceiverSessionError::audio_reassembly_failed,
                 "duplicate fragment did not surface session error");
    expect(duplicate.audio_error == AudioFrameError::duplicate_fragment,
           "duplicate fragment detail changed");
    expect(session.counters().duplicate_fragments == 1U,
           "duplicate fragment counter changed");

    const auto backwards = session.advance_time(11U);
    expect_error(backwards.error, ReceiverSessionError::non_monotonic_time,
                 "backwards session time was accepted");
}


void test_malformed_expired_and_inconsistent_counters() {
    ReceiverSession session{};
    expect(session.accept_datagram(make_stream_start(30U), 0U).ok(),
           "counter test stream_start failed");

    std::array<std::byte, maximum_audio_frame_payload_size> payload{};
    const auto first_frame = make_audio(30U, 1U, 100U, 0U, payload);
    expect(first_frame.ok() && first_frame.datagram_count == 4U,
           "counter test frame did not split");
    expect(session.accept_datagram(first_frame.datagrams[0].bytes(), 1U).ok(),
           "counter test first fragment failed");

    const auto expiry = session.advance_time(251U);
    expect(expiry.ok(), "incomplete-frame expiry returned an error");
    expect(expiry.expired_frame_count == 1U,
           "incomplete-frame expiry count changed");
    expect(session.counters().expired_frames == 1U,
           "expired-frame counter changed");

    std::array<std::byte, 3> malformed{};
    const auto malformed_result = session.accept_datagram(malformed, 252U);
    expect_error(malformed_result.error, ReceiverSessionError::malformed_datagram,
                 "short malformed datagram was accepted");
    expect(session.counters().malformed_datagrams == 1U,
           "malformed datagram counter changed");

    const auto consistent = make_audio(30U, 2U, 200U, 960U, payload);
    const auto inconsistent = make_audio(30U, 2U, 200U, 1'920U, payload);
    expect(session.accept_datagram(consistent.datagrams[0].bytes(), 253U).ok(),
           "inconsistent fixture first fragment failed");
    const auto mismatch = session.accept_datagram(inconsistent.datagrams[1].bytes(), 254U);
    expect_error(mismatch.error, ReceiverSessionError::audio_reassembly_failed,
                 "inconsistent fragment metadata was accepted");
    expect(mismatch.audio_error == AudioFrameError::inconsistent_fragment_metadata,
           "inconsistent fragment detail changed");
    expect(session.counters().inconsistent_fragments == 1U,
           "inconsistent-fragment counter changed");
}


void test_invalid_control_does_not_refresh_liveness() {
    ReceiverSession session{};
    expect(session.accept_datagram(make_stream_start(40U), 0U).ok(),
           "invalid-control liveness stream_start failed");

    const auto invalid = session.accept_datagram(make_invalid_error_payload(40U), 2'999U);
    expect_error(invalid.error, ReceiverSessionError::invalid_control_payload,
                 "malformed UTF-8 error payload was accepted");
    expect(session.counters().invalid_control_payloads == 1U,
           "invalid-control counter changed");

    const auto timeout = session.advance_time(3'000U);
    expect(timeout.ok(), "timeout after invalid control returned an error");
    expect(timeout.event == ReceiverSessionEvent::session_timed_out,
           "invalid control incorrectly refreshed liveness");
}

} // namespace

int main() {
    test_requires_stream_start_and_rejects_wrong_stream();
    test_frame_queue_discontinuity_and_overflow();
    test_stop_replacement_and_heartbeat_liveness();
    test_fragment_counters_and_non_monotonic_time();
    test_malformed_expired_and_inconsistent_counters();
    test_invalid_control_does_not_refresh_liveness();

    if (failures != 0) {
        std::cerr << failures << " session test(s) failed\n";
        return 1;
    }

    std::cout << "WireTone receiver session tests passed\n";
    return 0;
}
