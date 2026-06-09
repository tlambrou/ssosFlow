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
DO rhythm insert 0
DO rhythm density 0.35
DO rhythm route 0 chance 0.50
DO rhythm density 0.55
DO rhythm density 0.80
END

ACTION filter.sweep
TRIGGER midi cc ch=1 cc=23
QUANTIZE 0|1|0
DO macro filter midi-value ramp 0|1|0
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
- `macro <name> <value|midi-value> [ramp <bbt-offset>]`
- `state <name> <value>`
- `rhythm <param> <value>`
- `rhythm insert <route-index>`

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
   - Bind them through the live Generic MIDI `reactive="trigger"` adapter.
   - Use `DO macro <name> midi-value` to map the incoming note velocity or CC value onto a normalized `0.0..1.0` macro value.

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

The built-in fallback now demonstrates the rhythm path as well as cue launching:

- `mvp.cue.0` inserts `Reactive Rhythm State MVP` on controller route 0, resets rhythm density/chance/priority/rotation, then launches cue row 0.
- `mvp.cue.1` through `mvp.cue.7` keep launching cue rows 1 through 7 while changing density, chance, priority mode, or rotation.
- The eight action names stay stable so existing Generic MIDI action bindings continue to address the same slots.

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

The current `share/midi_maps/reactive-performance-mvp.map` binds notes 36 through 43 to those action slots, note 44 to `Reactive/show-action-document-status`, and note 45 to `Reactive/reload-action-document`.

`Reactive/reload-action-document` clears the cached action document for the current session and reloads using the same lookup order. Successful reloads report whether the session file, user file, or built-in fallback was loaded. Failed reloads report the configured file path and parse/read error without silently falling back.

`Reactive/show-action-document-status` opens the minimal Phase 5 status panel. It shows the loaded source type, configured path or fallback label, action count, last load error, latest execution status, next-action preview, routing status for the first eight controller-facing routes, the first eight action slots in a controller-bank-style summary, the first eight macro slots with current values, and the first eight user-defined state slots with current values. The panel includes a Reload control that uses the same reload path as `Reactive/reload-action-document`.

Macro-bank rows are discovered from `DO macro ...` commands in the loaded action document. Names are listed once in first-seen document order, default to `0.0` before execution, and reflect the latest values written by executed macro commands.

State-bank rows are discovered from `DO state ...` commands in the loaded action document. Names are listed once in first-seen document order, default to an empty value before execution, and reflect the latest values written by executed state commands.

The backend runner can now execute a loaded document from a parsed `ReactiveMidiEvent`. It matches MIDI note and CC triggers through `ReactiveActionEngine::match_midi_event(...)`, executes the first matching action in document order, and records the matched action index/name in the same last-execution status used by numbered slots.

`ReactiveMidiEvent::from_midi_bytes(...)` maps 3-byte note-on and control-change controller messages into this event model. Raw MIDI status channels are converted to the musician-facing 1-based channel numbers used by action syntax, so status `0x99` maps to `ch=10`. Note-off, note-on with velocity 0, unsupported statuses, short messages, and null buffers are ignored.

`ReactiveActionSlotRunner::execute_midi_bytes(...)` now provides the backend bridge from raw controller bytes to executable actions. It preserves the missing-document failure path, maps supported byte messages through `ReactiveMidiEvent::from_midi_bytes(...)`, delegates supported messages to `execute_midi_event(...)`, and records unsupported byte messages in the same last-execution status model.

Phase 4e research found that the existing Generic MIDI surface already supports fixed controller-to-action mappings through `MIDIAction`, which is how `share/midi_maps/reactive-performance-mvp.map` launches stable `Reactive/trigger-action-N` slots. Document-level `TRIGGER midi ...` matching needed one more adapter because fixed action dispatch does not pass the original note/CC bytes to Reactive Performance.

Phase 4f adds that live adapter path for Generic MIDI note-on and control-change bindings. A map entry can now use `reactive="trigger"` with `note` or `ctl`; the Generic MIDI surface reconstructs the 3-byte controller message on Ardour's existing MIDI/control-surface thread, emits it through `BasicUI`, and the GTK-side Reactive Performance entry point delegates to `ReactiveActionSlotRunner::execute_midi_bytes(...)`. The MVP map keeps notes 36 through 45 for fixed slot/status/reload actions and adds note 46 on channel 10 plus CC 22 on channel 1 as document-level trigger examples.

Phase 4g adds event-derived macro values. `DO macro <name> midi-value [ramp <bbt-offset>]` stores a macro command whose value is resolved from the MIDI event that triggered the action: CC values and note velocities are normalized from `0..127` to `0.0..1.0`, and the optional ramp is preserved. Literal macro commands such as `DO macro filter 0.80` are unchanged. Because manual slot execution has no originating controller value, `midi-value` actions should be triggered through a matching MIDI note/CC path.

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

Phase 6b adds a backend MIDI-shaped adapter before touching Ardour's live MIDI buffers:

- Convert note-on events into `ReactiveRhythmEvent` inputs for the rhythm engine.
- Forward passed note-ons and suppress dropped note-ons.
- Track note history by channel/note so note-offs for passed notes are forwarded, while note-offs for suppressed notes do not create bogus releases.
- Keep chance deterministic through explicit per-event chance values.
- Leave direct `MidiBuffer` processing, LuaProc wrapping, randomness injection, and demo routing for follow-up work.

Phase 6c adds raw MIDI-byte mapping before live buffer mutation:

- Map 3-byte MIDI note-on/note-off messages into the backend rhythm MIDI adapter.
- Treat note-on with velocity 0 as note-off.
- Keep original raw bytes attached to each decision so a later `MidiBuffer` processor can forward or suppress the source event.
- Pass non-note messages through unchanged.
- Leave direct `MidiBuffer` mutation, live randomness injection, LuaProc wrapping, and demo routing for follow-up work.

Phase 6d adds a backend `MidiBuffer` adapter before live routing integration:

- Read real Ardour `MidiBuffer` events and convert note messages through the raw MIDI-byte adapter.
- Rebuild the source buffer with only forwarded events while preserving event time, event type, and raw bytes.
- Treat note-on with velocity 0 as note-off through the shared byte mapper.
- Forward non-note MIDI events unchanged.
- Keep chance deterministic through explicit per-event chance values while live randomness remains unconnected.
- Leave live randomness injection, LuaProc wrapping, processor insertion, and demo routing for follow-up work.

Phase 6e adds a realtime-safe chance source for buffer processing:

- Generate normalized chance values from a deterministic, resettable seed without heap state in the generator.
- Allow the backend `MidiBuffer` adapter to process a buffer from generated chance values while keeping the explicit chance-vector API for tests.
- Cover chance-source-driven pass/drop behavior with real `MidiBuffer` objects.
- Leave processor insertion, LuaProc wrapping, clock-derived step selection, and demo routing for follow-up work.

Phase 6f adds frame-derived step selection for buffer processing:

- Map event frame/sample positions to rhythm step indices from an origin frame and positive frames-per-step value.
- Clamp events before the origin to step zero and normalize invalid frames-per-step values to avoid divide-by-zero behavior.
- Allow the backend `MidiBuffer` adapter to process a buffer with both generated chance values and frame-derived rhythm steps.
- Cover a density/priority case where frame-derived downbeat priority, not event order, chooses the forwarded note.
- Leave transport-tempo conversion, processor insertion, LuaProc wrapping, and demo routing for follow-up work.

Phase 6g adds tempo-derived step sizing:

- Calculate frames per rhythm step from sample rate, BPM, and steps-per-beat using deterministic rounding.
- Normalize invalid tempo inputs to avoid divide-by-zero or zero-frame steps.
- Allow the frame step source to be configured from tempo settings while preserving the explicit frames-per-step path.
- Leave Ardour TempoMap/transport wiring, processor insertion, LuaProc wrapping, and demo routing for follow-up work.

Phase 6h researches and scaffolds the live MIDI insertion path:

- Use LuaProc/plugin insertion as the first MVP stream-processing path when LuaProc has MIDI input/output and DSP time info available.
- Keep native C++ processors and external plugins as deferred alternatives for later performance or packaging needs.
- Treat MIDI route hooks and control surfaces as the wrong first insertion point for rhythm stream mutation.
- Add a backend insertion planner that records this policy and keeps live route mutation disabled.
- Leave actual route insertion, LuaProc script packaging, and demo-session routing for follow-up work.

Phase 6i adds the first LuaProc rhythm script prototype:

- Bundle `share/scripts/reactive_rhythm_state_mvp.lua` as `Reactive Rhythm State MVP`.
- Declare MIDI input/output, request DSP time info, and expose density, chance, priority, rotation, pattern length, latch steps, steps-per-beat, and chance seed.
- Pass non-note MIDI through unchanged and track forwarded note-ons so suppressed note-ons do not create bogus note-offs.
- Validate script discovery and plugin activation through the LuaProc test harness.
- Leave automated route insertion, script presets, event-level LuaProc buffer tests, and demo-session routing for follow-up work.

Phase 6j adds event-level LuaProc script coverage:

- Embed Lua in a CppUnit harness and stub the LuaProc globals used by `reactive_rhythm_state_mvp.lua`.
- Cover default note pass-through, density-zero suppression, non-note pass-through, velocity-zero note-offs, velocity-priority filtering, and latched parameter updates.
- Keep full Ardour `PluginInsert` MIDI-buffer execution and route insertion as follow-up work.

Phase 6k adds runtime-path LuaProc/`PluginInsert` MIDI coverage:

- Load `Reactive Rhythm State MVP` as a LuaProc plugin, wrap it in `PluginInsert`, configure one MIDI input/output, and run real `BufferSet`/`MidiBuffer` data through `PluginInsert::run`.
- Cover density-100 forwarding, density-zero note suppression with non-note pass-through, velocity-zero note-offs, and beat/time-driven latched parameter updates through the actual Ardour buffer mapping path.
- Keep automated route insertion, script presets, and demo-session routing as follow-up work.

Phase 6l adds a route-level LuaProc insertion helper:

- Add `ReactiveRhythmRouteInserter` to locate the bundled `Reactive Rhythm State MVP` LuaProc script and insert it into a MIDI route through Ardour's normal `PluginInsert` and `Route::add_processor` path.
- Configure the inserted processor for one MIDI input and one MIDI output, and verify it remains active and discoverable in the route processor list.
- Make repeated helper calls return the existing insert instead of creating duplicates.
- Keep action commands, UI commands, controller-triggered insertion, script preset installation, live route mutation policy, and demo-session routing as follow-up work.

Phase 6m exposes route insertion through the reactive action path:

- Parse `DO rhythm insert <route-index>` as a distinct command from numeric rhythm parameter updates.
- Execute that command through `ReactiveActionExecutor` and `ReactiveSessionTarget`, using `Session::get_remote_nth_route()` for the controller-facing route index.
- Call `ReactiveRhythmRouteInserter::ensure_inserted()` and treat an already-present insert as success.
- Return clear action-target errors for missing routes and route insertion failures.
- Keep UI buttons, controller feedback messages, script preset installation, and demo-session routing as follow-up work.

Phase 6n makes the built-in MVP fallback a small playable rhythm demo:

- Move the fallback action document into libardour so it is testable outside the GTK UI.
- Keep eight stable actions named `mvp.cue.0` through `mvp.cue.7`.
- Add route-level rhythm insertion and rhythm-state mutations to the fallback while preserving cue-row launches for the eight controller pads.
- Keep session-local and user action documents as the preferred performance path.

Phase 6o maps rhythm parameter actions onto inserted LuaProc controls:

- Apply `DO rhythm <param> <value>` to every existing `Reactive Rhythm State MVP` insert in the session.
- Map `density` and `chance` from normalized action values `0.0..1.0` to LuaProc percent controls `0..100`.
- Map `priority_mode` or `priority` to `Priority`, and `rotation` to `Rotation`.
- Return explicit errors for unknown rhythm parameters or sessions with no inserted rhythm processor.

Phase 6p adds the first execution-status read model for UI and controller feedback:

- `ReactiveActionSlotRunner` records the latest slot execution attempt, including slot index, action name when known, success/failure, error text, and executed command count.
- Missing documents, out-of-range slots, and target failures are recorded as status, not only returned to the caller.
- Clearing or loading an action document resets stale execution status.
- The Reactive Performance status dialog includes this latest execution summary.
- Actual MIDI feedback output remains a follow-up so this slice stays backend-first and reusable.

Phase 6q adds route-scoped rhythm parameter actions:

- Parse `DO rhythm route <route-index> <param> <value>` as a distinct command from global rhythm updates and route insertion.
- Resolve `<route-index>` through Ardour's controller-facing remote route order, matching `DO rhythm insert <route-index>`.
- Update only the targeted route's `Reactive Rhythm State MVP` insert.
- Keep `DO rhythm <param> <value>` as the broadcast form for simple demo-wide changes.
- Return explicit errors for missing routes, missing rhythm inserts on the target route, and unknown rhythm parameters.

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

Phase 4b adds backend MIDI-trigger execution from loaded action documents:

- Execute a loaded action document from a parsed `ReactiveMidiEvent`.
- Support note and CC trigger matches through the existing engine matcher.
- Use the first matching action in document order while preserving explicit numbered slot execution.
- Record last-execution status for matched MIDI-triggered actions.
- Keep live controller-event plumbing as a follow-up to avoid changing Ardour behavior outside explicit Reactive paths.

Phase 4c adds controller MIDI-byte mapping:

- Parse 3-byte note-on and control-change messages into `ReactiveMidiEvent`.
- Convert raw MIDI channels from 0-based status nibbles to 1-based action syntax channels.
- Ignore note-off, note-on with velocity 0, unsupported statuses, short messages, and null buffers.
- Keep this as a backend bridge for a later live MIDI/control-surface adapter.

Phase 4d adds runner-level MIDI-byte execution:

- Accept raw controller bytes at `ReactiveActionSlotRunner`.
- Preserve missing-document errors before parsing bytes.
- Reuse `ReactiveMidiEvent::from_midi_bytes(...)` and `execute_midi_event(...)`.
- Record unsupported byte messages in the normal last-execution status.
- Keep live control-surface and MIDI-port callback integration as the next explicit bridge.

Phase 4e chooses the live adapter hook:

- Keep existing Generic MIDI action mappings for fixed slot launches.
- Add a Reactive-specific Generic MIDI invokable or equivalent surface adapter for document-level MIDI triggers.
- Reconstruct supported 3-byte note-on/CC messages on Ardour's existing MIDI/control-surface thread.
- Delegate to the UI Reactive Performance entry point and then `ReactiveActionSlotRunner::execute_midi_bytes(...)`.
- Avoid parser, file loading, action planning, or session mutation in realtime audio callbacks.

Phase 4f wires the live Generic MIDI adapter:

- Add `reactive="trigger"` MIDI map bindings for note-on and control-change messages.
- Reconstruct matching 3-byte MIDI messages from Generic MIDI parser callbacks.
- Emit those bytes through `BasicUI` and handle them in the GTK Reactive Performance UI entry point.
- Keep existing `action="Reactive/trigger-action-N"` bindings unchanged for fixed slot launches.

Phase 4g maps controller values to macros:

- Parse `DO macro <name> midi-value [ramp <bbt-offset>]` as a dynamic macro command.
- Resolve the command value from the matched MIDI CC value or note velocity during event-triggered execution.
- Normalize controller values from `0..127` to `0.0..1.0` before updating macro state and dispatching the target command.
- Preserve existing literal macro commands, ramp metadata, and MIDI trigger matching behavior.

Phase 5: add minimal Reactive Performance UI panel.

Phase 5a adds the reusable read model for that panel:

- `ReactiveActionSlotRunner` can summarize the first controller action bank as slot index, action name, primary trigger label, command count, and latest-attempted marker.
- MIDI note/CC triggers are formatted in musician-facing syntax such as `MIDI note ch=10 note=36` and `MIDI cc ch=1 cc=22 value>63`.
- The existing status dialog displays this action-bank summary as the first panel-oriented UI slice.
- The later full panel should still move this into the Cue-page performance surface with macro/state values and queued-action preview.

Phase 5b adds the reusable macro-bank read model for that panel:

- `ReactiveActionSlotRunner` can summarize the first controller macro bank as slot index, macro name, and current value.
- Macro names are discovered from loaded action documents in stable first-seen document order, with duplicate macro commands collapsed to one row.
- Macro values default to `0.0` before execution and update as `DO macro ...` commands run through the action engine.
- The existing status dialog displays this compact macro bank below the action bank.

Phase 5c adds the reusable state-bank read model for that panel:

- `ReactiveActionSlotRunner` can summarize the first user-defined state bank as slot index, state name, and current value.
- State names are discovered from loaded action documents in stable first-seen document order, with duplicate state commands collapsed to one row.
- State values default to an empty string before execution and update as `DO state ...` commands run through the action engine.
- The existing status dialog displays this compact state bank below the macro bank.

Phase 5d adds the reusable next-action preview read model for that panel:

- `ReactiveActionEngine::preview_action(...)` builds the same action-plan metadata as execution without running target commands, updating macro/state values, marking last action, or advancing sequential action chains.
- `ReactiveActionSlotRunner` can preview a manual slot or the first matching MIDI event as slot index, action name, primary trigger label, chain mode, quantize label, and next command count.
- After a slot or MIDI event is executed, the runner refreshes a cached next-action preview for the same control so repeated sequential actions show the next planned command count without consuming it.
- The existing status dialog displays this compact next-action preview below the latest execution status.

Phase 5e adds the first visible reactive-routing read model:

- `ReactiveSessionTarget` can summarize the first controller-facing routes as route index, route name, whether `Reactive Rhythm State MVP` is present, and a compact routing status label.
- The summary uses the same `Session::get_remote_nth_route(...)` order as route-scoped rhythm actions, so rows match `DO rhythm route <route-index> ...`.
- The existing status dialog displays this compact routing section below the next-action preview.
- MIDI feedback, a dedicated Cue-page panel, and true queued-action scheduling remain follow-up work.

Phase 6: add reactive rhythm buffer processing, LuaProc script or processor insertion, and demo routing.

Phase 7: create demo session, docs, and follow-up roadmap.

Phase 7a adds the first practical demo guide:

- Document the local build/run path, minimal session layout, Generic MIDI map, built-in fallback pad behavior, and session-local `reactive-actions.txt` example.
- Include smoke-test checks for app boot, action document status, slot execution feedback, rhythm insertion, and route-scoped rhythm parameter updates.
- Keep the guide honest about current manual setup limits until an Ardour demo session archive is packaged.

Phase 7b makes the MVP map more controller-first:

- Keep notes 36 through 43 mapped to `Reactive/trigger-action-0` through `Reactive/trigger-action-7`.
- Bind note 44 to `Reactive/show-action-document-status`.
- Bind note 45 to `Reactive/reload-action-document`.
- Bind note 46 and CC 22 as live document-level `TRIGGER midi` examples through `reactive="trigger"`, including CC-driven `midi-value` macro examples in session-local action files.

## Acceptance Tests

- Parser unit tests cover valid actions, duplicate names, invalid commands, invalid quantize values, random/sequential chain modes, MIDI note triggers, MIDI CC triggers, literal macro ramps, and `midi-value` macro ramps.
- Engine tests cover action lookup, chain state, literal and event-derived macro state, and quantization calculation against a fixed TempoMap.
- Manual smoke test can trigger one cue row and one CC-derived macro from a MIDI map.
- UI smoke test can enable mode, load a file, show validation errors, and preview a queued action.
- Reactive rhythm tests and demo confirm density 0 mutes note-ons, density 1 passes them, chance 0 drops them, note-off handling avoids stuck notes, and non-note MIDI passes unchanged.
