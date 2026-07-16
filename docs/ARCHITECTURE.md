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

### Windows sender

Owns WASAPI capture, Windows device lifecycle, transport transmission, and the desktop
control surface.

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
