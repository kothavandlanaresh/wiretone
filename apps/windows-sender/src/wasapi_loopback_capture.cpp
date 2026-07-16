#include "wiretone/windows/wasapi_loopback_capture.hpp"

#include <windows.h>

#include <audioclient.h>
#include <combaseapi.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propkeydef.h>
#include <propsys.h>
#include <propvarutil.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace wiretone::windows {
namespace {

using Microsoft::WRL::ComPtr;

[[nodiscard]] std::string wide_to_utf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }

    const int required_size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required_size <= 1) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required_size), '\0');
    const int converted_size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        -1,
        result.data(),
        required_size,
        nullptr,
        nullptr);

    if (converted_size != required_size) {
        return {};
    }

    result.resize(static_cast<std::size_t>(required_size - 1));
    return result;
}

[[nodiscard]] bool guid_equal(const GUID& left, const GUID& right) noexcept {
    return InlineIsEqualGUID(left, right) != 0;
}

[[nodiscard]] WasapiMixFormat inspect_mix_format(const WAVEFORMATEX& format) noexcept {
    WasapiMixFormat result{};
    result.sample_rate = format.nSamplesPerSec;
    result.channel_count = format.nChannels;
    result.container_bits_per_sample = format.wBitsPerSample;
    result.valid_bits_per_sample = format.wBitsPerSample;

    if (format.wFormatTag == WAVE_FORMAT_PCM) {
        result.sample_kind = WasapiSampleKind::integer_pcm;
        return result;
    }

    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        result.sample_kind = WasapiSampleKind::floating_point;
        return result;
    }

    if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        format.cbSize < static_cast<WORD>(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))) {
        return result;
    }

    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&format);
    result.extensible = true;
    result.valid_bits_per_sample = extensible->Samples.wValidBitsPerSample;
    result.channel_mask = extensible->dwChannelMask;

    if (guid_equal(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
        result.sample_kind = WasapiSampleKind::integer_pcm;
    } else if (guid_equal(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        result.sample_kind = WasapiSampleKind::floating_point;
    }

    return result;
}

[[nodiscard]] bool supports_phase_2_2_normalization(
    const WasapiMixFormat& format,
    const WAVEFORMATEX& native_format) noexcept {
    return format.sample_rate == capture::normalized_sample_rate &&
        format.channel_count == capture::normalized_channel_count &&
        format.container_bits_per_sample == 32U &&
        format.valid_bits_per_sample == 32U &&
        format.sample_kind == WasapiSampleKind::floating_point &&
        native_format.nBlockAlign == capture::float32_stereo_bytes_per_frame;
}

[[nodiscard]] bool is_device_invalidated(HRESULT result) noexcept {
    return result == AUDCLNT_E_DEVICE_INVALIDATED ||
        result == AUDCLNT_E_RESOURCES_INVALIDATED;
}

[[nodiscard]] std::uint32_t normalize_buffer_flags(DWORD flags) noexcept {
    std::uint32_t result = 0U;
    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0U) {
        result |= capture::captured_packet_flag_silence;
    }
    if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0U) {
        result |= capture::captured_packet_flag_discontinuity;
    }
    if ((flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) != 0U) {
        result |= capture::captured_packet_flag_timestamp_error;
    }
    return result;
}

inline constexpr DWORD known_wasapi_buffer_flags =
    AUDCLNT_BUFFERFLAGS_SILENT |
    AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY |
    AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR;

} // namespace

struct WasapiLoopbackCapture::Impl {
    capture::CaptureLifecycle lifecycle{};
    WasapiCaptureError error{WasapiCaptureError::none};
    capture::CapturedPacketError conversion_error{capture::CapturedPacketError::none};
    capture::PcmFrameAssemblerError frame_assembler_error{
        capture::PcmFrameAssemblerError::none};
    HRESULT native_result{S_OK};
    bool com_initialized{false};
    bool audio_started{false};
    bool normalization_supported{false};
    ComPtr<IMMDeviceEnumerator> device_enumerator{};
    ComPtr<IMMDevice> endpoint{};
    ComPtr<IAudioClient> audio_client{};
    ComPtr<IAudioCaptureClient> capture_client{};
    WAVEFORMATEX* mix_format{nullptr};
    std::string endpoint_name{};
    std::string endpoint_id{};
    WasapiMixFormat inspected_mix_format{};
    UINT32 endpoint_buffer_frames{0};
    std::vector<std::byte> normalized_pcm_scratch{};
    capture::PcmFrameAssembler frame_assembler{};
    std::uint64_t packets_drained{0};
    std::uint64_t frames_drained{0};
    std::uint64_t pcm_bytes_produced{0};
    std::uint64_t silent_packets{0};
    std::uint64_t discontinuity_packets{0};
    std::uint64_t timestamp_error_packets{0};
    std::uint64_t empty_poll_count{0};
    std::uint64_t last_device_position_frames{0};
    std::uint64_t last_qpc_position_100ns{0};
    std::uint64_t completed_pcm_frames{0};
    std::uint64_t silent_pcm_frames{0};
    std::uint64_t discontinuity_pcm_frames{0};
    std::uint64_t timestamp_error_pcm_frames{0};
    std::uint64_t dropped_partial_pcm_frames{0};
    std::uint64_t last_completed_pcm_sequence{0};
    std::uint64_t last_completed_device_position_frames{0};
    std::uint64_t last_completed_qpc_position_100ns{0};

    static void on_completed_frame(
        const capture::PcmFrameView& frame,
        void* context) noexcept {
        auto& self = *static_cast<Impl*>(context);
        ++self.completed_pcm_frames;
        self.last_completed_pcm_sequence = frame.sequence_number;
        self.last_completed_device_position_frames = frame.device_position_frames;
        self.last_completed_qpc_position_100ns = frame.qpc_position_100ns;

        if ((frame.flags & capture::captured_packet_flag_silence) != 0U) {
            ++self.silent_pcm_frames;
        }
        if ((frame.flags & capture::captured_packet_flag_discontinuity) != 0U) {
            ++self.discontinuity_pcm_frames;
        }
        if ((frame.flags & capture::captured_packet_flag_timestamp_error) != 0U) {
            ++self.timestamp_error_pcm_frames;
        }
    }

    void reset_run_state() noexcept {
        (void)frame_assembler.reset();
        packets_drained = 0U;
        frames_drained = 0U;
        pcm_bytes_produced = 0U;
        silent_packets = 0U;
        discontinuity_packets = 0U;
        timestamp_error_packets = 0U;
        empty_poll_count = 0U;
        last_device_position_frames = 0U;
        last_qpc_position_100ns = 0U;
        completed_pcm_frames = 0U;
        silent_pcm_frames = 0U;
        discontinuity_pcm_frames = 0U;
        timestamp_error_pcm_frames = 0U;
        dropped_partial_pcm_frames = 0U;
        last_completed_pcm_sequence = 0U;
        last_completed_device_position_frames = 0U;
        last_completed_qpc_position_100ns = 0U;
    }

    ~Impl() {
        if (audio_started && audio_client) {
            (void)audio_client->Stop();
            audio_started = false;
        }

        if (mix_format != nullptr) {
            CoTaskMemFree(mix_format);
            mix_format = nullptr;
        }

        capture_client.Reset();
        audio_client.Reset();
        endpoint.Reset();
        device_enumerator.Reset();

        if (com_initialized) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool fail(
        WasapiCaptureError new_error,
        HRESULT result,
        capture::CapturedPacketError packet_error =
            capture::CapturedPacketError::none,
        capture::PcmFrameAssemblerError assembler_error =
            capture::PcmFrameAssemblerError::none) noexcept {
        if (audio_started && audio_client) {
            (void)audio_client->Stop();
            audio_started = false;
        }

        if (frame_assembler.reset()) {
            ++dropped_partial_pcm_frames;
        }
        error = new_error;
        conversion_error = packet_error;
        frame_assembler_error = assembler_error;
        native_result = result;
        lifecycle.fail();
        return false;
    }

    [[nodiscard]] bool fail_capture_call(
        WasapiCaptureError default_error,
        HRESULT result) noexcept {
        return fail(
            is_device_invalidated(result)
                ? WasapiCaptureError::device_invalidated
                : default_error,
            result);
    }

    void clear_error() noexcept {
        error = WasapiCaptureError::none;
        conversion_error = capture::CapturedPacketError::none;
        frame_assembler_error = capture::PcmFrameAssemblerError::none;
        native_result = S_OK;
    }
};

WasapiLoopbackCapture::WasapiLoopbackCapture()
    : impl_(std::make_unique<Impl>()) {}

WasapiLoopbackCapture::~WasapiLoopbackCapture() = default;

bool WasapiLoopbackCapture::initialize() noexcept {
    if (!impl_ || impl_->lifecycle.state() != capture::CaptureState::idle) {
        if (impl_) {
            return impl_->fail(WasapiCaptureError::invalid_state, E_UNEXPECTED);
        }
        return false;
    }

    impl_->clear_error();

    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::com_initialization_failed, result);
    }
    impl_->com_initialized = true;

    result = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&impl_->device_enumerator));
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::device_enumerator_creation_failed, result);
    }

    result = impl_->device_enumerator->GetDefaultAudioEndpoint(
        eRender,
        eConsole,
        &impl_->endpoint);
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::default_render_endpoint_failed, result);
    }

    LPWSTR endpoint_id = nullptr;
    result = impl_->endpoint->GetId(&endpoint_id);
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::endpoint_id_failed, result);
    }
    impl_->endpoint_id = wide_to_utf8(endpoint_id);
    CoTaskMemFree(endpoint_id);

    ComPtr<IPropertyStore> property_store;
    result = impl_->endpoint->OpenPropertyStore(STGM_READ, &property_store);
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::endpoint_property_store_failed, result);
    }

    PROPVARIANT friendly_name;
    PropVariantInit(&friendly_name);
    result = property_store->GetValue(PKEY_Device_FriendlyName, &friendly_name);
    if (FAILED(result)) {
        PropVariantClear(&friendly_name);
        return impl_->fail(WasapiCaptureError::endpoint_name_failed, result);
    }

    if (friendly_name.vt == VT_LPWSTR) {
        impl_->endpoint_name = wide_to_utf8(friendly_name.pwszVal);
    }
    PropVariantClear(&friendly_name);

    IAudioClient* audio_client = nullptr;
    result = impl_->endpoint->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(&audio_client));
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::audio_client_activation_failed, result);
    }
    impl_->audio_client.Attach(audio_client);

    result = impl_->audio_client->GetMixFormat(&impl_->mix_format);
    if (FAILED(result) || impl_->mix_format == nullptr) {
        return impl_->fail(
            WasapiCaptureError::mix_format_failed,
            FAILED(result) ? result : E_POINTER);
    }
    impl_->inspected_mix_format = inspect_mix_format(*impl_->mix_format);

    result = impl_->audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0,
        0,
        impl_->mix_format,
        nullptr);
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::loopback_initialization_failed, result);
    }

    IAudioCaptureClient* capture_client = nullptr;
    result = impl_->audio_client->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(&capture_client));
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::capture_service_failed, result);
    }
    impl_->capture_client.Attach(capture_client);

    result = impl_->audio_client->GetBufferSize(&impl_->endpoint_buffer_frames);
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::buffer_size_failed, result);
    }

    impl_->normalization_supported = supports_phase_2_2_normalization(
        impl_->inspected_mix_format,
        *impl_->mix_format);
    if (impl_->normalization_supported) {
        impl_->normalized_pcm_scratch.resize(
            capture::normalized_pcm_size_for_frames(impl_->endpoint_buffer_frames));
    }

    const auto transition = impl_->lifecycle.prepare();
    if (transition != capture::CaptureTransitionError::none) {
        return impl_->fail(WasapiCaptureError::invalid_state, E_UNEXPECTED);
    }

    return true;
}

bool WasapiLoopbackCapture::start() noexcept {
    if (!impl_ || impl_->lifecycle.state() != capture::CaptureState::ready) {
        if (impl_) {
            return impl_->fail(WasapiCaptureError::invalid_state, E_UNEXPECTED);
        }
        return false;
    }

    if (!impl_->normalization_supported) {
        return impl_->fail(WasapiCaptureError::unsupported_mix_format, E_INVALIDARG);
    }

    impl_->reset_run_state();

    const HRESULT result = impl_->audio_client->Start();
    if (FAILED(result)) {
        return impl_->fail_capture_call(WasapiCaptureError::stream_start_failed, result);
    }
    impl_->audio_started = true;

    if (impl_->lifecycle.start() != capture::CaptureTransitionError::none) {
        return impl_->fail(WasapiCaptureError::invalid_state, E_UNEXPECTED);
    }

    impl_->clear_error();
    return true;
}

bool WasapiLoopbackCapture::drain_available() noexcept {
    if (!impl_ || impl_->lifecycle.state() != capture::CaptureState::running) {
        if (impl_) {
            return impl_->fail(WasapiCaptureError::invalid_state, E_UNEXPECTED);
        }
        return false;
    }

    UINT32 next_packet_frames = 0U;
    HRESULT result = impl_->capture_client->GetNextPacketSize(&next_packet_frames);
    if (FAILED(result)) {
        return impl_->fail_capture_call(
            WasapiCaptureError::next_packet_size_failed,
            result);
    }

    if (next_packet_frames == 0U) {
        ++impl_->empty_poll_count;
        impl_->clear_error();
        return true;
    }

    while (next_packet_frames != 0U) {
        BYTE* packet_data = nullptr;
        UINT32 packet_frames = 0U;
        DWORD raw_flags = 0U;
        UINT64 device_position = 0U;
        UINT64 qpc_position = 0U;

        result = impl_->capture_client->GetBuffer(
            &packet_data,
            &packet_frames,
            &raw_flags,
            &device_position,
            &qpc_position);
        if (result == AUDCLNT_S_BUFFER_EMPTY) {
            ++impl_->empty_poll_count;
            impl_->clear_error();
            return true;
        }
        if (FAILED(result)) {
            return impl_->fail_capture_call(
                WasapiCaptureError::capture_buffer_failed,
                result);
        }

        WasapiCaptureError packet_error = WasapiCaptureError::none;
        capture::CapturedPacketError conversion_error =
            capture::CapturedPacketError::none;
        capture::PcmFrameAssemblerError frame_assembler_error =
            capture::PcmFrameAssemblerError::none;
        std::uint32_t dropped_partial_frames = 0U;
        std::size_t converted_output_size = 0U;
        std::uint32_t normalized_flags = 0U;

        if (packet_frames != next_packet_frames) {
            packet_error = WasapiCaptureError::packet_size_mismatch;
        } else if (packet_frames > impl_->endpoint_buffer_frames) {
            packet_error = WasapiCaptureError::packet_frame_count_exceeds_buffer;
        } else if ((raw_flags & ~known_wasapi_buffer_flags) != 0U) {
            packet_error = WasapiCaptureError::unsupported_buffer_flags;
        } else {
            normalized_flags = normalize_buffer_flags(raw_flags);
            const bool silent =
                (normalized_flags & capture::captured_packet_flag_silence) != 0U;
            const std::size_t input_size = silent
                ? 0U
                : static_cast<std::size_t>(packet_frames) *
                    static_cast<std::size_t>(impl_->mix_format->nBlockAlign);

            capture::CapturedPacketView packet{};
            if (!silent && packet_data != nullptr) {
                packet.data = std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(packet_data),
                    input_size);
            }
            packet.frame_count = packet_frames;
            packet.flags = normalized_flags;
            packet.device_position_frames = device_position;
            packet.qpc_position_100ns = qpc_position;

            const auto conversion =
                capture::convert_float32_stereo_to_pcm_s16le(
                    packet,
                    impl_->normalized_pcm_scratch);
            if (!conversion.ok()) {
                packet_error = WasapiCaptureError::packet_conversion_failed;
                conversion_error = conversion.error;
            } else {
                converted_output_size = conversion.output_size;
            }
        }

        const HRESULT release_result =
            impl_->capture_client->ReleaseBuffer(packet_frames);
        if (FAILED(release_result)) {
            return impl_->fail_capture_call(
                WasapiCaptureError::capture_buffer_release_failed,
                release_result);
        }

        if (packet_error != WasapiCaptureError::none) {
            return impl_->fail(
                packet_error,
                E_INVALIDARG,
                conversion_error,
                frame_assembler_error);
        }

        const auto assembly = impl_->frame_assembler.accept_packet(
            std::span<const std::byte>(
                impl_->normalized_pcm_scratch.data(),
                converted_output_size),
            packet_frames,
            normalized_flags,
            device_position,
            qpc_position,
            &Impl::on_completed_frame,
            impl_.get());
        if (!assembly.ok()) {
            return impl_->fail(
                WasapiCaptureError::frame_assembly_failed,
                E_INVALIDARG,
                capture::CapturedPacketError::none,
                assembly.error);
        }
        dropped_partial_frames = assembly.dropped_partial_frames;

        impl_->dropped_partial_pcm_frames += dropped_partial_frames;
        ++impl_->packets_drained;
        impl_->frames_drained += packet_frames;
        impl_->pcm_bytes_produced += converted_output_size;
        impl_->last_device_position_frames = device_position;
        impl_->last_qpc_position_100ns = qpc_position;

        if ((normalized_flags & capture::captured_packet_flag_silence) != 0U) {
            ++impl_->silent_packets;
        }
        if ((normalized_flags & capture::captured_packet_flag_discontinuity) != 0U) {
            ++impl_->discontinuity_packets;
        }
        if ((normalized_flags & capture::captured_packet_flag_timestamp_error) != 0U) {
            ++impl_->timestamp_error_packets;
        }

        next_packet_frames = 0U;
        result = impl_->capture_client->GetNextPacketSize(&next_packet_frames);
        if (FAILED(result)) {
            return impl_->fail_capture_call(
                WasapiCaptureError::next_packet_size_failed,
                result);
        }
    }

    impl_->clear_error();
    return true;
}

bool WasapiLoopbackCapture::stop() noexcept {
    if (!impl_ || impl_->lifecycle.state() != capture::CaptureState::running) {
        if (impl_) {
            return impl_->fail(WasapiCaptureError::invalid_state, E_UNEXPECTED);
        }
        return false;
    }

    const HRESULT result = impl_->audio_client->Stop();
    if (FAILED(result)) {
        return impl_->fail_capture_call(WasapiCaptureError::stream_stop_failed, result);
    }
    impl_->audio_started = false;
    if (impl_->frame_assembler.reset()) {
        ++impl_->dropped_partial_pcm_frames;
    }

    if (impl_->lifecycle.stop() != capture::CaptureTransitionError::none) {
        return impl_->fail(WasapiCaptureError::invalid_state, E_UNEXPECTED);
    }

    impl_->clear_error();
    return true;
}

WasapiCaptureSnapshot WasapiLoopbackCapture::snapshot() const {
    WasapiCaptureSnapshot result{};
    if (!impl_) {
        result.state = capture::CaptureState::failed;
        result.error = WasapiCaptureError::invalid_state;
        result.native_result = static_cast<std::int32_t>(E_UNEXPECTED);
        return result;
    }

    result.state = impl_->lifecycle.state();
    result.error = impl_->error;
    result.conversion_error = impl_->conversion_error;
    result.frame_assembler_error = impl_->frame_assembler_error;
    result.native_result = static_cast<std::int32_t>(impl_->native_result);
    result.endpoint_name = impl_->endpoint_name;
    result.endpoint_id = impl_->endpoint_id;
    result.mix_format = impl_->inspected_mix_format;
    result.endpoint_buffer_frames = impl_->endpoint_buffer_frames;
    result.normalization_supported = impl_->normalization_supported;
    result.packets_drained = impl_->packets_drained;
    result.frames_drained = impl_->frames_drained;
    result.pcm_bytes_produced = impl_->pcm_bytes_produced;
    result.silent_packets = impl_->silent_packets;
    result.discontinuity_packets = impl_->discontinuity_packets;
    result.timestamp_error_packets = impl_->timestamp_error_packets;
    result.empty_poll_count = impl_->empty_poll_count;
    result.last_device_position_frames = impl_->last_device_position_frames;
    result.last_qpc_position_100ns = impl_->last_qpc_position_100ns;
    result.completed_pcm_frames = impl_->completed_pcm_frames;
    result.silent_pcm_frames = impl_->silent_pcm_frames;
    result.discontinuity_pcm_frames = impl_->discontinuity_pcm_frames;
    result.timestamp_error_pcm_frames = impl_->timestamp_error_pcm_frames;
    result.dropped_partial_pcm_frames = impl_->dropped_partial_pcm_frames;
    result.last_completed_pcm_sequence = impl_->last_completed_pcm_sequence;
    result.last_completed_device_position_frames =
        impl_->last_completed_device_position_frames;
    result.last_completed_qpc_position_100ns =
        impl_->last_completed_qpc_position_100ns;
    return result;
}

std::string_view to_string(WasapiSampleKind kind) noexcept {
    switch (kind) {
    case WasapiSampleKind::unknown:
        return "unknown";
    case WasapiSampleKind::integer_pcm:
        return "integer_pcm";
    case WasapiSampleKind::floating_point:
        return "floating_point";
    }

    return "unknown_sample_kind";
}

std::string_view to_string(WasapiCaptureError error) noexcept {
    switch (error) {
    case WasapiCaptureError::none:
        return "none";
    case WasapiCaptureError::invalid_state:
        return "invalid_state";
    case WasapiCaptureError::com_initialization_failed:
        return "com_initialization_failed";
    case WasapiCaptureError::device_enumerator_creation_failed:
        return "device_enumerator_creation_failed";
    case WasapiCaptureError::default_render_endpoint_failed:
        return "default_render_endpoint_failed";
    case WasapiCaptureError::endpoint_id_failed:
        return "endpoint_id_failed";
    case WasapiCaptureError::endpoint_property_store_failed:
        return "endpoint_property_store_failed";
    case WasapiCaptureError::endpoint_name_failed:
        return "endpoint_name_failed";
    case WasapiCaptureError::audio_client_activation_failed:
        return "audio_client_activation_failed";
    case WasapiCaptureError::mix_format_failed:
        return "mix_format_failed";
    case WasapiCaptureError::loopback_initialization_failed:
        return "loopback_initialization_failed";
    case WasapiCaptureError::capture_service_failed:
        return "capture_service_failed";
    case WasapiCaptureError::buffer_size_failed:
        return "buffer_size_failed";
    case WasapiCaptureError::unsupported_mix_format:
        return "unsupported_mix_format";
    case WasapiCaptureError::stream_start_failed:
        return "stream_start_failed";
    case WasapiCaptureError::stream_stop_failed:
        return "stream_stop_failed";
    case WasapiCaptureError::next_packet_size_failed:
        return "next_packet_size_failed";
    case WasapiCaptureError::capture_buffer_failed:
        return "capture_buffer_failed";
    case WasapiCaptureError::capture_buffer_release_failed:
        return "capture_buffer_release_failed";
    case WasapiCaptureError::packet_size_mismatch:
        return "packet_size_mismatch";
    case WasapiCaptureError::packet_frame_count_exceeds_buffer:
        return "packet_frame_count_exceeds_buffer";
    case WasapiCaptureError::unsupported_buffer_flags:
        return "unsupported_buffer_flags";
    case WasapiCaptureError::packet_conversion_failed:
        return "packet_conversion_failed";
    case WasapiCaptureError::frame_assembly_failed:
        return "frame_assembly_failed";
    case WasapiCaptureError::device_invalidated:
        return "device_invalidated";
    }

    return "unknown_wasapi_capture_error";
}

} // namespace wiretone::windows
