# Current Truth

Date: 2026-07-16

## Owner-validated foundation and Phase 1

- WireTone targets Windows 11 system audio to a Pixel 9a over local Wi-Fi.
- C++20 is the native source of truth; Kotlin remains a thin Android layer.
- Windows builds use Visual Studio 18 Community, MSVC, CMake, and the Windows SDK.
- Android builds use JDK 21, Gradle 9.5, SDK 36, NDK 28.2.13676358, and CMake 3.22.1.
- The Windows sender shell and Android JNI shell build successfully.
- Phase 1 packet, control-payload, fragmentation, reassembly, and receiver-session
  contracts are complete and tagged `phase-1.4-pass`.
- The five Phase 1 native test executables pass under MSVC.

## Implemented in Phase 2.1

- Platform-neutral capture lifecycle states: idle, ready, running, and failed.
- Strict prepare/start/stop/fail/reset transitions with native tests.
- Windows COM ownership contained inside the sender process.
- Default `eRender` / `eConsole` endpoint discovery.
- Endpoint friendly-name and stable endpoint-ID reporting.
- Shared-mode mix-format inspection, including extensible PCM and floating-point formats.
- WASAPI loopback initialization with the endpoint mix format.
- `IAudioCaptureClient` acquisition and endpoint-buffer frame reporting.
- Clean `IAudioClient::Start` and `Stop` probe behavior.
- Windows-specific source is excluded from non-Windows builds.

## Owner-validated Phase 2.1

- Windows MSVC clean build: PASS.
- `wiretone_core_tests`: PASS.
- `wiretone_protocol_tests`: PASS.
- `wiretone_control_payload_tests`: PASS.
- `wiretone_audio_frame_tests`: PASS.
- `wiretone_session_tests`: PASS.
- `wiretone_capture_lifecycle_tests`: PASS.
- Default render endpoint discovery: PASS.
- Endpoint friendly-name and ID reporting: PASS.
- Shared-mode mix-format inspection: PASS.
- Loopback `IAudioClient::Start`: PASS.
- Loopback `IAudioClient::Stop`: PASS.
- Live endpoint: `Virtual Speakers (Virtual Speakers for AudioRelay)`.
- Live mix format: 48,000 Hz, stereo, 32-bit floating-point, extensible.
- Live endpoint buffer: 1,056 frames.
- Windows SDK property-key include-order correction: PASS.

## Explicit Phase 2.1 limit

Phase 2.1 proves the Windows capture boundary only. It does not call
`IAudioCaptureClient::GetBuffer`, drain packets, normalize samples, resample, remix,
frame PCM, send UDP, encode Opus, or play audio on Android.

## Not implemented

- captured-packet draining and timestamps
- 48 kHz stereo PCM normalization
- WASAPI device-change recovery
- UDP socket transport
- Android audio output
- jitter buffering
- Opus integration
- discovery, pairing, encryption, foreground service, reconnection, or packaging
