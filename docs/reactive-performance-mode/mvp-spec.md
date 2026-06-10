# Reactive Performance Mode MVP Specification

Issue: #1

## Product Intent

Reactive Performance Mode turns Ardour into a playable song-system. The MVP should prove that clips, cues, tracks, macros, routing, MIDI input, harmony state, rhythm state, and musical clock position can be declared as reactive state and controlled in real time without a mouse after setup.

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
- Harmony states: named harmonic values such as key, scale, chord, section, or progression step declared by the action file.
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
DO rhythm insert 0
DO rhythm route 0 density midi-value
DO trigger probability 0 0 midi-value
DO macro filter midi-value ramp 0|1|0
DO harmony key C_minor
DO harmony chord i
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

ACTION scene.drop
TRIGGER scene 3
QUANTIZE 0|1|0
DO cue 3
DO state section drop
END

ACTION region.drop
TRIGGER region Breakdown Loop
QUANTIZE 0|1|0
DO cue 3
DO state section drop
END
```

Required MVP commands:

- `cue <row>`
- `trigger <route-index> <row-index>`
- `trigger probability <route-index> <row-index> <value|midi-value>`
- `trigger-stop <route-index>`
- `stop-all`
- `transport play`
- `transport stop`
- `scene apply <index>`
- `scene store <index>`
- `macro <name> <value|midi-value> [ramp <bbt-offset>]`
- `macro snapshot store <name>`
- `macro snapshot recall <name> [ramp <bbt-offset>]`
- `macro morph <from-snapshot> <to-snapshot> [amount <0..1|midi-value>] [ramp <bbt-offset>]`
- `state <name> <value>`
- `harmony <name> <value>`
- `rhythm <param> <value>`
- `rhythm insert <route-index>`
- `rhythm route <route-index> <param> <value|midi-value>`

Required chain modes:

- `all`: execute commands in order.
- `sequential`: execute one child per trigger, rotating each time.
- `random`: choose one child per trigger.

Required MVP conditions:

- `WHEN state <name> <value>`
- `WHEN harmony <name> <value>`
- `WHEN macro <name> <value>`
- `WHEN transport rolling`
- `WHEN transport stopped`

Phase 3i stores and enforces these conditions in the Reactive action engine before previewing or triggering commands. Unmet conditions fail visibly without mutating macro/state/harmony values, advancing sequential chains, or updating the last action.

Phase 4h wires transport conditions to the live Ardour bridge:

- `ReactiveActionSlotRunner` accepts a transport-state provider and refreshes the engine's rolling/stopped flag before manual-slot previews, MIDI-event previews, manual-slot execution, and MIDI-event/MIDI-byte execution.
- `ARDOUR_UI` supplies that provider from `Session::transport_state_rolling()`, so `WHEN transport rolling` and `WHEN transport stopped` follow Ardour's transport state machine rather than a stale engine default.
- The provider is deliberately non-realtime and lives at the GTK/session bridge boundary; Generic MIDI continues to send only compact trigger bytes through `BasicUI`, and action planning remains outside the audio process callback.

Required validation:

- Duplicate action names fail.
- Unknown commands fail with file/line information.
- Unknown conditions fail with file/line information.
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
- `Reactive/toggle-performance-mode`

The current `share/midi_maps/reactive-performance-mvp.map` binds notes 36 through 43 to those action slots, note 44 to `Reactive/show-action-document-status`, note 45 to `Reactive/reload-action-document`, and note 47 to `Reactive/toggle-performance-mode`.

`Reactive/toggle-performance-mode` arms or disarms Reactive Performance Mode from a controller or key binding. When disabled, manual slots and live MIDI-triggered reactive actions report a visible disabled execution status without dispatching target commands, mutating macro/state/harmony values, or advancing sequential chains. Status/reload commands remain available so a performer can inspect or repair the setup before re-enabling the mode.

`Reactive/reload-action-document` clears the cached action document for the current session and reloads using the same lookup order. Successful reloads report whether the session file, user file, or built-in fallback was loaded. Failed reloads report the configured file path and parse/read error without silently falling back.

`Reactive/show-action-document-status` opens the minimal Phase 5 status panel. It shows whether Reactive Performance Mode is enabled, the loaded source type, configured path or fallback label, action count, last load error, latest execution status, performance-control availability for the first eight slots, a latest-attempted slot marker, next-action preview, session state, track state, mixer scene state, routing status for the first eight controller-facing routes, the first eight action slots in a controller-bank-style summary, the first eight macro slots with current values, and the first eight user-defined state slots with current values. The panel includes large trigger controls for the first eight slots, an Enable/Disable Mode control, and a Reload control that uses the same reload path as `Reactive/reload-action-document`.

Macro-bank rows are discovered from `DO macro ...` commands in the loaded action document. Names are listed once in first-seen document order, default to `0.0` before execution, and reflect the latest values written by executed macro commands.

State-bank rows are discovered from `DO state ...` commands in the loaded action document. Names are listed once in first-seen document order, default to an empty value before execution, and reflect the latest values written by executed state commands.

Harmony-bank rows are discovered from `DO harmony ...` commands in the loaded action document. Names are listed once in first-seen document order, default to an empty value before execution, and reflect the latest values written by executed harmony commands.

The backend runner can now execute a loaded document from a parsed `ReactiveMidiEvent`. It matches MIDI note and CC triggers through `ReactiveActionEngine::match_midi_event(...)`, executes the first matching action in document order, and records the matched action index/name in the same last-execution status used by numbered slots.

`ReactiveMidiEvent::from_midi_bytes(...)` maps 3-byte note-on and control-change controller messages into this event model. Raw MIDI status channels are converted to the musician-facing 1-based channel numbers used by action syntax, so status `0x99` maps to `ch=10`. Note-off, note-on with velocity 0, unsupported statuses, short messages, and null buffers are ignored.

`ReactiveActionSlotRunner::execute_midi_bytes(...)` now provides the backend bridge from raw controller bytes to executable actions. It preserves the missing-document failure path, maps supported byte messages through `ReactiveMidiEvent::from_midi_bytes(...)`, delegates supported messages to `execute_midi_event(...)`, and records unsupported byte messages in the same last-execution status model.

The backend runner can also execute loaded documents from a named `ReactiveMarkerEvent`. It matches exact `TRIGGER marker <name>` triggers through `ReactiveActionEngine::match_marker_event(...)`, uses the first matching action in document order, supports preview/status reporting, and can queue nonzero-quantized marker actions through the same scheduler path as manual slots and MIDI triggers.

Live Ardour marker triggers are bridged by a narrow `Session`/`Location` adapter in GTK/session space. The adapter observes visible named location markers, detects forward transport crossings, resets on stop, locate, or backward movement, skips markers that have no matching reactive action, and delegates matched hits to the same marker runner API. It intentionally rides the existing Reactive Performance panel/queue poll loop for the MVP; native sample-accurate `SessionEvent` scheduling remains future work.

The backend runner can now execute loaded documents from a numbered `ReactiveSceneEvent`. It parses `TRIGGER scene <index>` with a non-negative scene index, matches exact scene indexes through `ReactiveActionEngine::match_scene_event(...)`, reports readable `scene <index>` labels for previews, status, and queued summaries, and can execute or queue matching actions through the same scheduler path as manual slots, MIDI triggers, and marker triggers. A narrow GTK/Cue-page adapter now emits scene events for Cue-row launches without modifying `Session::trigger_cue_row(...)`; lower-level BasicUI/control-surface/session observation remains a follow-up.

The backend runner can now execute loaded documents from a named `ReactiveRegionEvent`. It parses `TRIGGER region <name>` while preserving multi-word names, matches exact region names through `ReactiveActionEngine::match_region_event(...)`, reports readable `region <name>` labels for previews, status, and queued summaries, and can execute or queue matching actions through the same scheduler path as manual slots, MIDI triggers, marker triggers, and scene triggers. A narrow GTK/session adapter now observes active track playlists for named, non-hidden region crossings; native sample-accurate scheduling remains a follow-up.

Live Ardour region triggers are bridged by a narrow track-playlist adapter in GTK/session space. The adapter observes named, non-hidden regions on active track playlists, detects forward crossings of their timeline start positions, resets on stop, locate, or backward movement, skips regions that have no matching reactive action, and delegates matched hits to the same region runner API. It intentionally rides the existing Reactive Performance panel/queue poll loop for the MVP; native sample-accurate `SessionEvent` scheduling remains future work.

Phase 4e research found that the existing Generic MIDI surface already supports fixed controller-to-action mappings through `MIDIAction`, which is how `share/midi_maps/reactive-performance-mvp.map` launches stable `Reactive/trigger-action-N` slots. Document-level `TRIGGER midi ...` matching needed one more adapter because fixed action dispatch does not pass the original note/CC bytes to Reactive Performance.

Phase 4f adds that live adapter path for Generic MIDI note-on and control-change bindings. A map entry can now use `reactive="trigger"` with `note` or `ctl`; the Generic MIDI surface reconstructs the 3-byte controller message on Ardour's existing MIDI/control-surface thread, emits it through `BasicUI`, and the GTK-side Reactive Performance entry point delegates to `ReactiveActionSlotRunner::execute_midi_bytes(...)`. The MVP map keeps notes 36 through 45 for fixed slot/status/reload actions and adds note 46 on channel 10 plus CC 22 on channel 1 as document-level trigger examples.

Phase 4g adds event-derived macro values. `DO macro <name> midi-value [ramp <bbt-offset>]` stores a macro command whose value is resolved from the MIDI event that triggered the action: CC values and note velocities are normalized from `0..127` to `0.0..1.0`, and the optional ramp is preserved. Literal macro commands such as `DO macro filter 0.80` are unchanged. Because manual slot execution has no originating controller value, `midi-value` actions should be triggered through a matching MIDI note/CC path.

Phase 3h adds macro snapshot store/recall. `DO macro snapshot store <name>` captures the current Reactive macro values under a named snapshot after any earlier macro commands in the same action have been applied. `DO macro snapshot recall <name> [ramp <bbt-offset>]` expands the stored values back into normal macro commands, preserving the optional recall ramp so existing executor, controller feedback, and performance UI paths can apply the recalled values. Missing snapshots fail the action plan visibly instead of silently changing live macro state.

Phase 3k adds first-class macro morphs. `DO macro morph <from-snapshot> <to-snapshot> [amount <0..1|midi-value>] [ramp <bbt-offset>]` expands shared macro values from two stored snapshots into ordinary macro commands by linear interpolation, defaulting to `amount 0.5`. Literal amounts are constrained to `0.0..1.0`; `midi-value` resolves from the triggering note velocity or CC value. Missing snapshots or snapshots with no shared macro names fail the action plan visibly without mutating live macro state or advancing last-action status.

Phase 3j adds first-class harmony state actions. `DO harmony <name> <value>` updates a dedicated harmony read model, and `WHEN harmony <name> <value>` gates actions against that model without conflating harmonic state with general user state. The executor currently treats harmony as a non-session no-op target command, so this slice gives the UI, controller workflow, and action engine stable key/chord/scale values before any MIDI chord generation or clip mutation work.

## Performance UI

Add the smallest useful UI surface, preferably integrated with the existing Cue page:

- Enable/disable Reactive Performance Mode.
- Load/reload action file.
- Show active action bank.
- Show 8 macro values with names and current value.
- Show 8 harmony values with names and current value.
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
- Keep UI buttons, controller output-port wiring, script preset installation, and demo-session routing as follow-up work.

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
- Actual MIDI output-port wiring remains a follow-up so this slice stays backend-first and reusable.

Phase 6q adds route-scoped rhythm parameter actions:

- Parse `DO rhythm route <route-index> <param> <value|midi-value>` as a distinct command from global rhythm updates and route insertion.
- Resolve `<route-index>` through Ardour's controller-facing remote route order, matching `DO rhythm insert <route-index>`.
- Update only the targeted route's `Reactive Rhythm State MVP` insert.
- Keep `DO rhythm <param> <value>` as the broadcast form for simple demo-wide changes.
- Return explicit errors for missing routes, missing rhythm inserts on the target route, and unknown rhythm parameters.

Phase 6r allows route-scoped rhythm commands to use controller-derived values:

- `DO rhythm route <route-index> <param> midi-value` resolves the value from the MIDI event that triggered the action, using the same normalized `0.0..1.0` CC/velocity mapping as macro `midi-value`.
- The MVP demo's CC 22 action inserts the route 0 rhythm module if needed, drives route 0 density from the controller value, updates the `filter` macro read model for UI feedback, and morphs `texture`/`space` between stored macro snapshots; the routing summary shows the resulting route 0 density.
- General macro-to-plugin or macro-to-Ardour-parameter routing remains a follow-up; this slice only connects controller values to existing route-scoped rhythm parameters.

Phase 6s exposes TriggerBox follow probability as a declarative action:

- `DO trigger probability <route-index> <row-index> <value|midi-value>` sets one trigger slot's Ardour follow-action probability without launching or stopping the slot.
- Literal values are normalized `0.0..1.0`; `midi-value` uses the same controller value resolution as macro and route-scoped rhythm commands.
- `ReactiveSessionTarget` resolves the controller-facing triggerbox route via `Session::triggerbox_at(...)`, resolves the slot with `TriggerBox::trigger(...)`, converts the normalized value to Ardour's `0..100` follow-probability value, and reports missing route/slot/value errors visibly.
- The MVP demo's CC 22 action also drives `DO trigger probability 0 0 midi-value`, so the same knob can mutate route 0 rhythm density, route 0 slot 0 clip follow probability, macro state, and macro morph preview details.

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

Phase 4i adds backend marker-trigger execution:

- Represent marker hits as named `ReactiveMarkerEvent` values.
- Match exact `TRIGGER marker <name>` declarations in document order without affecting MIDI matching.
- Preview and execute the first matching marker action through `ReactiveActionSlotRunner`.
- Queue nonzero-quantized marker actions using the same TempoMap-backed scheduler path and queued-action summaries.

Phase 4j bridges live Ardour markers to that backend path:

- Detect forward crossings of visible named `Session` location markers from the GTK/session poll path.
- Reset the detector when transport stops, moves backward, locates, reloads the action document, or toggles Reactive Performance Mode.
- Delegate matching crossings to `ReactiveActionSlotRunner::execute_or_queue_marker_event(...)` so quantized marker actions reuse the existing scheduler.
- Skip unmatched timeline markers so ordinary session markers do not produce noisy reactive failures.
- Keep marker scanning, document lookup, and action planning outside realtime audio callbacks; exact sample-accurate native scheduling remains a later `SessionEvent` design.

Phase 4k adds backend scene-trigger execution:

- Parse `TRIGGER scene <index>` with non-negative scene indexes and clear validation errors.
- Represent scene hits as numbered `ReactiveSceneEvent` values.
- Match exact scene-index triggers in document order without affecting MIDI or marker matching.
- Preview, execute, and quantize/queue the first matching scene action through `ReactiveActionSlotRunner`.
- Keep lower-level mixer-scene, TriggerBox, BasicUI/control-surface, and native session observation as follow-up work; the first live adapter is the GTK/Cue-page row launch path.

Phase 4l adds backend region-trigger execution:

- Parse `TRIGGER region <name>` while preserving multi-word region names and rejecting empty names with clear validation errors.
- Represent region hits as named `ReactiveRegionEvent` values.
- Match exact region-name triggers in document order without affecting MIDI, marker, or scene matching.
- Preview, execute, and quantize/queue the first matching region action through `ReactiveActionSlotRunner`.
- Keep live Ardour region, playlist, or editor-selection adapter wiring as follow-up work so this slice remains backend-first and non-realtime.

Phase 4m bridges live Ardour timeline regions to that backend path:

- Represent named, non-hidden timeline regions as `ReactiveRegionObservation` values with start sample and route name/order metadata.
- Detect forward crossings of region starts from the GTK/session poll path and reset on stop, locate, discontinuity, or backward movement.
- Delegate matching crossings to `ReactiveActionSlotRunner::execute_or_queue_region_event(...)` so quantized region actions reuse the existing scheduler.
- Skip unmatched regions so ordinary session regions do not produce noisy reactive failures.
- Keep playlist scanning, document lookup, and action planning outside realtime audio callbacks; exact sample-accurate native scheduling remains a later `SessionEvent` design.

Phase 4n bridges Cue-page launches to the scene backend path:

- Emit `ReactiveSceneEvent::numbered(row)` when Cue rows are launched through `ARDOUR_UI::trigger_cue_row(...)` or the Cue-page row-click path.
- Preview first and skip unmatched rows quietly so ordinary Cue-page launches do not produce reactive failures.
- Delegate matched rows to `ReactiveActionSlotRunner::execute_or_queue_scene_event(...)` with the existing TempoMap-backed scheduler.
- Preserve the normal cue launch even when Reactive Performance Mode is disabled, no action document is loaded, no scene trigger matches, or the reactive action fails.
- Keep `Session::trigger_cue_row(...)` unchanged so reactive `DO cue` commands do not feed back into scene triggers; broader BasicUI/control-surface/session observation remains follow-up work.

Phase 5: add minimal Reactive Performance UI panel.

Phase 5a adds the reusable read model for that panel:

- `ReactiveActionSlotRunner` can summarize the first controller action bank as slot index, action name, primary trigger label, command count, and latest-attempted marker.
- MIDI note/CC triggers are formatted in musician-facing syntax such as `MIDI note ch=10 note=36` and `MIDI cc ch=1 cc=22 value>63`.
- The existing status dialog displays this action-bank summary as the first panel-oriented UI slice.
- The later full panel should still move this into the Cue-page performance surface with macro/state/harmony values and queued-action preview.

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

Phase 5t adds the matching panel read model for harmony state:

- `ReactiveActionSlotRunner` can summarize the first harmony bank as slot index, harmony name, and current value.
- Harmony names are discovered from loaded action documents in stable first-seen document order, with duplicate harmony commands collapsed to one row.
- Harmony values default to an empty string before execution and update as `DO harmony ...` commands run through the action engine.
- The existing status dialog displays this compact harmony bank below the state bank, and the Cue-page panel summary includes the same harmony values.

Phase 5d adds the reusable next-action preview read model for that panel:

- `ReactiveActionEngine::preview_action(...)` builds the same action-plan metadata as execution without running target commands, updating macro/state/harmony values, marking last action, or advancing sequential action chains.
- `ReactiveActionSlotRunner` can preview a manual slot or the first matching MIDI event as slot index, action name, primary trigger label, chain mode, quantize label, next command count, and a bounded set of musician-readable command details.
- MIDI-event previews pass the matched note/CC event into planning, so `midi-value` commands, trigger follow probabilities, and macro morph amounts preview the current controller value instead of a stale literal default.
- Preview planning refreshes the live transport-state provider first, so transport-gated actions appear unavailable while their `WHEN transport ...` condition is unmet.
- After a slot or MIDI event is executed, the runner refreshes a cached next-action preview for the same control so repeated sequential actions show the next planned command count and command detail without consuming it.
- The existing status dialog displays this next-action preview below the latest execution status, while the Cue-page panel shows a compact single-line command preview.

Phase 5e adds the first visible reactive-routing read model:

- `ReactiveSessionTarget` can summarize the first controller-facing routes as route index, route name, whether `Reactive Rhythm State MVP` is present, and a compact routing status label.
- The summary uses the same `Session::get_remote_nth_route(...)` order as route-scoped rhythm actions, so rows match `DO rhythm route <route-index> ...`.
- The existing status dialog displays this compact routing section below the next-action preview.
- MIDI feedback, a dedicated Cue-page panel, and live transport-clock release of queued actions remain follow-up work.

Phase 5r makes that routing read model show live rhythm parameter values:

- Inserted `Reactive Rhythm State MVP` routes expose structured density, chance, priority, and rotation values in `ReactiveRoutingSlotSummary`.
- The formatted routing summary displays compact values such as `density=0.25 chance=0.50 priority=2 rotation=4`.
- The existing status dialog and Cue-page panel summary inherit those values through `ReactiveSessionTarget::format_routing_summary(...)`, so controller-driven rhythm changes are visible without adding new UI ownership.

Phase 5u adds the first trigger-slot/clip-state read model:

- `ReactiveSessionTarget` summarizes bounded trigger-visible routes in Ardour's sorted trigger-track order, matching the controller-facing TriggerBox route order used by `Session::triggerbox_at(...)`.
- `ReactiveTriggerSlotSummary` exposes route index, slot index, route name, trigger-region name when present, triggerbox presence, populated/empty state, playable state, follow probability, and a compact status label.
- The formatted trigger-slot summary displays rows such as `0/0: Reactive Rhythm Lane - Reactive Cue 0 Reset playable follow=65%` and `0/1: Reactive Rhythm Lane - empty`.
- The existing status dialog and Cue-page panel summary display this read-only section next to routing, so seeded demo clips and empty slots are visible without adding a new clip-grid implementation or mutating normal Ardour trigger behavior.

Phase 5v adds the first session/clock state read model:

- `ReactiveSessionTarget` exposes `ReactiveSessionStateSummary` for the loaded-session boundary, including transport stopped/rolling state, transport sample, BBT position, tempo, meter, total route count, trigger-visible route count, and a compact status string.
- The formatted session-state summary displays rows such as `stopped @ 1|1|0 tempo=120.00 meter=4/4 sample=0 routes=3 trigger-routes=3`.
- The existing status dialog and Cue-page panel summary display this read-only section next to routing and trigger-slot state, giving performers live musical-clock context without adding realtime session mutation or a parallel transport model.

Phase 5w adds the first track-state read model:

- `ReactiveSessionTarget` exposes bounded `ReactiveTrackStateSummary` rows in the same controller-facing route order used by route-scoped rhythm actions.
- Rows include route index, stable route id, route name, selected/unselected state, active/inactive state, mute state, solo state, record-enable availability/state for real tracks, gain value, and a compact status string.
- The formatted track-state summary displays rows such as `0: Reactive Rhythm Lane [1234] - active selected unmuted unsoloed rec-off gain=1.00` and uses `rec=-` for non-track routes.
- The existing status dialog and Cue-page panel summary display this read-only section next to session state, routing, and trigger-slot state without adding new route-mutating commands.

Phase 5x extends the session transport read model:

- `ReactiveSessionStateSummary` now includes transport speed, session record-enabled state, play-loop state, and locate-pending state from Ardour's existing `Session` APIs.
- The formatted session-state summary displays rows such as `stopped @ 1|1|0 speed=0.00 record=off loop=off locate=idle tempo=120.00 meter=4/4 sample=0 routes=3 trigger-routes=3`.
- The status dialog and Cue-page panel receive these fields through the existing `format_session_state_summary()` path, keeping the slice read-only and avoiding new transport ownership, realtime callbacks, or UI-specific state.

Phase 5y adds the first mixer scene-state read model:

- `ReactiveSessionTarget` exposes bounded `ReactiveMixerSceneSummary` rows from Ardour's existing mixer-scene slots.
- Rows include scene slot, display name, stored/empty validity, last-touched state, and a compact status string.
- The formatted scene-state summary displays rows such as `2: Reactive Drop Snapshot - stored last-touched`, while sparse unset slots are shown as `Scene 1 - empty`.
- The status dialog and Cue-page panel summary display this read-only section beside session and track state, so scene validity is visible without adding new mixer-scene mutation behavior.

Phase 5f adds an explicit mode-arm toggle:

- `ReactiveActionSlotRunner` exposes enabled/disabled state and a compact status string for the status panel.
- Disabled mode blocks slot, MIDI-event, and MIDI-byte execution before target dispatch or action-engine mutation.
- `Reactive/toggle-performance-mode` toggles the state from Ardour action bindings, and the MVP MIDI map binds note 47 for controller-first arming/disarming.
- A dedicated Cue-page panel and controller feedback output remain follow-up work.

Phase 5g adds performance-safe controls to the status surface:

- `ReactiveActionSlotRunner` exposes bounded performance-control rows with slot index, action name, primary trigger label, availability, enabled state, and a large-control button label.
- The existing status dialog shows eight trigger buttons, disables unavailable or disarmed slots, and sends button presses through the same slot-execution path used by controller actions.
- The same dialog includes an Enable/Disable Mode button and refreshes status text after mode changes, reloads, and slot execution.
- A dedicated Cue-page panel, controller LED feedback, and richer layout remain follow-up work.

Phase 5h adds the first dedicated Cue-page panel scaffold:

- `ReactivePerformancePanel` lives in the Cue page main content area, above the trigger strip grid.
- The panel provides eight large slot buttons for `Reactive/trigger-action-0` through `Reactive/trigger-action-7` behavior by calling the same `ARDOUR_UI::trigger_reactive_action(...)` path used by action bindings and MIDI maps.
- The panel provides Mode, Reload, and Status controls that call the existing Reactive Performance mode-toggle, document reload, and status-dialog paths.
- This scaffold intentionally keeps live read-model rendering, button-label synchronization, controller feedback, and richer layout as follow-up work.

Phase 5i binds that Cue-page panel to the existing performance-control read model:

- `ARDOUR_UI` exposes a narrow `reactive_performance_control_summary(...)` accessor that reuses `ReactiveActionSlotRunner::performance_control_summary(...)`.
- The Cue-page panel refreshes slot labels and sensitivity from loaded action documents and the current Reactive Performance enabled state.
- Empty or missing slots are disabled, and disarming Reactive Performance Mode disables slot buttons while leaving Mode, Reload, and Status available.
- The panel refreshes after local slot, Mode, Reload, and Status interactions.

Phase 5j adds controller-driven panel refresh feedback:

- `ReactiveActionSlotRunner::performance_control_summary(...)` prefixes the latest attempted slot's large-control label with `> ` after manual or MIDI-triggered execution, giving the panel a compact performance feedback marker.
- `ARDOUR_UI::ReactivePerformanceChanged` is emitted after Reactive slot execution, MIDI-byte execution, mode toggles, and document reloads.
- `ReactivePerformancePanel` observes that signal on the GUI context, so controller-triggered Reactive actions refresh the Cue-page controls without a mouse interaction.
- The Cue-page panel now also shows a compact live read-model summary for next action preview, macros, user states, and first controller-facing routing rows, while the existing status dialog remains the detailed/debug view.
- Controller LED byte generation and output-port wiring remain follow-up work at this phase.

Phase 5k adds the first controller-feedback read model:

- `ReactiveActionSlotRunner::controller_feedback_summary(...)` returns bounded slot feedback rows with slot index, action name, primary trigger label, availability, enabled state, latest-attempted state, and a controller-output value.
- Feedback values are deterministic for the MVP: unavailable or disabled rows report `0`, enabled idle rows report `32`, and the latest attempted enabled row reports `127`.
- Manual slot execution and MIDI-triggered execution both move the latest-attempted feedback row.
- Disabled Reactive Performance Mode keeps feedback rows available but disabled and reports zero output values.
- Actual MIDI/LED byte generation and output-port wiring remain follow-up work at this phase; this slice only creates the tested data source that an output adapter can consume.

Phase 5l bridges the controller-feedback read model to raw MIDI messages:

- `ReactiveControllerFeedbackBinding` describes a slot-to-controller output binding for note or CC feedback using musician-facing MIDI channels `1` through `16`.
- `ReactiveActionSlotRunner::controller_feedback_midi_messages(...)` turns those bindings into deterministic three-byte note/CC feedback messages.
- The generated values reuse the Phase 5k rules: unavailable or disabled rows send `0`, enabled idle rows send `32`, and the latest attempted enabled row sends `127`.
- Mapped empty slots still generate zero-valued messages so controller LEDs can be cleared when an action document changes.
- Invalid channel or note/CC numbers are ignored instead of producing malformed bytes.
- Actual Generic MIDI output-port wiring remains follow-up work; this slice proves the reusable byte adapter that surface code can consume.

Phase 5m declares Reactive feedback bindings in the Generic MIDI map:

- The bundled `reactive-performance-mvp.map` now declares `reactive="feedback"` bindings for slots `0` through `7` on the same channel-10 pad notes `36` through `43`.
- Feedback bindings are separate XML rows from the action trigger rows so they do not collide with `Reactive/trigger-action-*` bindings.
- `GenericMidiControlProtocol` accepts `reactive="feedback"` rows, validates slot/channel/note-or-CC metadata, and stores them as `ReactiveControllerFeedbackBinding` values for a later output-wiring slice.
- Invalid feedback bindings are ignored cleanly instead of being reported as unknown Reactive trigger targets.

Phase 5n wires those feedback bindings to Generic MIDI's output port through a cached non-realtime bridge:

- `BasicUI` now publishes Reactive feedback binding changes from Generic MIDI maps and precomputed Reactive feedback MIDI messages from the GTK-owned runner.
- `ARDOUR_UI` keeps the active feedback bindings, recomputes slot feedback bytes after document reloads, mode changes, manual slot execution, and live MIDI-triggered execution, then publishes those bytes back to Generic MIDI.
- `GenericMidiControlProtocol` caches the latest feedback messages and writes only cached byte vectors from its existing feedback tick with a try-lock, avoiding parser, document loading, runner planning, or allocation on the realtime feedback path.
- Physical controller behavior remains hardware/output-routing dependent and should be smoke-tested with the selected controller after connecting Ardour's Generic MIDI Control Out port.

Phase 5o adds the first backend queued-action scheduler:

- `ReactiveActionScheduler` queues already-planned actions with slot index, action name, primary trigger label, quantize value, requested BBT, due BBT, and planned commands.
- The scheduler reports bounded queued-action summary rows for UI/status surfaces, including formatted request/due times, command count, bounded command details, and whether the item is currently due.
- `pop_due(...)` releases due actions in deterministic due-time order, preserving queue id order for actions due at the same BBT.
- Zero-quantize plans are treated as due at the request BBT even when a later due BBT is supplied, so immediate actions cannot get stuck in the queue.
- This slice is deliberately backend-only and non-realtime. The later session-clock bridge should compute due BBT from Ardour's `TempoMap`, enqueue non-immediate actions from the runner, and execute popped actions from a safe UI/session context.

Phase 5p connects the scheduler to the runner without adding realtime session events:

- `ReactiveActionSlotRunner::execute_or_queue_slot(...)` and `execute_or_queue_midi_event(...)` plan actions on the existing control/session-side runner boundary.
- Nonzero-quantize plans are queued with caller-supplied requested/due BBT values and do not dispatch target commands until `release_due_queued_actions(...)` is called.
- Zero-quantize plans still execute immediately through the existing executor path.
- Queued MIDI-triggered plans preserve the matched action slot and event-derived values such as `DO macro <name> midi-value`, `DO trigger probability <route> <row> midi-value`, and `DO macro morph <from> <to> amount midi-value`.
- The runner exposes bounded queued-action summaries and a compact formatter for Cue-page/status UI display, including resolved command details from the frozen queued plan.
- Clearing/replacing the action document and disabling Reactive Performance Mode clear pending queued actions, keeping disarm/reload behavior predictable under performance pressure.
- This remains a non-realtime bridge. The next slice should compute due BBT from Ardour's `TempoMap` and poll/release due actions from a safe GTK/session clock context before any native `SessionEvent` design.

Phase 5q connects queued actions to the session clock without adding a native realtime `SessionEvent` type:

- `ReactiveActionClock` converts the current transport BBT and an action quantize offset into requested/due BBT values using Ardour's `TempoMap`.
- The runner now has TempoMap-backed `execute_or_queue_slot(...)`, `execute_or_queue_midi_event(...)`, and `execute_or_queue_midi_bytes(...)` overloads, so callers do not parse preview text or guess an action's quantize value before planning.
- `ARDOUR_UI` uses the current session transport sample to compute the request BBT, then routes manual panel/status actions and Generic MIDI trigger bytes through the queue-aware runner path.
- The Cue-page Reactive Performance panel polls queued actions from GTK space every 100 ms and releases due actions through `ReactiveSessionTarget`, keeping execution out of realtime callbacks.
- The always-visible panel summary and detailed status dialog now include queued-action summary text and command details so performers can see pending quantized work and what it will do when released.
- This is still an MVP clock bridge: it uses polling rather than sample-accurate native `SessionEvent` scheduling, and exact hardware/controller smoke testing remains follow-up.

Phase 5s makes queued quantized actions visible in that controller-feedback model:

- `ReactiveControllerFeedbackSummary` marks slots with pending queued Reactive actions.
- Deterministic MVP feedback values are now `0` for unavailable or disabled, `32` for enabled idle, `96` for enabled queued, and `127` for the latest attempted enabled slot.
- Latest-attempted feedback takes priority over queued feedback for the same slot, so a just-pressed quantized action still gives immediate full-bright confirmation while it remains pending.
- Queued feedback clears when pending actions release, when Reactive Performance Mode is disabled, or when the action document is cleared/reloaded through the existing queue-clearing paths.
- The existing cached Generic MIDI feedback bridge inherits the queued value through `controller_feedback_midi_messages(...)`; no new realtime scheduling or hardware-specific controller behavior is added in this slice.

Phase 5t makes queued-action previews more useful under performance pressure:

- `ReactiveQueuedActionSummary` now carries a bounded list of musician-readable command summaries copied from the queued plan.
- The summaries are generated after action planning, so queued MIDI-triggered actions preserve already-resolved controller values such as CC-derived macro values, TriggerBox follow probabilities, and macro morph amounts.
- `ReactiveActionSlotRunner::format_queued_action_summary(...)` prints those command details under each pending action for the status dialog and Cue-page panel read model.
- The output remains bounded and read-only; queued execution, release ordering, and realtime boundaries are unchanged.

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
- Bind note 47 to `Reactive/toggle-performance-mode`.
- Bind note 46 and CC 22 as live document-level `TRIGGER midi` examples through `reactive="trigger"`, including CC-driven `midi-value` macro, macro morph, TriggerBox follow-probability, and route-scoped rhythm examples in session-local action files.

Phase 7c packages a small demo asset:

- `examples/reactive-performance-mvp/reactive-actions.txt` is a source-controlled session-local action document with pad-triggered rhythm insertion/reset, route-scoped rhythm updates, TriggerBox follow-probability updates, harmony-state updates, transport-gated actions, state changes, a CC-derived macro action, and a CC-derived macro morph between stored texture snapshots.
- `examples/reactive-performance-mvp/README.md` documents the manual session layout, controller map, feedback routing, and smoke checks.
- `ReactiveActionDocumentLoaderTest::packagedDemoSessionActionFileLoads` loads the packaged action file as a session document so parser drift breaks automated tests.
- A full `.ardour` session archive remains a follow-up because current session XML is generated, ID-heavy, and environment-dependent.

Phase 7e adds the first repeatable demo-session template path:

- `share/scripts/reactive_performance_mvp_session.lua` is a bundled `SessionInit` script named `Reactive Performance MVP`, so it appears in Ardour's new-session template list as a factory template.
- The script creates three MIDI-only controller-facing routes named `Reactive Rhythm Lane`, `Reactive Harmony Lane`, and `Reactive Macro Lane`, giving the demo action document stable route-index targets without hand-authoring generated session XML.
- `LuaScriptTest::reactive_performance_session_init_script_test` verifies the script is discoverable in the `SessionInit` catalog and compiles through Ardour's Lua factory path.
- The example README now recommends creating a session from this template; Phase 7g removes the remaining manual copy step by having the template install the demo action document.
- A full session archive with clips/cues remains a later demo-production step; the template script keeps this slice repeatable while the final route/clip layout is still evolving.

Phase 7g makes the template setup path closer to one-step:

- `share/scripts/reactive_performance_mvp_session.lua` now writes the demo `reactive-actions.txt` into the new session folder after creating the three MIDI lanes.
- The script leaves an existing session-local `reactive-actions.txt` untouched, so user-edited action documents are not overwritten.
- `LuaScriptTest::reactive_performance_session_init_installs_demo_action_document_test` executes the actual Lua template against a fake session and verifies the installed document exactly matches `examples/reactive-performance-mvp/reactive-actions.txt`.
- `LuaScriptTest::reactive_performance_session_init_keeps_existing_action_document_test` verifies the template does not overwrite an existing document.
- `LuaScriptTest::reactive_performance_session_init_tolerates_unavailable_file_io_test` verifies that hardened Lua settings which remove file I/O do not crash the template; in that case users can still copy the packaged action file manually.
- The example README and demo guide no longer require manual action-file copying for new sessions created from the factory template; manual existing-session setup still uses the packaged example file.

Phase 7h makes the template-created lanes Cue-page visible:

- `share/scripts/reactive_performance_mvp_session.lua` passes Ardour's `trigger_visibility=true` flag for `Reactive Rhythm Lane`, `Reactive Harmony Lane`, and `Reactive Macro Lane`.
- The template still creates the same three named MIDI lanes and installs the same session-local demo action document, but the lanes now appear as trigger-visible Cue-page routes instead of ordinary hidden-from-Cues MIDI tracks.
- `LuaScriptTest` records the `new_midi_track(...)` trigger-visibility argument in the SessionInit harness and verifies all three demo lanes request it.
- At this phase, generated musical note content remained a later demo-production step; Phase 7o now adds the first simple note content to the Cue-page trigger clips.

Phase 7i adds marker-trigger demo coverage:

- `examples/reactive-performance-mvp/reactive-actions.txt` includes `demo.marker.breakdown`, triggered by a visible session marker named `Breakdown`.
- The action uses existing safe commands to insert the rhythm processor if needed, lower route 0 density/chance, set the `filter` macro, update harmony/state read models, and launch cue row 2.
- The `Reactive Performance MVP` SessionInit template embeds matching content, and automated tests require the packaged document to expose a `marker Breakdown` preview.
- The example README and demo guide include a smoke check for adding/crossing the marker and confirming panel/status read-model changes.

Phase 7j adds region-trigger demo coverage:

- `examples/reactive-performance-mvp/reactive-actions.txt` includes `demo.region.breakdown.loop`, triggered by a non-hidden timeline region named `Breakdown Loop`.
- The action uses existing safe commands to insert the rhythm processor if needed, adjust route 0 density/chance, set the `filter` macro, update harmony/state read models, and launch cue row 3.
- The `Reactive Performance MVP` SessionInit template embeds matching content, and automated tests require the packaged document to expose a `region Breakdown Loop` preview.
- The example README and demo guide include a smoke check for crossing the named region and confirming panel/status read-model changes.

Phase 7k adds scene-trigger demo coverage:

- `examples/reactive-performance-mvp/reactive-actions.txt` includes `demo.scene.drop`, triggered by Cue row 3 through `TRIGGER scene 3`.
- The action uses existing safe commands to insert the rhythm processor if needed, adjust route 0 density/chance/rotation, set the `filter` macro, and update harmony/state read models while leaving the normal Cue-page launch path responsible for launching the row.
- The `Reactive Performance MVP` SessionInit template embeds matching content, and automated tests require the packaged document to expose a `scene 3` preview.
- The example README and demo guide include a smoke check for launching Cue row 3 and confirming panel/status read-model changes.

Phase 7l seeds the first timeline landmark in the repeatable template:

- `ARDOUR.LuaAPI.ensure_session_marker(...)` provides a non-realtime setup helper for SessionInit and other Lua scripts to create a visible named session marker without exposing direct `Location` ownership to Lua.
- The helper returns false for invalid inputs, creates the marker when missing, and avoids duplicate visible markers by name.
- `share/scripts/reactive_performance_mvp_session.lua` uses the helper to seed a visible `Breakdown` marker for `demo.marker.breakdown` while preserving the three trigger-visible MIDI lanes and existing action-document install behavior.
- `LuaScriptTest` covers both the real helper against an Ardour test session and the SessionInit template's marker request in the fake-session harness.
- A full `.ardour` session archive with richer generated musical cue content remains a later demo-production step.

Phase 7m seeds the first region-trigger landmark in the repeatable template:

- `ARDOUR.LuaAPI.ensure_session_midi_region(...)` provides a non-realtime setup helper for SessionInit and other Lua scripts to create a named, visible MIDI region on a named MIDI route playlist without exposing direct source/region ownership to Lua.
- The helper returns false for invalid inputs, missing routes, non-MIDI routes, or unavailable playlists; creates the region when missing; and avoids duplicate visible regions by name on the target route.
- `share/scripts/reactive_performance_mvp_session.lua` uses the helper to seed a non-hidden `Breakdown Loop` timeline region on `Reactive Rhythm Lane` for `demo.region.breakdown.loop`, preserving the three trigger-visible MIDI lanes, `Breakdown` marker, and existing action-document install behavior.
- `LuaScriptTest` covers both the real helper against an Ardour test session and the SessionInit template's region request in the fake-session harness.
- This is a timeline landmark, not generated musical note content; a full `.ardour` session archive remains a later demo-production step.

Phase 7n seeds the first Cue-page trigger clip placeholders in the repeatable template:

- `ARDOUR.LuaAPI.ensure_session_midi_trigger_region(...)` provides a non-realtime setup helper for SessionInit and other Lua scripts to create a named MIDI region in a named MIDI route's TriggerBox slot without exposing direct source/region ownership to Lua.
- The helper returns false for invalid inputs, missing routes, non-MIDI routes, missing TriggerBoxes, invalid slots, or non-positive lengths; leaves an already-populated slot untouched; and uses a setup-only TriggerBox region path that updates active-slot bookkeeping immediately.
- `share/scripts/reactive_performance_mvp_session.lua` uses the helper to seed `Reactive Cue 0 Reset`, `Reactive Cue 1 Tighten`, and `Reactive Cue 2 Sparse` on `Reactive Rhythm Lane`; `Reactive Harmony i` on `Reactive Harmony Lane`; and `Reactive Macro Filter` plus `Reactive Macro Texture` on `Reactive Macro Lane`.
- `LuaScriptTest` covers both the real helper against an Ardour test session and the SessionInit template's six trigger-region requests in the fake-session harness.
- These are named Cue-page placeholders at this phase; Phase 7o adds first-note content while leaving a full `.ardour` session archive and richer generated arrangement as later demo-production work.

Phase 7o seeds first-note content into the demo trigger clips:

- `ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note(...)` provides a non-realtime setup helper that creates a named MIDI TriggerBox region and inserts one MIDI note while the slot is empty.
- The helper returns false for invalid route, slot, length, note timing, channel, note number, or velocity inputs; if the slot already has any region, it returns true without overwriting or adding notes to existing performer content.
- `share/scripts/reactive_performance_mvp_session.lua` uses the helper to seed notes 36, 38, 41, 48, 60, and 67 into the six named demo trigger clips.
- `LuaScriptTest` covers both the real helper against an Ardour test session and the SessionInit template's six trigger-note requests in the fake-session harness.
- This is deliberately simple monophonic note content, not a polished generated arrangement or full `.ardour` session archive.

Phase 7p makes the generated demo clips more musical without committing session XML:

- `ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_notes(...)` provides a non-realtime setup helper that creates a named MIDI TriggerBox region and inserts a Lua table of notes while the slot is empty.
- The helper rejects empty note tables, invalid note timing, invalid channel/note/velocity values, missing fields, missing routes, invalid slots, and nonpositive clip lengths before creating a region.
- If the slot already has any region, it returns true without overwriting or appending to existing performer content.
- `share/scripts/reactive_performance_mvp_session.lua` keeps the first three rhythm clips monophonic and seeds the harmony/macro clips with triads: 48/51/55, 60/64/67, and 67/72/74.
- `LuaScriptTest` covers both the real helper against an Ardour test session and the SessionInit template's 12 seeded note requests in the fake-session harness.

## Acceptance Tests

- Parser unit tests cover valid actions, duplicate names, invalid commands, invalid quantize values, random/sequential chain modes, MIDI note triggers, MIDI CC triggers, literal macro ramps, `midi-value` macro ramps, macro snapshot/morph syntax, route-scoped rhythm `midi-value`, TriggerBox follow-probability commands, and harmony commands/conditions.
- Engine tests cover action lookup, chain state, literal and event-derived macro state, macro snapshot recall, literal and MIDI-derived macro morphs, harmony state and conditions, route-scoped rhythm event values, TriggerBox follow-probability event values, and quantization calculation against a fixed TempoMap.
- Scheduler tests cover queued-action summaries with bounded command details, including TriggerBox follow-probability details, deterministic due popping, zero-quantize immediate due behavior, and queue clearing.
- Runner queue tests cover quantized slot/MIDI action queuing, formatted queued command details, explicit due release, zero-quantize immediate execution, clear/load queue reset, disabled-mode blocking, event-derived MIDI macro preservation, route-scoped rhythm value dispatch, and TriggerBox follow-probability dispatch.
- Clock bridge tests cover zero, bar, beat, and sample-derived BBT quantize calculations plus runner TempoMap-backed slot and MIDI-byte queuing.
- Session-target tests cover route-scoped rhythm insertion, parameter writes, TriggerBox follow-probability dispatch, missing-target errors, routing summaries with live rhythm parameter values, trigger-slot summaries for populated and empty slots, session/clock state summaries, and track-state summaries.
- Controller-feedback tests cover idle/latest/queued/disabled values and MIDI byte generation for queued quantized actions.
- Lua template tests cover the repeatable demo session template, installed action-document drift, idempotent marker/region/TriggerBox helper behavior, and the 12 seeded demo trigger-clip notes.
- Manual smoke test can trigger one cue row, one harmony-state update, and one CC-derived macro from a MIDI map.
- UI smoke test can toggle mode, load a file, show validation errors, and preview a queued action.
- Reactive rhythm tests and demo confirm density 0 mutes note-ons, density 1 passes them, chance 0 drops them, note-off handling avoids stuck notes, and non-note MIDI passes unchanged.
