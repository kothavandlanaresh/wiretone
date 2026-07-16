# Next Actions

## Phase 2.2 — Drain and normalize captured packets

Stay on branch `phase/02-windows-capture`.

Implement the next bounded Windows capture increment:

- drain `IAudioCaptureClient` packets without adding network transport
- preserve packet frame counts and WASAPI capture flags
- surface silent, discontinuous, timestamp-error, and device-invalidated events
- define a platform-neutral captured-packet view for deterministic tests
- convert supported 48 kHz stereo floating-point input into signed 16-bit
  interleaved PCM
- validate clipping, silence, channel order, frame counts, and discontinuity behavior
- keep endpoint discovery and COM/WASAPI ownership inside the Windows sender layer

## Phase 2.2 validation gate

- all existing six native tests remain green
- new packet-drain and sample-conversion tests pass under MSVC
- a live sender probe drains real loopback packets and reports frame/flag counters
- captured output remains local in memory; no UDP transmission is introduced

Do not add UDP transport, Android playback, jitter-buffer timing, Opus, discovery,
pairing, or encryption during Phase 2.2.
