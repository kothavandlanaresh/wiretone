# Next Actions

## Phase 2.3 — Assemble exact 20 ms PCM frames

Stay on branch `phase/02-windows-capture`.

Implement the next bounded Windows capture increment:

- consume variable-length normalized packet output
- assemble exact 960-frame / 20 ms stereo PCM logical frames
- preserve left/right interleaving and signed 16-bit little-endian representation
- carry capture discontinuity onto the first completed frame after a gap
- synthesize exact silent frames from silent capture regions
- keep the assembler bounded and allocation-free after initialization
- reset partial state deterministically after device invalidation or explicit restart
- expose completed frames through an in-memory callback or bounded view only

## Phase 2.3 validation gate

- all existing seven native tests remain green
- new assembler tests cover split packets, combined packets, exact packets, silence,
  clipping-independent byte preservation, discontinuity, reset, and overflow bounds
- the live sender probe reports at least one completed 960-frame logical frame
- completed-frame timestamps and discontinuity counters are visible
- captured PCM remains local in memory; no transport is introduced

Do not add UDP transport, Android playback, jitter-buffer timing, Opus, discovery,
pairing, or encryption during Phase 2.3.
