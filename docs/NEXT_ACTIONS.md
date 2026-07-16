# Next Actions

## Phase 2.4 — Default-device invalidation and capture recovery

Stay on branch `phase/02-windows-capture`.

Implement the next bounded Windows capture increment:

- detect `AUDCLNT_E_DEVICE_INVALIDATED` and default render-endpoint changes
- stop using the invalid capture client immediately
- discard partial normalized and 960-frame assembler state deterministically
- reacquire the current `eRender` / `eConsole` default endpoint
- re-read and validate the endpoint mix format
- restart loopback capture only when the locked 48 kHz stereo float contract remains supported
- mark the first completed PCM frame after recovery as discontinuous
- add bounded restart-attempt, successful-restart, failed-restart, and device-change counters
- keep recovery state transitions platform-neutral and deterministically testable
- keep COM, endpoint notification, and WASAPI ownership inside the Windows sender layer

## Phase 2.4 validation gate

- all existing eight native tests remain green
- new recovery-state tests cover invalidation, restart success, restart failure,
  repeated invalidation, reset, and unsupported replacement formats
- a Windows live probe survives a default-output-device change or simulated
  invalidation without crashing or retaining stale capture state
- the sender reports recovery and discontinuity counters
- completed PCM remains local in memory; no transport is introduced

Do not add UDP transport, Android playback, jitter-buffer timing, Opus, discovery,
pairing, or encryption during Phase 2.4.
