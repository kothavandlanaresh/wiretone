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

## Pending owner validation

- Apply the Phase 1.1 patch on Windows.
- Re-run Windows native tests and confirm both test executables pass.
- Rebuild the Android APK so the protocol library also compiles through the NDK.

## Not implemented

- WASAPI capture
- Android audio output
- UDP socket transport
- jitter buffering
- Opus integration
- discovery, pairing, encryption, foreground service, reconnection, or packaging
