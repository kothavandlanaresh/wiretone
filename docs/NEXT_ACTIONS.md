# Next Actions

## Phase 1.4 — In-memory session state

Implement a pure C++ sender/receiver session state machine around the locked
packet, control-payload, fragmentation, and reassembly contracts:

- require `stream_start` before accepting audio
- reject audio for the wrong stream or after `stream_stop`
- define deterministic stream replacement and restart behavior
- track heartbeat liveness without sockets
- feed completed frames into a bounded receiver queue
- expose counters for malformed, duplicate, expired, rejected, and dropped data
- preserve discontinuity and silence semantics
- test stop, restart, timeout, wrong-stream, and queue-overflow behavior

## Gate

Phase 1.4 must pass native exact-state-transition and malformed-input tests under
both Windows MSVC and Android NDK compilation.

Do not add UDP sockets, WASAPI capture, Android playback, jitter-buffer timing,
or Opus until the in-memory session-state tests pass.
