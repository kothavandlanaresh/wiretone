#include "wiretone/protocol/audio_frame.hpp"
#include "wiretone/protocol/control_payload.hpp"
#include "wiretone/protocol/packet.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using wiretone::protocol::AudioFrameError;
using wiretone::protocol::AudioFrameReassembler;
using wiretone::protocol::AudioFrameView;
using wiretone::protocol::EncodedAudioDatagram;
using wiretone::protocol::PacketHeader;
using wiretone::protocol::PacketType;
using wiretone::protocol::ProtocolError;

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

void expect_audio_error(
    AudioFrameError actual,
    AudioFrameError expected,
    std::string_view message) {
    if (actual != expected) {
        std::cerr << "FAIL: " << message << " (expected "
                  << wiretone::protocol::to_string(expected) << ", received "
                  << wiretone::protocol::to_string(actual) << ")\n";
        ++failures;
    }
}

std::array<std::byte, wiretone::protocol::maximum_audio_frame_payload_size>
make_pcm_payload() {
    std::array<std::byte, wiretone::protocol::maximum_audio_frame_payload_size> payload{};
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<std::byte>(index & 0xFFU);
    }
    return payload;
}

AudioFrameView make_frame(std::span<const std::byte> payload) {
    AudioFrameView frame{};
    frame.stream_id = 0x01020304U;
    frame.first_sequence_number = 0xFFFFFFFEU;
    frame.timestamp_samples = 96'000U;
    frame.frame_id = 77U;
    frame.flags = wiretone::protocol::flag_discontinuity;
    frame.payload = payload;
    return frame;
}

std::vector<std::byte> copy_datagram(const EncodedAudioDatagram& datagram) {
    return std::vector<std::byte>(datagram.bytes().begin(), datagram.bytes().end());
}

std::vector<std::byte> make_custom_audio_datagram(
    PacketHeader header,
    std::size_t payload_size,
    std::byte fill = std::byte{0x5A}) {
    header.payload_size = static_cast<std::uint16_t>(payload_size);
    std::vector<std::byte> datagram(
        wiretone::protocol::packet_header_size + payload_size,
        fill);

    const auto encoded = wiretone::protocol::encode_header(header, datagram);
    expect(encoded == ProtocolError::none, "custom audio header did not encode");
    return datagram;
}

void test_pcm_fragmentation_exact_shape() {
    const auto payload = make_pcm_payload();
    const auto result = wiretone::protocol::fragment_audio_frame(make_frame(payload));

    expect(result.ok(), "3,840-byte PCM frame did not fragment");
    expect(result.datagram_count == 4U, "PCM frame did not produce four datagrams");
    expect(result.next_sequence_number == 2U, "sequence wrap result is incorrect");

    const std::array<std::size_t, 4> expected_payload_sizes{
        1'168U,
        1'168U,
        1'168U,
        336U,
    };
    const std::array<std::uint32_t, 4> expected_sequences{
        0xFFFFFFFEU,
        0xFFFFFFFFU,
        0U,
        1U,
    };

    for (std::size_t index = 0; index < result.datagram_count; ++index) {
        const auto parsed = wiretone::protocol::parse_datagram(
            result.datagrams[index].bytes());
        expect(parsed.ok(), "fragmented PCM datagram did not parse");
        if (!parsed.ok()) {
            continue;
        }

        expect(parsed.header.type == PacketType::audio, "fragment type changed");
        expect(parsed.header.stream_id == 0x01020304U, "fragment stream changed");
        expect(parsed.header.frame_id == 77U, "fragment frame id changed");
        expect(parsed.header.timestamp_samples == 96'000U, "fragment timestamp changed");
        expect(parsed.header.flags == wiretone::protocol::flag_discontinuity, "flags changed");
        expect(parsed.header.sequence_number == expected_sequences[index], "sequence changed");
        expect(parsed.header.fragment_index == index, "fragment index changed");
        expect(parsed.header.fragment_count == 4U, "fragment count changed");
        expect(parsed.payload.size() == expected_payload_sizes[index], "payload split changed");
        expect(
            result.datagrams[index].size ==
                wiretone::protocol::packet_header_size + expected_payload_sizes[index],
            "datagram size changed");
    }
}

void test_unfragmented_and_silence_frames() {
    std::array<std::byte, 240> opus_payload{};
    std::fill(opus_payload.begin(), opus_payload.end(), std::byte{0x7B});

    auto opus_frame = make_frame(opus_payload);
    opus_frame.first_sequence_number = 100U;
    opus_frame.flags = 0U;
    const auto opus = wiretone::protocol::fragment_audio_frame(opus_frame);
    expect(opus.ok(), "small encoded frame did not fragment");
    expect(opus.datagram_count == 1U, "small encoded frame was fragmented");
    expect(opus.next_sequence_number == 101U, "small-frame sequence result changed");

    AudioFrameView silence{};
    silence.stream_id = 9U;
    silence.first_sequence_number = 200U;
    silence.timestamp_samples = 1'920U;
    silence.frame_id = 3U;
    silence.flags = wiretone::protocol::flag_silence;

    const auto silent = wiretone::protocol::fragment_audio_frame(silence);
    expect(silent.ok(), "explicit silence frame did not encode");
    expect(silent.datagram_count == 1U, "silence frame was fragmented");
    expect(
        silent.datagrams[0].size == wiretone::protocol::packet_header_size,
        "silence datagram contains payload bytes");

    const auto parsed = wiretone::protocol::parse_datagram(silent.datagrams[0].bytes());
    expect(parsed.ok(), "silence datagram did not parse");
    expect(parsed.payload.empty(), "silence datagram payload is not empty");
}

void test_fragmentation_rejections() {
    std::array<std::byte, 1> one_byte{std::byte{1}};

    auto frame = make_frame(one_byte);
    frame.stream_id = 0U;
    expect_audio_error(
        wiretone::protocol::fragment_audio_frame(frame).error,
        AudioFrameError::zero_stream_id,
        "zero stream id was fragmented");

    frame = make_frame(one_byte);
    frame.frame_id = 0U;
    expect_audio_error(
        wiretone::protocol::fragment_audio_frame(frame).error,
        AudioFrameError::zero_frame_id,
        "zero frame id was fragmented");

    frame = make_frame(one_byte);
    frame.flags = 0x8000U;
    expect_audio_error(
        wiretone::protocol::fragment_audio_frame(frame).error,
        AudioFrameError::unknown_flags,
        "unknown audio flag was fragmented");

    frame = make_frame({});
    frame.flags = 0U;
    expect_audio_error(
        wiretone::protocol::fragment_audio_frame(frame).error,
        AudioFrameError::empty_payload_without_silence,
        "empty non-silence frame was fragmented");

    frame = make_frame(one_byte);
    frame.flags = wiretone::protocol::flag_silence;
    expect_audio_error(
        wiretone::protocol::fragment_audio_frame(frame).error,
        AudioFrameError::silence_payload_present,
        "silence frame with payload was fragmented");

    std::array<std::byte, wiretone::protocol::maximum_audio_frame_payload_size + 1U> oversized{};
    frame = make_frame(oversized);
    expect_audio_error(
        wiretone::protocol::fragment_audio_frame(frame).error,
        AudioFrameError::payload_too_large,
        "oversized frame was fragmented");
}

void test_out_of_order_reassembly() {
    const auto payload = make_pcm_payload();
    const auto fragmented = wiretone::protocol::fragment_audio_frame(make_frame(payload));
    expect(fragmented.ok(), "reassembly fixture did not fragment");

    AudioFrameReassembler reassembler{};
    const std::array<std::size_t, 4> order{2U, 0U, 3U, 1U};

    for (std::size_t position = 0; position < order.size(); ++position) {
        const auto result = reassembler.accept_datagram(
            fragmented.datagrams[order[position]].bytes(),
            10U + position);
        expect(result.ok(), "valid out-of-order fragment was rejected");

        const bool should_complete = position + 1U == order.size();
        expect(result.frame_completed == should_complete, "completion occurred at wrong time");

        if (result.frame_completed) {
            expect(result.frame.stream_id == 0x01020304U, "reassembled stream changed");
            expect(result.frame.frame_id == 77U, "reassembled frame id changed");
            expect(result.frame.first_sequence_number == 0xFFFFFFFEU, "base sequence changed");
            expect(result.frame.timestamp_samples == 96'000U, "timestamp changed");
            expect(result.frame.flags == wiretone::protocol::flag_discontinuity, "flags changed");
            expect(result.frame.payload_size == payload.size(), "reassembled size changed");
            expect(
                std::equal(payload.begin(), payload.end(), result.frame.payload().begin()),
                "reassembled payload changed");
        }
    }

    expect(reassembler.in_flight_frame_count() == 0U, "completed frame remained in window");
}

void test_duplicate_and_inconsistent_fragments() {
    const auto payload = make_pcm_payload();
    const auto fragmented = wiretone::protocol::fragment_audio_frame(make_frame(payload));

    AudioFrameReassembler duplicate_reassembler{};
    const auto first = duplicate_reassembler.accept_datagram(
        fragmented.datagrams[0].bytes(),
        1U);
    expect(first.ok(), "first fragment was rejected");

    const auto duplicate = duplicate_reassembler.accept_datagram(
        fragmented.datagrams[0].bytes(),
        2U);
    expect_audio_error(
        duplicate.error,
        AudioFrameError::duplicate_fragment,
        "duplicate fragment was accepted");
    expect(
        duplicate_reassembler.in_flight_frame_count() == 1U,
        "duplicate fragment corrupted the in-flight window");

    AudioFrameReassembler inconsistent_reassembler{};
    expect(
        inconsistent_reassembler.accept_datagram(
            fragmented.datagrams[0].bytes(),
            10U).ok(),
        "inconsistent fixture first fragment rejected");

    auto changed = copy_datagram(fragmented.datagrams[1]);
    auto parsed = wiretone::protocol::parse_datagram(changed);
    expect(parsed.ok(), "fragment for metadata mutation did not parse");
    if (parsed.ok()) {
        auto header = parsed.header;
        ++header.timestamp_samples;
        expect(
            wiretone::protocol::encode_header(header, changed) == ProtocolError::none,
            "mutated metadata header did not encode");
    }

    const auto inconsistent = inconsistent_reassembler.accept_datagram(changed, 11U);
    expect_audio_error(
        inconsistent.error,
        AudioFrameError::inconsistent_fragment_metadata,
        "inconsistent timestamp was accepted");
}

void test_reassembly_shape_rejections() {
    PacketHeader header{};
    header.type = PacketType::audio;
    header.stream_id = 1U;
    header.sequence_number = 1U;
    header.timestamp_samples = 960U;
    header.frame_id = 1U;
    header.fragment_count = 5U;
    header.fragment_index = 0U;

    AudioFrameReassembler reassembler{};
    const auto too_many = make_custom_audio_datagram(
        header,
        wiretone::protocol::maximum_payload_size);
    expect_audio_error(
        reassembler.accept_datagram(too_many, 1U).error,
        AudioFrameError::fragment_count_exceeds_limit,
        "five-fragment frame was accepted");

    header.fragment_count = 3U;
    header.fragment_index = 0U;
    const auto short_nonfinal = make_custom_audio_datagram(header, 10U);
    expect_audio_error(
        reassembler.accept_datagram(short_nonfinal, 2U).error,
        AudioFrameError::noncanonical_fragment_size,
        "short non-final fragment was accepted");

    header.fragment_count = 4U;
    header.fragment_index = 3U;
    header.sequence_number = 4U;
    const auto oversized_final = make_custom_audio_datagram(header, 337U);
    expect_audio_error(
        reassembler.accept_datagram(oversized_final, 3U).error,
        AudioFrameError::payload_too_large,
        "logical frame larger than 3,840 bytes was accepted");

    PacketHeader heartbeat{};
    heartbeat.type = PacketType::heartbeat;
    heartbeat.stream_id = 1U;
    heartbeat.payload_size = static_cast<std::uint16_t>(
        wiretone::protocol::heartbeat_payload_size);
    std::vector<std::byte> heartbeat_datagram(
        wiretone::protocol::packet_header_size +
        wiretone::protocol::heartbeat_payload_size);
    expect(
        wiretone::protocol::encode_header(heartbeat, heartbeat_datagram) ==
            ProtocolError::none,
        "heartbeat fixture did not encode");
    expect_audio_error(
        reassembler.accept_datagram(heartbeat_datagram, 4U).error,
        AudioFrameError::not_audio_packet,
        "control packet entered audio reassembly");

    std::array<std::byte, 3> malformed{};
    const auto malformed_result = reassembler.accept_datagram(malformed, 5U);
    expect_audio_error(
        malformed_result.error,
        AudioFrameError::malformed_datagram,
        "malformed datagram entered audio reassembly");
    expect(
        malformed_result.protocol_error == ProtocolError::datagram_too_small,
        "underlying protocol error was not preserved");
}

void test_bounded_window_and_expiry() {
    const auto payload = make_pcm_payload();
    AudioFrameReassembler reassembler{};

    for (std::size_t index = 0;
         index < wiretone::protocol::maximum_in_flight_audio_frames;
         ++index) {
        auto frame = make_frame(payload);
        frame.stream_id = 10U;
        frame.frame_id = static_cast<std::uint32_t>(index + 1U);
        frame.first_sequence_number = static_cast<std::uint32_t>(index * 10U);
        const auto fragmented = wiretone::protocol::fragment_audio_frame(frame);
        expect(fragmented.ok(), "window fixture did not fragment");
        const auto accepted = reassembler.accept_datagram(
            fragmented.datagrams[0].bytes(),
            0U);
        expect(accepted.ok(), "window fixture fragment was rejected");
    }

    expect(
        reassembler.in_flight_frame_count() ==
            wiretone::protocol::maximum_in_flight_audio_frames,
        "reassembly window did not fill to its fixed bound");

    auto ninth_frame = make_frame(payload);
    ninth_frame.stream_id = 10U;
    ninth_frame.frame_id = 99U;
    ninth_frame.first_sequence_number = 900U;
    const auto ninth = wiretone::protocol::fragment_audio_frame(ninth_frame);

    const auto full = reassembler.accept_datagram(ninth.datagrams[0].bytes(), 249U);
    expect_audio_error(
        full.error,
        AudioFrameError::reassembly_window_full,
        "ninth in-flight frame exceeded the fixed window");

    const auto after_expiry = reassembler.accept_datagram(
        ninth.datagrams[0].bytes(),
        wiretone::protocol::audio_frame_reassembly_timeout_ms);
    expect(after_expiry.ok(), "new frame was rejected after deterministic expiry");
    expect(
        after_expiry.expired_frame_count ==
            wiretone::protocol::maximum_in_flight_audio_frames,
        "expiry did not report all stale frames");
    expect(reassembler.in_flight_frame_count() == 1U, "expiry window state is incorrect");

    const auto backwards = reassembler.expire_incomplete(
        wiretone::protocol::audio_frame_reassembly_timeout_ms - 1U);
    expect_audio_error(
        backwards.error,
        AudioFrameError::non_monotonic_time,
        "backwards clock value was accepted");
}

void test_silence_reassembly() {
    AudioFrameView silence{};
    silence.stream_id = 11U;
    silence.first_sequence_number = 22U;
    silence.timestamp_samples = 33U;
    silence.frame_id = 44U;
    silence.flags = wiretone::protocol::flag_silence;

    const auto fragmented = wiretone::protocol::fragment_audio_frame(silence);
    AudioFrameReassembler reassembler{};
    const auto completed = reassembler.accept_datagram(
        fragmented.datagrams[0].bytes(),
        1U);

    expect(completed.ok(), "silence datagram was rejected by reassembler");
    expect(completed.frame_completed, "silence frame did not complete immediately");
    expect(completed.frame.payload().empty(), "silence frame gained payload bytes");
    expect(
        completed.frame.flags == wiretone::protocol::flag_silence,
        "silence flag changed during reassembly");
}

} // namespace

int main() {
    test_pcm_fragmentation_exact_shape();
    test_unfragmented_and_silence_frames();
    test_fragmentation_rejections();
    test_out_of_order_reassembly();
    test_duplicate_and_inconsistent_fragments();
    test_reassembly_shape_rejections();
    test_bounded_window_and_expiry();
    test_silence_reassembly();

    if (failures != 0) {
        std::cerr << failures << " audio-frame test(s) failed\n";
        return 1;
    }

    std::cout << "PASS: WireTone audio fragmentation and bounded reassembly tests\n";
    return 0;
}
