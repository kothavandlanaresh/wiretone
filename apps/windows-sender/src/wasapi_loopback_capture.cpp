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
#include <utility>

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

} // namespace

struct WasapiLoopbackCapture::Impl {
    capture::CaptureLifecycle lifecycle{};
    WasapiCaptureError error{WasapiCaptureError::none};
    HRESULT native_result{S_OK};
    bool com_initialized{false};
    ComPtr<IMMDeviceEnumerator> device_enumerator{};
    ComPtr<IMMDevice> endpoint{};
    ComPtr<IAudioClient> audio_client{};
    ComPtr<IAudioCaptureClient> capture_client{};
    WAVEFORMATEX* mix_format{nullptr};
    std::string endpoint_name{};
    std::string endpoint_id{};
    WasapiMixFormat inspected_mix_format{};
    UINT32 endpoint_buffer_frames{0};

    ~Impl() {
        if (lifecycle.state() == capture::CaptureState::running && audio_client) {
            (void)audio_client->Stop();
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

    [[nodiscard]] bool fail(WasapiCaptureError new_error, HRESULT result) noexcept {
        error = new_error;
        native_result = result;
        lifecycle.fail();
        return false;
    }

    void clear_error() noexcept {
        error = WasapiCaptureError::none;
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

    const HRESULT result = impl_->audio_client->Start();
    if (FAILED(result)) {
        return impl_->fail(WasapiCaptureError::stream_start_failed, result);
    }

    if (impl_->lifecycle.start() != capture::CaptureTransitionError::none) {
        return impl_->fail(WasapiCaptureError::invalid_state, E_UNEXPECTED);
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
        return impl_->fail(WasapiCaptureError::stream_stop_failed, result);
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
    result.native_result = static_cast<std::int32_t>(impl_->native_result);
    result.endpoint_name = impl_->endpoint_name;
    result.endpoint_id = impl_->endpoint_id;
    result.mix_format = impl_->inspected_mix_format;
    result.endpoint_buffer_frames = impl_->endpoint_buffer_frames;
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
    case WasapiCaptureError::stream_start_failed:
        return "stream_start_failed";
    case WasapiCaptureError::stream_stop_failed:
        return "stream_stop_failed";
    }

    return "unknown_wasapi_capture_error";
}

} // namespace wiretone::windows
