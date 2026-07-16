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

## Implemented in Phase 2.2

- Platform-neutral captured-packet view with frame count, flags, device position, and
  QPC position.
- Exact 48 kHz stereo float32 to signed 16-bit little-endian PCM conversion.
- Deterministic clipping, silence synthesis, channel-order preservation, and malformed
  packet rejection.
- WASAPI `GetNextPacketSize` / `GetBuffer` / `ReleaseBuffer` drain loop.
- Packet, frame, PCM-byte, silence, discontinuity, timestamp-error, and empty-poll
  counters.
- Device-invalidated and capture-call error reporting.
- Preallocated endpoint-sized PCM scratch memory; normalized data is not retained or sent.
- Seventh native test executable for captured-packet conversion.

## Owner-validated Phase 2.2

- Windows MSVC clean build: PASS.
- `wiretone_core_tests`: PASS.
- `wiretone_protocol_tests`: PASS.
- `wiretone_control_payload_tests`: PASS.
- `wiretone_audio_frame_tests`: PASS.
- `wiretone_session_tests`: PASS.
- `wiretone_capture_lifecycle_tests`: PASS.
- `wiretone_captured_packet_tests`: PASS.
- Live loopback packet drain while audible content used the default endpoint: PASS.
- Captured packets: 1.
- Captured frames: 480.
- Normalized PCM bytes: 1,920.
- Silent packets: 0.
- Discontinuity packets: 1.
- Timestamp-error packets: 0.
- Empty polls: 1.
- Last device position: 497,177,760 frames.
- Last QPC position: 104,101,493,330 in 100 ns units.
- Normalized audio remained in bounded scratch memory and was not retained or sent.

## Explicit Phase 2.2 limit

Phase 2.2 converts each WASAPI packet independently into bounded scratch memory. It
does not assemble a continuous stream into 960-frame / 20 ms logical PCM frames and
does not retain audio after each packet is counted.

## Implemented in Phase 2.3

- Platform-neutral exact 960-frame / 20 ms PCM frame assembler.
- Fixed 3,840-byte internal storage with no steady-state allocation.
- Synchronous non-owning completed-frame callback.
- Split, exact, and combined packet handling.
- Monotonic completed-frame sequence numbers.
- Device-position and QPC timestamp propagation from the first frame sample.
- Discontinuity-driven partial discard and first-completed-frame marking.
- Whole-frame silence and timestamp-error propagation.
- Deterministic reset of partial state and sequence numbering.
- Windows sender counters for completed and flagged logical PCM frames.
- Pure native frame-assembler tests.

## Owner-validated Phase 2.3

- Windows MSVC clean build: PASS.
- `wiretone_core_tests`: PASS.
- `wiretone_protocol_tests`: PASS.
- `wiretone_control_payload_tests`: PASS.
- `wiretone_audio_frame_tests`: PASS.
- `wiretone_session_tests`: PASS.
- `wiretone_capture_lifecycle_tests`: PASS.
- `wiretone_captured_packet_tests`: PASS.
- `wiretone_pcm_frame_assembler_tests`: PASS.
- Live exact 960-frame / 20 ms PCM assembly: PASS.
- Captured packets: 3.
- Captured frames: 1,440.
- Normalized PCM bytes: 5,760.
- Completed PCM frames: 1.
- Silent PCM frames: 0.
- Discontinuity PCM frames: 1.
- Timestamp-error PCM frames: 0.
- Dropped partial PCM frames: 1.
- Last completed PCM sequence: 1.
- Last completed device position: 556,498,080 frames.
- Last completed QPC position: 116,459,893,447 in 100 ns units.
- Completed PCM remained local in memory and was not retained or sent.

## Not implemented

- retained PCM queues, files, or transport handoff
- WASAPI default-device invalidation and restart recovery
- UDP socket transport
- Android audio output
- jitter buffering
- Opus integration
- discovery, pairing, encryption, foreground service, reconnection, or packaging
