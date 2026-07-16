# Current Truth

Date: 2026-07-16

## Confirmed on the owner's machines

- Project name: WireTone.
- Main target: Windows 11 to Pixel 9a over local Wi-Fi.
- Native language: C++20.
- Android integration language: thin Kotlin layer.
- Native build system: CMake 3.22.1 compatibility baseline.
- Windows environment check passes with Visual Studio 18 Community, MSVC,
  Windows SDK, CMake, and Ninja.
- Windows sender shell builds and runs.
- Native foundation tests pass under MSVC.
- Android debug APK and JNI C++ library build with JDK 21, Gradle 9.5, SDK 36,
  NDK 28.2.13676358, and CMake 3.22.1.
- The APK installs and launches on the Pixel 9a.
- The Pixel displays `WireTone native 0.1.0`, proving Kotlin -> JNI -> C++ works.

## Implemented in Phase 1.1

- Version-1 fixed packet header and strict parser.
- Network-byte-order serialization.
- Datagram and payload size limits.
- Stream, sequence, timestamp, frame, and fragment fields.
- Packet-type, flag, fragment, and payload validation.
- Exact-byte, round-trip, and malformed-packet native tests.
- Complete initial protocol contract in `docs/PROTOCOL.md`.

## Owner-validated Phase 1.1

- Windows MSVC build: PASS.
- `wiretone_core_tests`: PASS.
- `wiretone_protocol_tests`: PASS.
- Android NDK rebuild with the protocol library: PASS.
- Git commit `232a495` is published on `main` and `phase/01-protocol`.
- Annotated tag `phase-1.1-pass` is published.

## Implemented in Phase 1.2

- Typed `stream_start`, `stream_stop`, `heartbeat`, `receiver_report`, and
  protocol-error payload values.
- Exact network-order control-payload encoders and parsers.
- Locked semantic validation for the initial 48 kHz, stereo, 20 ms contract.
- PCM/Opus field-consistency checks.
- Reserved-field and well-formed UTF-8 rejection.
- Shared internal byte-order helpers for header and control payload code.
- Exact-byte, round-trip, and malformed-control-payload tests.

## Owner-validated Phase 1.2

- Windows MSVC build: PASS.
- `wiretone_core_tests`: PASS.
- `wiretone_protocol_tests`: PASS.
- `wiretone_control_payload_tests`: PASS.
- Android NDK rebuild with the control-payload library: PASS.
- Git commit `57995a9` is published on `phase/01-protocol`.
- Annotated tag `phase-1.2-pass` is published.

## Implemented in Phase 1.3

- Allocation-free fragmentation for logical audio frames up to 3,840 bytes.
- Canonical four-datagram splitting for the initial 20 ms stereo PCM frame.
- Sequence-number wrap preservation across fragments.
- Fixed eight-frame reassembly window with a deterministic 250 ms expiry.
- Out-of-order fragment acceptance.
- Duplicate, inconsistent-metadata, noncanonical-size, and over-limit rejection.
- Exact payload reconstruction and silence-frame handling.

## Owner-validated Phase 1.3

- Windows environment check with direct MSVC discovery: PASS.
- Windows CMake configure and MSVC build: PASS.
- `wiretone_core_tests`: PASS.
- `wiretone_protocol_tests`: PASS.
- `wiretone_control_payload_tests`: PASS.
- `wiretone_audio_frame_tests`: PASS.
- Android NDK rebuild with the audio-frame library: PASS.
- Git commit `e3fe9ff` is published on `phase/01-protocol`.
- The Windows bootstrap no longer imports the complete Visual Studio environment
  into the parent PowerShell process, avoiding cumulative `PATH` inflation.

## Implemented in Phase 1.4

- Shared C++ receiver session lifecycle with idle and streaming states.
- Valid `stream_start` requirement before audio or active-stream control data.
- Deterministic active-stream replacement, stop, reset, and timeout behavior.
- Fixed eight-frame completed-audio queue with oldest-frame drop on overflow.
- Discontinuity flushing before the newest frame is queued.
- Heartbeat sender-time tracking and 3,000 ms liveness expiry.
- Counters for malformed, rejected, wrong-stream, duplicate, inconsistent,
  expired, completed, dropped, discontinuity, and lifecycle events.
- Pure native session-state tests.

## Pending owner validation

- Apply the Phase 1.4 patch on Windows.
- Confirm all five native test executables pass under MSVC.
- Rebuild the Android APK so the session code compiles through NDK.

## Not implemented

- WASAPI capture
- Android audio output
- UDP socket transport
- jitter buffering
- Opus integration
- discovery, pairing, encryption, foreground service, reconnection, or packaging
