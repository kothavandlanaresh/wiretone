#include "wiretone/capture/captured_packet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

int fail(std::string_view message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

[[nodiscard]] std::vector<std::byte> encode_floats(
    std::span<const float> samples) {
    std::vector<std::byte> result(samples.size() * sizeof(float));
    if (!samples.empty()) {
        std::memcpy(result.data(), samples.data(), result.size());
    }
    return result;
}

[[nodiscard]] std::int16_t read_s16le(
    std::span<const std::byte> bytes,
    std::size_t sample_index) {
    const std::size_t offset = sample_index * 2U;
    const auto low = std::to_integer<std::uint16_t>(bytes[offset]);
    const auto high = std::to_integer<std::uint16_t>(bytes[offset + 1U]);
    return static_cast<std::int16_t>(low | static_cast<std::uint16_t>(high << 8U));
}

} // namespace

int main() {
    using namespace wiretone::capture;

    {
        const std::array<float, 10> samples{
            -1.5F,
            1.5F,
            -1.0F,
            1.0F,
            -0.5F,
            0.5F,
            0.0F,
            0.25F,
            -0.25F,
            0.0F,
        };
        const auto input = encode_floats(samples);
        std::array<std::byte, 20> output{};

        CapturedPacketView packet{};
        packet.data = input;
        packet.frame_count = 5U;
        packet.flags = captured_packet_flag_discontinuity |
            captured_packet_flag_timestamp_error;
        packet.device_position_frames = 12'345U;
        packet.qpc_position_100ns = 67'890U;

        const auto result = convert_float32_stereo_to_pcm_s16le(packet, output);
        if (!result.ok() || result.output_size != output.size()) {
            return fail("valid floating-point packet was not converted");
        }

        const std::array<std::int16_t, 10> expected{
            -32'768,
            32'767,
            -32'768,
            32'767,
            -16'384,
            16'384,
            0,
            8'192,
            -8'192,
            0,
        };

        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (read_s16le(output, index) != expected[index]) {
                return fail("float-to-PCM conversion or channel order changed");
            }
        }

        if (result.frame_count != packet.frame_count ||
            result.flags != packet.flags ||
            result.device_position_frames != packet.device_position_frames ||
            result.qpc_position_100ns != packet.qpc_position_100ns) {
            return fail("packet metadata was not preserved");
        }
    }

    {
        std::array<std::byte, 12> output{};
        output.fill(std::byte{0x5A});

        CapturedPacketView packet{};
        packet.frame_count = 3U;
        packet.flags = captured_packet_flag_silence |
            captured_packet_flag_discontinuity;
        packet.device_position_frames = 42U;

        const auto result = convert_float32_stereo_to_pcm_s16le(packet, output);
        if (!result.ok() || result.output_size != output.size()) {
            return fail("silent packet was not converted");
        }

        for (const auto value : output) {
            if (value != std::byte{0}) {
                return fail("silent packet did not produce zero PCM");
            }
        }
    }

    {
        std::array<std::byte, 8> output{};
        CapturedPacketView packet{};
        packet.frame_count = 0U;
        if (convert_float32_stereo_to_pcm_s16le(packet, output).error !=
            CapturedPacketError::zero_frame_count) {
            return fail("zero-frame packet was not rejected");
        }

        packet.frame_count = 1U;
        packet.flags = 0x8000'0000U;
        if (convert_float32_stereo_to_pcm_s16le(packet, output).error !=
            CapturedPacketError::unknown_flags) {
            return fail("unknown packet flag was not rejected");
        }
    }

    {
        const std::array<float, 2> samples{0.0F, 0.0F};
        const auto input = encode_floats(samples);
        std::array<std::byte, 3> too_small{};
        too_small.fill(std::byte{0x3C});

        CapturedPacketView packet{};
        packet.data = input;
        packet.frame_count = 1U;

        const auto result = convert_float32_stereo_to_pcm_s16le(packet, too_small);
        if (result.error != CapturedPacketError::output_too_small) {
            return fail("undersized output was not rejected");
        }
        for (const auto value : too_small) {
            if (value != std::byte{0x3C}) {
                return fail("output changed after output_too_small");
            }
        }
    }

    {
        std::array<std::byte, 4> output{};
        CapturedPacketView packet{};
        packet.frame_count = 1U;

        if (convert_float32_stereo_to_pcm_s16le(packet, output).error !=
            CapturedPacketError::data_missing) {
            return fail("missing non-silent data was not rejected");
        }

        const std::array<std::byte, 4> invalid_input{};
        packet.data = invalid_input;
        if (convert_float32_stereo_to_pcm_s16le(packet, output).error !=
            CapturedPacketError::invalid_data_size) {
            return fail("invalid floating-point packet size was not rejected");
        }
    }

    {
        const std::array<float, 2> samples{
            std::numeric_limits<float>::quiet_NaN(),
            0.0F,
        };
        const auto input = encode_floats(samples);
        std::array<std::byte, 4> output{};
        output.fill(std::byte{0x7E});

        CapturedPacketView packet{};
        packet.data = input;
        packet.frame_count = 1U;

        const auto result = convert_float32_stereo_to_pcm_s16le(packet, output);
        if (result.error != CapturedPacketError::non_finite_sample) {
            return fail("non-finite sample was not rejected");
        }
        for (const auto value : output) {
            if (value != std::byte{0x7E}) {
                return fail("output changed after non-finite sample rejection");
            }
        }
    }

    if (normalized_pcm_size_for_frames(960U) != 3'840U ||
        to_string(CapturedPacketError::non_finite_sample) != "non_finite_sample") {
        return fail("captured-packet constants or diagnostics changed");
    }

    std::cout << "PASS: WireTone captured-packet conversion tests\n";
    return 0;
}
