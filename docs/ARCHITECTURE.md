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

Owns platform-neutral capture lifecycle states, captured-packet metadata, exact
48 kHz stereo float-to-signed-16-bit PCM normalization, and bounded assembly into
960-frame / 20 ms logical PCM frames. It contains no Windows headers and is tested
independently from audio hardware.

### Windows sender

Owns COM, endpoint discovery, WASAPI loopback initialization, packet acquisition and
release, Windows device lifecycle, transport transmission, and the desktop control
surface. Phase 2.3 drains packets on the owning thread, normalizes supported 48 kHz
stereo float input into bounded scratch memory, and synchronously feeds exact 20 ms
PCM frames into a non-owning in-memory completion callback.

### Android native layer

Owns receive-side packet processing, jitter management, decoding, and performance-sensitive
audio state.

### Android Kotlin layer

Owns Android lifecycle, foreground service, permissions, notification, UI, and a deliberately
small JNI surface.

## Source-of-truth order

1. Protocol specification
2. Native engine state
3. Measured test results
4. Platform UI

A UI state may not claim connection or quality that the native engine has not reported.
