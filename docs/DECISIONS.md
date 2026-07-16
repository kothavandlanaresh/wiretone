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

## D-013 — Bounded audio-frame fragmentation and reassembly

**Status:** Accepted
**Decision:** Limit the initial logical audio-frame payload to 3,840 bytes, split
it canonically into at most four 1,168-byte protocol payloads, and reassemble
within a fixed eight-frame window. Incomplete frames expire 250 ms after their
most recently accepted fragment.
**Reason:** The bound exactly covers the initial 20 ms / 48 kHz / stereo PCM
frame, avoids heap allocation in the protocol path, makes memory use predictable,
and prevents malformed or missing UDP fragments from growing receiver state.

## D-014 — Bounded receiver session state

**Status:** Accepted
**Decision:** Keep the pre-network receiver lifecycle in shared C++ with one
active stream, a fixed eight-frame completed-audio queue, and a 3,000 ms
liveness timeout. A new valid `stream_start` deterministically replaces the
active stream; stop, replacement, timeout, and explicit reset discard incomplete
and queued data. Queue overflow drops the oldest completed frame to preserve low
latency.
**Reason:** The lifecycle and memory policy must be deterministic before sockets
and platform audio APIs can introduce timing, threading, and error complexity.

## D-015 — Initial WASAPI loopback boundary

**Status:** Accepted
**Decision:** Phase 2.1 opens the default `eRender` / `eConsole` endpoint through
`IMMDeviceEnumerator`, uses the endpoint shared-mode mix format, initializes
`IAudioClient` with `AUDCLNT_STREAMFLAGS_LOOPBACK`, acquires
`IAudioCaptureClient`, and verifies start/stop without draining samples.
**Reason:** Endpoint ownership, COM lifetime, format discovery, loopback eligibility,
and stream lifecycle should be proven before packet draining, sample conversion,
threading, or transport are introduced. The platform-neutral lifecycle remains
separately testable without Windows audio hardware.

## D-016 — Phase 2.2 packet-drain and normalization boundary

**Status:** Accepted
**Decision:** Drain every currently available shared-mode WASAPI capture packet on
the same thread that calls `GetBuffer` and `ReleaseBuffer`. Preserve frame counts,
device/QPC positions, and silence, discontinuity, and timestamp-error flags. Convert
only 48 kHz stereo 32-bit floating-point packets into signed 16-bit little-endian PCM
using a preallocated endpoint-sized scratch buffer.
**Reason:** This proves the complete acquisition and normalization boundary without
introducing transport, a worker thread, resampling, channel remixing, or long-lived
audio queues. Strict format rejection prevents silent reinterpretation of unsupported
endpoint data.


## D-017 — Bounded 20 ms PCM frame assembly

**Status:** Accepted
**Decision:** Assemble normalized 48 kHz stereo signed-16-bit PCM into exact 960-frame
/ 3,840-byte logical frames with one fixed internal buffer and a synchronous non-owning
completion callback. A capture discontinuity discards any partial frame and marks the
first subsequent completed frame. Silence is marked only when every contributing
region is silent, and timestamp errors propagate to each affected completed frame.
**Reason:** The protocol contract requires exact 20 ms logical frames, while WASAPI
packet lengths are device- and schedule-dependent. A bounded assembler isolates that
variation without introducing queues, retained audio, transport, or heap activity in
the steady-state framing path.

## D-018 — Bounded default-device recovery

**Status:** Accepted
**Decision:** Register one `IMMNotificationClient` with the Windows endpoint enumerator
and coalesce default `eRender` / `eConsole` changes with WASAPI invalidation results into
one platform-neutral recovery controller. Recovery performs at most three immediate
endpoint reacquisition attempts, discards partial PCM assembly without resetting the
completed-frame sequence, rejects unsupported replacement formats, and marks the first
post-recovery logical frame discontinuous.
**Reason:** Windows default endpoints can change or become invalid while the sender is
running. Recovery must not retain stale COM interfaces, hide a capture gap, grow
unbounded retry state, or make platform callbacks the source of truth. A deterministic
controller keeps event counts and restart policy testable while the Windows layer owns
COM notification and endpoint reconstruction.

## D-019 — Initial Android AAudio output boundary

**Status:** Accepted
**Decision:** Own Android playback in NDK C++ through one AAudio callback output stream.
Request 48 kHz stereo signed 16-bit PCM, shared mode, and low-latency performance, then
query and report the negotiated stream properties. Phase 3.1 renders silence only and
keeps the callback limited to zero-fill plus atomic counters.
**Reason:** Native stream negotiation, callback scheduling, lifecycle, disconnection,
and underrun visibility must be proven on the Pixel before network timing or PCM queue
behavior is introduced. The real-time callback cannot allocate, lock, log, call JNI, or
perform blocking work.

## D-016 — Phase 3.2 playback queue is fixed-capacity SPSC with drop-newest overflow

The playback queue has one producer and the AAudio callback as its one consumer. It stores
thirty-two complete 960-frame stereo PCM16 logical frames in fixed memory. The producer drops
the newest frame when full instead of overwriting audio already committed to playback. The
callback may consume any positive frame count, crosses logical-frame boundaries without copying
or allocating outside fixed storage, and zero-fills unmet demand. Stop waits for AAudio to leave
the running state before queued and partially consumed data are discarded.

Rationale: this gives deterministic real-time behavior and visible loss/starvation counters while
keeping transport timing, jitter policy, and network ownership out of Phase 3.
