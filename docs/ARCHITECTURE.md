# Architecture

## Planned V1 data path

```text
Windows WASAPI loopback
    -> PCM normalization
    -> Opus encoder
    -> versioned packet protocol
    -> encrypted UDP transport
    -> Android packet receiver
    -> jitter buffer
    -> Opus decoder
    -> native Android audio output
    -> Pixel speakers
```

## Layer boundaries

### `native/core`

Platform-neutral C++20 code. It must not include Windows or Android headers.

### `native/protocol`

Owns the versioned wire contract, typed control payloads, audio fragmentation and
reassembly, and the bounded receiver session lifecycle. Platform wrappers pass
datagrams and monotonic arrival times into this layer; they do not duplicate its
state rules.

### `native/capture`

Owns platform-neutral capture lifecycle and recovery states, captured-packet metadata,
exact 48 kHz stereo float-to-signed-16-bit PCM normalization, and bounded assembly
into 960-frame / 20 ms logical PCM frames. Recovery attempts, triggers, success/failure
counters, and post-recovery discontinuity state are testable without Windows headers
or audio hardware.

### Windows sender

Owns COM, endpoint discovery, WASAPI loopback initialization, packet acquisition and
release, Windows device lifecycle, transport transmission, and the desktop control
surface. Phase 2.4 registers an `IMMNotificationClient` for the default `eRender` /
`eConsole` endpoint, coalesces endpoint-change and invalidation signals, discards partial
PCM state while preserving completed-frame sequence continuity, and performs at most
three endpoint reacquisition attempts on the owning thread. The first completed frame
after a successful restart is marked discontinuous.

### `native/playback`

Owns platform-neutral playback lifecycle states, transition rules, and the fixed-capacity
SPSC PCM playback queue. The queue accepts exact 960-frame stereo PCM16 logical frames,
preserves sequence/discontinuity metadata, uses a drop-newest overflow policy, and zero-fills
callback demand when empty. This layer contains no Android headers and remains testable on
Windows and Linux.

### Android native layer

Owns AAudio stream construction, negotiated-property inspection, callback rendering,
error/disconnect signaling, receive-side packet processing, jitter management, decoding,
and performance-sensitive audio state. Phase 3.2 connects the native SPSC queue to the
AAudio callback and provides a bounded local test producer. The callback performs no allocation,
locking, logging, JNI, sleeping, or blocking work.

### Android Kotlin layer

Owns Android lifecycle, foreground service, permissions, notification, UI, and a deliberately
small JNI surface.

## Source-of-truth order

1. Protocol specification
2. Native engine state
3. Measured test results
4. Platform UI

A UI state may not claim connection or quality that the native engine has not reported.
