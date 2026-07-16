# Test Results

## Phase 0 — Owner machine validation

Date: 2026-07-16

### Windows 11

- Environment checker: PASS
- Visual Studio 18 Community / MSVC activation: PASS
- CMake configure and generation: PASS
- Windows sender shell build: PASS
- Native foundation test: PASS
- Sender shell runtime output: PASS

Observed sender output:

```text
WireTone sender shell 0.1.0
Phase 0 foundation is active. WASAPI capture is not implemented yet.
```

### Pixel 9a / Android

- JDK 21 selection: PASS
- Gradle 9.5 startup: PASS
- SDK 36: PASS
- NDK 28.2.13676358: PASS
- CMake 3.22.1: PASS
- Android APK and JNI build: PASS
- ADB device authorization: PASS
- APK installation: PASS
- Main activity launch: PASS
- Kotlin -> JNI -> shared C++ version call: PASS

Observed Pixel text:

```text
WireTone receiver shell

Native bridge: WireTone native 0.1.0

Phase 0 only — audio playback is not implemented yet.
```

## Phase 1.1 — Validation environment

Date: 2026-07-16

- CMake configure: PASS
- C++ build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- Exact 32-byte wire-layout test: PASS
- Header round-trip test: PASS
- Malformed packet rejection tests: PASS

### Owner-machine Phase 1.1 validation

- MSVC build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- Android NDK rebuild with protocol library: PASS
- GitHub publication of `main`, `phase/01-protocol`, and `phase-1.1-pass`: PASS

## Phase 1.2 — Validation environment

Date: 2026-07-16

- Typed control-payload implementation: COMPLETE
- CMake configure: PASS
- C++ build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- Exact control-payload byte-layout tests: PASS
- Control-payload round-trip tests: PASS
- Semantic and malformed-payload rejection tests: PASS

### Owner-machine Phase 1.2 validation

- MSVC build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- Android NDK rebuild with control-payload library: PASS
- Commit `57995a9` pushed to `phase/01-protocol`: PASS
- Annotated tag `phase-1.2-pass` pushed: PASS

## Phase 1.3 — Validation environment

Date: 2026-07-16

- Audio-frame fragmentation implementation: COMPLETE
- Bounded reassembly implementation: COMPLETE
- CMake configure: PASS
- C++ build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- Exact 3,840-byte PCM four-fragment test: PASS
- Out-of-order reconstruction test: PASS
- Duplicate and inconsistent-metadata rejection tests: PASS
- Fixed-window and deterministic-expiry tests: PASS
- Sequence-number wrap test: PASS

### Owner-machine Phase 1.3 validation

- Android NDK rebuild with audio-frame library: PASS
- Windows environment checker after bootstrap correction: PASS
- Visual Studio 18 Community / MSVC discovery: PASS
- CMake x64 configure and generation: PASS
- MSVC native build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- Commit `e3fe9ff` pushed to `phase/01-protocol`: PASS
- Premature `phase-1.3-pass` tag removed before final Windows validation: PASS

### Windows bootstrap correction

- Reproduced failure: `cmd.exe` reported `The input line is too long`: PASS
- Root cause: importing the complete Visual Studio environment into the parent
  PowerShell process caused cumulative environment growth.
- Direct MSVC compiler discovery: PASS
- CMake Visual Studio generator without parent-shell environment import: PASS
- Clean x64 CMake reconfiguration after removing the stale cache: PASS

## Phase 1.4 — Validation environment

Date: 2026-07-16

- Receiver session-state implementation: COMPLETE
- CMake configure: PASS
- C++ build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- Stream-start-before-audio enforcement: PASS
- Wrong-stream and post-stop rejection: PASS
- Stream replacement and heartbeat timeout behavior: PASS
- Bounded queue and oldest-frame overflow policy: PASS
- Discontinuity queue flushing: PASS
- Malformed, duplicate, inconsistent, and expired counters: PASS
- Non-monotonic clock rejection: PASS
- AddressSanitizer: PASS
- UndefinedBehaviorSanitizer: PASS

### Owner-machine Phase 1.4 validation

- Android NDK rebuild with session library: PASS
- Initial incremental Windows link exposed a stale pre-Phase-1.4 object: OBSERVED
- Premature `phase-1.4-pass` tag removed before final Windows validation: PASS
- Clean CMake x64 configuration: PASS
- Clean MSVC build of all protocol sources: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- Commit `2c6d7a7` pushed to `phase/01-protocol`: PASS

### Windows clean-build hardening

- Root cause: ZIP extraction preserved source timestamps older than existing object files.
- `AudioFrameReassembler::reset()` declaration and definition were both present in source.
- Removing `out/build/windows` forced the correct rebuild and resolved the link error.
- `scripts/bootstrap-windows.ps1` now uses CMake `--clean-first`: PASS

## Phase 2.1 — Validation environment

Date: 2026-07-16

- Platform-neutral capture lifecycle implementation: COMPLETE
- Default-render-endpoint WASAPI owner implementation: COMPLETE
- Shared-mode loopback initialization path: COMPLETE
- Mix-format and endpoint-buffer reporting: COMPLETE
- Clean start/stop probe path: COMPLETE
- CMake configure: PASS
- C++ build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- `wiretone_capture_lifecycle_tests`: PASS
- GCC AddressSanitizer: PASS
- GCC UndefinedBehaviorSanitizer: PASS

### Owner-machine Phase 2.1 validation

- Windows MSVC clean build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- `wiretone_capture_lifecycle_tests`: PASS
- Windows SDK property-key include-order correction: PASS
- Default render endpoint discovery: PASS
- Endpoint friendly-name and endpoint-ID reporting: PASS
- Shared-mode mix-format inspection: PASS
- Endpoint-buffer frame reporting: PASS
- Loopback start: PASS
- Loopback stop: PASS
- Final sender-shell Phase 2.1 result: PASS

Observed owner-machine endpoint:

```text
Default render endpoint: Virtual Speakers (Virtual Speakers for AudioRelay)
Mix format: 48000 Hz, 2 channels, 32 container bits, 32 valid bits, floating_point, extensible
Endpoint buffer: 1056 frames
```

### Validation-environment note

- The non-Windows validation environment could not compile or execute WASAPI.
- GCC AddressSanitizer and UndefinedBehaviorSanitizer passed all six tests.
- The container's Swift-packaged Clang sanitizer runtime crashed before `main()` for
  every executable, including unchanged Phase 1 tests; it was not used as evidence.

## Phase 2.2 — Validation environment

Date: 2026-07-16

- Platform-neutral captured-packet view: COMPLETE
- Float32 stereo to PCM S16LE conversion: COMPLETE
- Exact clipping and channel-order tests: PASS
- Silent-packet zero synthesis: PASS
- Frame-count, flags, device-position, and QPC preservation tests: PASS
- Unknown-flag, invalid-size, missing-data, undersized-output, and non-finite tests: PASS
- CMake configure: PASS
- C++ build with strict warnings: PASS
- Existing Phase 1 and Phase 2.1 tests: PASS
- `wiretone_captured_packet_tests`: PASS
- Combined native tests: PASS — 7/7
- GCC AddressSanitizer: PASS — 7/7
- GCC UndefinedBehaviorSanitizer: PASS — 7/7

### Owner-machine Phase 2.2 validation

- Windows MSVC clean build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- `wiretone_capture_lifecycle_tests`: PASS
- `wiretone_captured_packet_tests`: PASS
- Live WASAPI packet drain with audible content: PASS
- Non-zero packet counter: PASS — 1 packet
- Non-zero frame counter: PASS — 480 frames
- Non-zero PCM-byte counter: PASS — 1,920 bytes
- Silent packets: 0
- Discontinuity packets: 1
- Timestamp-error packets: 0
- Empty polls: 1
- Last device position: 497,177,760 frames
- Last QPC position: 104,101,493,330 in 100 ns units
- Final sender-shell Phase 2.2 result: PASS

Observed owner-machine output:

```text
Default render endpoint: Virtual Speakers (Virtual Speakers for AudioRelay)
Mix format: 48000 Hz, 2 channels, 32 container bits, 32 valid bits, floating_point, extensible
Endpoint buffer: 1056 frames
Packets drained: 1
Frames drained: 480
PCM bytes produced: 1920
Silent packets: 0
Discontinuity packets: 1
Timestamp-error packets: 0
Empty polls: 1
Last device position: 497177760 frames
Last QPC position: 104101493330 x 100 ns
Phase 2.2 WASAPI packet drain and PCM normalization: PASS
```


## Phase 2.3 — Validation environment

Date: 2026-07-16

- Exact 960-frame PCM assembler implementation: COMPLETE
- Fixed allocation-free frame storage: COMPLETE
- Split-packet assembly tests: PASS
- Exact-packet assembly tests: PASS
- Combined multi-frame packet tests: PASS
- Discontinuity partial-discard tests: PASS
- Silence and timestamp-error propagation tests: PASS
- Reset and malformed-input tests: PASS
- CMake configure: PASS
- C++ build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- `wiretone_capture_lifecycle_tests`: PASS
- `wiretone_captured_packet_tests`: PASS
- `wiretone_pcm_frame_assembler_tests`: PASS
- GCC AddressSanitizer: PASS
- GCC UndefinedBehaviorSanitizer: PASS

### Owner-machine Phase 2.3 validation

- Windows MSVC clean build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- `wiretone_capture_lifecycle_tests`: PASS
- `wiretone_captured_packet_tests`: PASS
- `wiretone_pcm_frame_assembler_tests`: PASS
- Live completed 960-frame PCM frame: PASS
- Packets drained: 3
- Frames drained: 1,440
- PCM bytes produced: 5,760
- Completed 20 ms PCM frames: 1
- Silent PCM frames: 0
- Discontinuity PCM frames: 1
- Timestamp-error PCM frames: 0
- Dropped partial PCM frames: 1
- Last completed PCM sequence: 1
- Last completed device position: 556,498,080 frames
- Last completed QPC position: 116,459,893,447 in 100 ns units
- Final sender-shell Phase 2.3 result: PASS

Observed owner-machine output:

```text
Default render endpoint: Virtual Speakers (Virtual Speakers for AudioRelay)
Mix format: 48000 Hz, 2 channels, 32 container bits, 32 valid bits, floating_point, extensible
Endpoint buffer: 1056 frames
Packets drained: 3
Frames drained: 1440
PCM bytes produced: 5760
Silent packets: 0
Discontinuity packets: 1
Timestamp-error packets: 0
Empty polls: 1
Last device position: 556499040 frames
Last QPC position: 116460103103 x 100 ns
Completed 20 ms PCM frames: 1
Silent PCM frames: 0
Discontinuity PCM frames: 1
Timestamp-error PCM frames: 0
Dropped partial PCM frames: 1
Last completed PCM sequence: 1
Last completed device position: 556498080 frames
Last completed QPC position: 116459893447 x 100 ns
Phase 2.3 exact 20 ms PCM frame assembly: PASS
```

## Phase 2.4 — Validation environment

Date: 2026-07-16

- Platform-neutral recovery controller: COMPLETE
- Three-attempt restart bound: COMPLETE
- Repeated-trigger coalescing tests: PASS
- Retryable and non-retryable failure tests: PASS
- One-shot discontinuity tests: PASS
- Sequence-preserving partial-discard tests: PASS
- CMake configure: PASS
- C++ build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- `wiretone_capture_lifecycle_tests`: PASS
- `wiretone_captured_packet_tests`: PASS
- `wiretone_pcm_frame_assembler_tests`: PASS
- `wiretone_capture_recovery_tests`: PASS
- Combined native tests: PASS — 9/9
- GCC AddressSanitizer: PASS — 9/9
- GCC UndefinedBehaviorSanitizer: PASS — 9/9

### Owner-machine Phase 2.4 validation

- Windows MSVC clean build: PASS
- `wiretone_core_tests`: PASS
- `wiretone_protocol_tests`: PASS
- `wiretone_control_payload_tests`: PASS
- `wiretone_audio_frame_tests`: PASS
- `wiretone_session_tests`: PASS
- `wiretone_capture_lifecycle_tests`: PASS
- `wiretone_captured_packet_tests`: PASS
- `wiretone_pcm_frame_assembler_tests`: PASS
- `wiretone_capture_recovery_tests`: PASS
- Endpoint-notification callback compilation: PASS
- Simulated invalidation endpoint reacquisition: PASS
- Successful post-recovery PCM frame: PASS
- Recovery discontinuity propagation: PASS
- Monotonic completed-frame sequence after recovery: PASS
- Packets drained: 4
- Frames drained: 1,920
- PCM bytes produced: 7,680
- Silent packets: 0
- Discontinuity packets: 2
- Timestamp-error packets: 0
- Empty polls: 2
- Completed 20 ms PCM frames: 2
- Discontinuity PCM frames: 2
- Dropped partial PCM frames: 0
- Last completed PCM sequence: 2
- Recovery attempts: 1
- Successful recoveries: 1
- Failed recoveries: 0
- Device invalidations: 1
- Default-device changes: 0
- Recovery discontinuity PCM frames: 1
- Final sender-shell Phase 2.4 result: PASS

Observed owner-machine output:

```text
Default render endpoint: Virtual Speakers (Virtual Speakers for AudioRelay)
Mix format: 48000 Hz, 2 channels, 32 container bits, 32 valid bits, floating_point, extensible
Endpoint buffer: 1056 frames
Packets drained: 4
Frames drained: 1920
PCM bytes produced: 7680
Silent packets: 0
Discontinuity packets: 2
Timestamp-error packets: 0
Empty polls: 2
Completed 20 ms PCM frames: 2
Discontinuity PCM frames: 2
Dropped partial PCM frames: 0
Last completed PCM sequence: 2
Recovery attempts: 1
Successful recoveries: 1
Failed recoveries: 0
Device invalidations: 1
Default-device changes: 0
Recovery discontinuity PCM frames: 1
Phase 2.4 default-device invalidation and capture recovery: PASS
```

### Documented validation limitation

- A real interactive Windows default-output-device switch was not performed.
- The deterministic invalidation exercised the same endpoint teardown,
  default-endpoint reacquisition, mix-format validation, loopback initialization,
  restart, sequence-preserving partial discard, and post-recovery discontinuity path.
