# Reactive Rhythm LuaProc Prototype

`share/scripts/reactive_rhythm_state_mvp.lua` is the first bundled LuaProc script for the Reactive Rhythm MVP.

## Purpose

The script proves the Phase 6h insertion recommendation at the plugin/LuaProc boundary without mutating Ardour route processor chains automatically. It is intended for manual insertion or later automated insertion as a LuaProc MIDI processor named `Reactive Rhythm State MVP`.

## Parameters

- `Density %`: gates active rhythm steps and filters simultaneous note-ons.
- `Chance %`: applies deterministic probability after density filtering.
- `Priority`: chooses which simultaneous note-ons survive density filtering: `Off`, `Downbeat`, `Pitch`, or `Velocity`.
- `Rotation`: rotates the rhythm-step pattern.
- `Pattern Steps`: sets the repeating density pattern length.
- `Latch Steps`: applies changed parameters only on a rhythm-step boundary.
- `Steps Per Beat`: derives rhythm steps from Ardour beat time.
- `Chance Seed`: keeps chance decisions repeatable for tests and demos.

## Behavior

- Requests LuaProc DSP `time_info` and derives rhythm steps from `time.beat`.
- Declares one MIDI input and one MIDI output, with no audio pins.
- Passes non-note MIDI events through unchanged.
- Treats note-on with velocity `0` as note-off.
- Tracks forwarded note-ons by channel and note so matching note-offs pass, while note-offs for suppressed note-ons are suppressed.
- Applies changed parameters at configured step boundaries rather than immediately inside every block.

## Limits

- This does not insert itself into routes. Route insertion, script presets, and demo-session routing remain follow-up work.
- Event-level Lua behavior is covered by `unit-test-reactive_rhythm_luaproc_harness`, which embeds Lua, stubs the LuaProc globals, and calls this script's `dsp_run` with deterministic MIDI events. A future Ardour-buffer-through-`PluginInsert` test can still cover the full runtime path.
- Step assignment uses the block's current beat time, not each event's sample offset inside the block. Sample-offset-aware Lua behavior is a later refinement.
