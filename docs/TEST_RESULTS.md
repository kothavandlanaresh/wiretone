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

- MSVC build and all native tests: PENDING
- Android NDK rebuild with control-payload library: PENDING
