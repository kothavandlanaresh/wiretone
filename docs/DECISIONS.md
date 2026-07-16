# Architecture Decision Log

## D-001 — Product name

**Status:** Accepted
**Decision:** Use `WireTone` as the project and repository name.
**Reason:** `OpenRelay` and `LocalAudioLink` are already in active use. The final Google Play
application ID remains provisional until release preparation.

## D-002 — Native language

**Status:** Accepted
**Decision:** Use C++20 for the Windows sender, shared core, and Android NDK layer.
**Reason:** The owner knows C++, and the project requires direct, timing-sensitive native audio
and networking work.

## D-003 — Android language

**Status:** Accepted
**Decision:** Use a thin Kotlin layer only where Android APIs require it.
**Reason:** Kotlin is the modern Android integration language, but protocol, buffering, codec,
and audio-engine complexity should remain in maintainable C++.

## D-004 — Initial transport

**Status:** Accepted
**Decision:** Start with versioned UDP on the local network, not WebRTC.
**Reason:** LAN-only V1 does not require ICE, TURN, signalling, or internet NAT traversal.

## D-005 — Initial codec

**Status:** Accepted
**Decision:** Establish end-to-end PCM first, then introduce Opus.
**Reason:** PCM isolates capture, transport, and playback failures before codec complexity is
added.

## D-006 — License

**Status:** Accepted for foundation; review before public V1
**Decision:** Apache License 2.0.
**Reason:** Clear permissive terms and an explicit patent grant make it practical for a Play
Store app and native desktop distribution.

## D-007 — Android foundation toolchain

**Status:** Accepted
**Decision:** AGP 9.3.0 with its built-in Kotlin support, Gradle 9.5.0, JDK 17,
NDK 28.2.13676358, compile SDK 36, and target SDK 36.
**Reason:** This avoids a redundant Kotlin Android plugin and follows the current stable Android
Gradle Plugin compatibility baseline selected during Phase 0.

## D-008 — Gradle wrapper bootstrap

**Status:** Accepted for Phase 0
**Decision:** Keep the wrapper scripts and properties in the repository. Download the official
Gradle 9.5.0 wrapper JAR through `scripts/bootstrap-android.ps1`, then verify its Git blob SHA
before execution. Commit the verified JAR with the rest of Phase 0 on the owner's machine.
**Reason:** The validation environment cannot package the binary wrapper JAR directly, but the
repository must still provide a deterministic, verified setup path.

## D-009 — CMake compatibility baseline

**Status:** Accepted
**Decision:** Use CMake 3.22.1 as the shared Windows/Android project baseline during
Phase 0. Pin the Android native build to 3.22.1 and avoid newer-only CMake
features unless they are required.
**Reason:** Android Gradle Plugin installed and selected CMake 3.22.1. The
foundation did not need the CMake 3.23 `FILE_SET HEADERS` feature, so requiring
3.25 added installation complexity without product value.

## D-010 — Version-1 packet envelope

**Status:** Accepted
**Decision:** Use a fixed 32-byte big-endian header and a maximum WireTone UDP
 datagram size of 1,200 bytes. Every packet carries a non-zero stream ID, per-
 datagram sequence number, sample timestamp, and explicit frame-fragment fields.
**Reason:** The fixed envelope is inexpensive to parse, deterministic across C++
 platforms, avoids dependence on compiler struct layout, and leaves enough room
 to transport temporary PCM frames as fragments without IP fragmentation.

## D-011 — Initial audio timing contract

**Status:** Accepted
**Decision:** Use 48 kHz stereo and 20 ms logical frames. Begin end-to-end
 transport with signed 16-bit little-endian PCM, then replace the audio payload
 with Opus while retaining the same packet timing fields.
**Reason:** Twenty-millisecond frames are a practical starting balance for video
 latency and network efficiency. PCM first isolates capture, transport, and
 playback defects before codec behavior is introduced.

## D-012 — Typed control-payload boundary

**Status:** Accepted
**Decision:** Keep version-1 control payload serialization and semantic validation
inside the shared C++ protocol library. Android and Windows wrappers consume typed
values rather than hand-building byte arrays.
**Reason:** One implementation prevents platform drift, keeps malformed input away
from UI/audio state, and makes exact wire compatibility testable before networking.
