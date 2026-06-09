# Reactive Performance Mode MVP Specification

Issue: #1

## Product Intent

Reactive Performance Mode turns Ardour into a playable song-system. The MVP should prove that clips, cues, tracks, macros, routing, MIDI input, rhythm state, and musical clock position can be declared as reactive state and controlled in real time without a mouse after setup.

This is an experimental mode. Normal Ardour behavior must remain unchanged when the mode is not enabled.

## MVP Scope

The MVP is successful when a performer can:

1. Load a demo Ardour session with several Cue/Trigger tracks.
2. Enable Reactive Performance Mode.
3. Load a readable action definition file.
4. Trigger named action chains from MIDI notes/CCs.
5. Launch cue rows or trigger slots through action chains.
6. Adjust mappable macros from MIDI controls.
7. See active state and next quantized action preview in a small UI panel.
8. Insert a reactive rhythm MIDI processor that applies density, chance, priority filtering, rotation, and quantized parameter updates.
9. Run normal Ardour workflows outside the mode without changed behavior.

## Reactive State Model

The MVP state graph should expose stable node identities for:

- Transport: stopped, rolling, speed, record-enabled, loop state, locate pending.
- Musical time: sample position, beats, BBT, tempo, meter, quantization grid.
- Routes/tracks: route id, name, selected state, active, mute, solo, record enable, gain.
- Trigger boxes: route index, row index, current trigger, trigger state, launch style, quantization, follow actions, probability, progress.
- Cues/scenes: cue row id, pending cue, active cue, mixer scene id, scene validity.
- MIDI input: note on/off, CC, program, sysex/msg pattern, channel, value, velocity, source map.
- Parameters/macros: named macro value, source control, target controls, ramp state, snapshot value.
- Routing states: named route groups or route ids, input/output port names, send targets where available.
- User performance states: booleans, enums, counters, and named modes declared by the action file.

The first implementation can keep this as a non-realtime snapshot plus signal subscriptions. It does not need to become a full graph database.

## Declarative Action Syntax

Use a small line-oriented text format for the first parser. It is intentionally ClyphX-like in spirit but not a dependency and not syntax-compatible.

Example:

```text
ACTION intro.drop
TRIGGER midi note ch=10 note=36
WHEN transport rolling
QUANTIZE 1|0|0
DO cue 0
DO macro filter 0.72 ramp 0|2|0
DO scene apply 0
END

ACTION drums.mutate
TRIGGER midi cc ch=1 cc=22 value>63
QUANTIZE 0|1|0
CHAIN random
DO rhythm density 0.35
DO rhythm density 0.55
DO rhythm density 0.80
END

ACTION breakdown
TRIGGER marker Breakdown
QUANTIZE 1|0|0
CHAIN sequential
DO trigger 2 4
DO trigger 3 4
DO macro delay_send 0.65 ramp 1|0|0
DO transport stop after 4|0|0
END
```

Required MVP commands:

- `cue <row>`
- `trigger <route-index> <row-index>`
- `trigger-stop <route-index>`
- `stop-all`
- `transport play`
- `transport stop`
- `scene apply <index>`
- `scene store <index>`
- `macro <name> <value> [ramp <bbt-offset>]`
- `state <name> <value>`
- `rhythm <param> <value>`

Required chain modes:

- `all`: execute commands in order.
- `sequential`: execute one child per trigger, rotating each time.
- `random`: choose one child per trigger.

Required validation:

- Duplicate action names fail.
- Unknown commands fail with file/line information.
- Invalid quantization fails.
- MIDI trigger conflicts are reported before mode is enabled.
- Ramps must reject negative durations.

## MIDI Controller Workflow

The MVP should support two paths:

1. Existing Generic MIDI action maps:
   - Add static actions such as `Reactive/trigger-action-0` through `Reactive/trigger-action-15`.
   - Add `share/midi_maps/reactive-performance-mvp.map`.
   - Map notes and CCs to those actions.

2. Reactive action file triggers:
   - Parse `TRIGGER midi note ...` and `TRIGGER midi cc ...`.
   - Bind them through the reactive engine or a thin Generic MIDI adapter in a later phase.

After setup, the demo should be playable with:

- Notes/pads to trigger action chains.
- CCs/faders/knobs to control macros.
- Optional feedback where existing MIDI output support is already available.

## Action Document Loading

Reactive Performance Mode looks for a line-oriented action document named `reactive-actions.txt`.

Lookup order:

1. Session-local document: `<session-folder>/reactive-actions.txt`.
2. User default document: `<ardour-user-config-folder>/reactive-actions.txt`.
3. Built-in MVP fallback: eight demo actions named `mvp.cue.0` through `mvp.cue.7`, mapped to cue rows 0 through 7.

The session-local document is the preferred performance path because it travels with the set. The user default document is useful for a controller-wide setup shared across sessions. The built-in fallback is only a demo preset; it is used when neither configured file exists.

If a configured file exists but cannot be read or parsed, Reactive Performance Mode reports that file path and error and does not fall back silently. This keeps a broken set-specific document visible during setup instead of triggering the wrong demo actions during performance.

The existing Generic MIDI action names remain stable:

- `Reactive/reload-action-document`
- `Reactive/show-action-document-status`
- `Reactive/trigger-action-0`
- `Reactive/trigger-action-1`
- `Reactive/trigger-action-2`
- `Reactive/trigger-action-3`
- `Reactive/trigger-action-4`
- `Reactive/trigger-action-5`
- `Reactive/trigger-action-6`
- `Reactive/trigger-action-7`

The current `share/midi_maps/reactive-performance-mvp.map` binds notes 36 through 43 to those action slots.

`Reactive/reload-action-document` clears the cached action document for the current session and reloads using the same lookup order. Successful reloads report whether the session file, user file, or built-in fallback was loaded. Failed reloads report the configured file path and parse/read error without silently falling back.

`Reactive/show-action-document-status` opens the minimal Phase 5 status panel. It shows the loaded source type, configured path or fallback label, action count, and last load error. The panel includes a Reload control that uses the same reload path as `Reactive/reload-action-document`.

## Performance UI

Add the smallest useful UI surface, preferably integrated with the existing Cue page:

- Enable/disable Reactive Performance Mode.
- Load/reload action file.
- Show active action bank.
- Show 8 macro values with names and current value.
- Show last action and next quantized action preview.
- Show parser/validation errors.
- Provide large performance-safe controls.

Do not redesign all of Ardour. Do not replace TriggerPage.

## Reactive Rhythm Prototype

First implementation target: LuaProc MIDI script.

Phase 6a adds the backend rhythm-state semantics first, independent of MIDI I/O:

- Evaluate step events against density, chance, priority, and rotation settings.
- Use explicit chance values for deterministic tests before live randomness is introduced.
- Latch pending parameter changes until a configured step boundary.
- Leave MIDI note-on/note-off handling, LuaProc wrapping, and demo routing for the next Phase 6 slice.

Parameters:

- `density`: 0.0 to 1.0.
- `chance`: 0.0 to 1.0.
- `priority_mode`: off, downbeat, pitch, velocity.
- `rotation`: integer step offset.
- `latch_quantize`: BBT offset, default 0|1|0.

Behavior:

- Incoming notes are assigned a priority score.
- Density keeps the highest-priority events and drops the rest.
- Chance applies after density.
- Rotation shifts the pattern position.
- Parameter changes are latched and applied only at the configured quantization point.
- Note-off handling must avoid stuck notes.

## Phased Roadmap

Phase 0: research architecture and extension points. Complete in this issue.

Phase 1: write design doc and MVP spec. Complete in this issue.

Phase 2: build Ardour locally and generate compile database.

Phase 3: implement parser, validator, action registry, and minimal action engine.

Phase 4: expose MIDI-triggerable actions through Generic MIDI and/or a new reactive control surface adapter.

Phase 5: add minimal Reactive Performance UI panel.

Phase 6: add reactive rhythm LuaProc script and demo routing.

Phase 7: create demo session, docs, and follow-up roadmap.

## Acceptance Tests

- Parser unit tests cover valid actions, duplicate names, invalid commands, invalid quantize values, random/sequential chain modes, MIDI note triggers, MIDI CC triggers, and macro ramps.
- Engine tests cover action lookup, chain state, macro state, and quantization calculation against a fixed TempoMap.
- Manual smoke test can trigger one cue row and one macro from a MIDI map.
- UI smoke test can enable mode, load a file, show validation errors, and preview a queued action.
- Lua rhythm test/demo confirms density 0 mutes note-ons, density 1 passes them, chance 0 drops them, and note-offs do not hang.
