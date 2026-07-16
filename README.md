# WireTone

WireTone is a private, local-first audio relay for sending Windows 11 system audio to an
Android phone over the same local network.

## Product boundary

WireTone V1 targets one path only:

```text
Windows 11 system audio -> local Wi-Fi -> Pixel 9a speakers
```

No cloud account, telemetry, advertising, or remote relay is part of V1.

## Languages

- C++20: Windows sender, shared protocol/audio engine, Android NDK layer
- Kotlin: thin Android lifecycle and UI layer
- CMake: native builds on Windows and Android

## Current phase

**Phase 0 — Project foundation**

The repository currently contains:

- a buildable native core library
- a console sender shell
- native foundation tests
- a minimal Kotlin Android activity
- a JNI bridge that loads the shared C++ core
- project governance and decision documents

No audio capture, networking, codec, or playback code exists yet.

## Build native foundation on Windows

Open **Developer PowerShell for Visual Studio** and run:

```powershell
Set-Location C:\Path\To\wiretone
.\scripts\check-environment.ps1
.\scripts\bootstrap-windows.ps1
.\scripts\run-sender-shell.ps1
```

The tests must pass and the shell must print `WireTone sender shell 0.1.0`.

## Bootstrap and build the Android shell

First obtain and verify the official Gradle wrapper JAR:

```powershell
Set-Location C:\Path\To\wiretone
.\scripts\bootstrap-android.ps1
```

Then open `apps/android-receiver` in Android Studio. Install the SDK, NDK, CMake, and LLDB
components requested by the project, connect the Pixel 9a, and run the `app` configuration.

The screen should display `WireTone native 0.1.0`, returned through JNI from the C++ core.

The Android application ID `dev.wiretone.receiver` is provisional. It will not be treated as
permanent until the Play Console release identity is deliberately locked.

## Documents

- `docs/PROJECT_CHARTER.md`
- `docs/ARCHITECTURE.md`
- `docs/DECISIONS.md`
- `docs/CURRENT_TRUTH.md`
- `docs/NEXT_ACTIONS.md`
- `docs/TEST_RESULTS.md`
- `docs/PROTOCOL.md`

## License

Apache License 2.0. See `LICENSE`.
