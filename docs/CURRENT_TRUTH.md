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

## Pending owner validation

- Apply the Phase 1.2 patch on Windows.
- Confirm all three native test executables pass under MSVC.
- Rebuild the Android APK so the new control-payload code compiles through NDK.

## Not implemented

- WASAPI capture
- Android audio output
- UDP socket transport
- jitter buffering
- Opus integration
- discovery, pairing, encryption, foreground service, reconnection, or packaging
