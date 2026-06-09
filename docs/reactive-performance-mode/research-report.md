# Reactive Performance Mode Research Report

Issue: #1

Date: 2026-06-08

## Executive Summary

Ardour already contains several foundations for a reactive performance instrument. The strongest MVP path is not to build a parallel clip launcher. It is to extend the existing Cue/Trigger, ControlProtocol, Generic MIDI, Lua, and TempoMap systems with a small declarative action layer.

The first implementation should be a modular experiment that preserves normal Ardour behavior:

- Native C++ for the central reactive action registry, state snapshot, scheduler, and integration with existing trigger/cue/session APIs.
- Existing Generic MIDI maps and/or a small new ControlProtocol-facing adapter for MIDI note/CC triggering.
- Existing TriggerBox and Cue APIs for clip/scene-style launch behavior.
- Existing LuaProc support for the first reactive rhythm MIDI processor, because it can process MIDI events and expose parameters without modifying core track processing.
- A small GTK2/YTK panel in the existing Cue/Trigger UI area to visualize macros, armed actions, active state, and next quantized actions.

## Source Evidence

Primary source files inspected:

- `doc/mainpage.md`: in-code architecture overview.
- `libs/ardour/ardour/session.h`: Session owns transport, routes, cue events, mixer scenes, Lua script hooks, triggerbox access, and action-relevant state.
- `libs/ardour/ardour/triggerbox.h` and `libs/ardour/triggerbox.cc`: Trigger, AudioTrigger, MIDITrigger, TriggerBox, cue recording, quantization, launch styles, follow actions, probability, MIDI trigger mapping, cue row launch.
- `gtk2_ardour/trigger_page.h`, `gtk2_ardour/trigger_strip.h`, `gtk2_ardour/cuebox_ui.h`, `gtk2_ardour/trigger_master.h`: existing performance-oriented Cue UI.
- `libs/ctrl-interface/control_protocol/control_protocol/basic_ui.h` and `libs/ctrl-interface/control_protocol/basic_ui.cc`: controller-facing transport, action access, cue/trigger, trigger bank, and mixer-scene operations.
- `libs/ctrl-interface/control_protocol/control_protocol/control_protocol.h`: dynamically loaded control protocol interface.
- `libs/ctrl-interface/midi_surface/midi_surface/midi_surface.h`: MIDI input/output surface base with parser callbacks.
- `libs/surfaces/generic_midi/*`: Generic MIDI map parser, action/function/URI bindings, feedback, banking, and MIDI learn.
- `share/midi_maps/*.map`: XML MIDI map examples using `action`, `function`, `uri`, `note`, `ctl`, `msg`, and bank metadata.
- `libs/ardour/ardour/luascripting.h`, `libs/ardour/luaproc.cc`, `libs/ardour/luabindings.cc`, `share/scripts/README`: Lua script/plugin support.
- `libs/ardour/ardour/session_event.h`, `libs/ardour/session_process.cc`, `libs/ardour/session_transport.cc`: session event queue and transport scheduling.
- `libs/temporal/temporal/tempo.h`, `libs/temporal/temporal/bbt_time.h`: tempo, meter, BBT, beats, and quantization primitives.
- `libs/ardour/wscript`, `wscript`, `doc/unit_tests.txt`: build and test entry points.

## Architecture Summary

Ardour is split into a GTK2/YTK front end and `libardour` backend:

- `gtk2_ardour/` contains the main GUI and most user-facing complexity.
- `libs/ardour/` runs sessions, routes, processors, transport, plugins, triggers, MIDI tracks, and state persistence.
- `libs/ctrl-interface/` and `libs/surfaces/` provide dynamically loaded control surfaces.
- `libs/temporal/` owns time, tempo maps, beats, BBT, meters, and conversions.
- `libs/midi++2/` and `libs/evoral/` provide MIDI/event abstractions.
- `libs/lua/`, `libs/ardour/luascripting.*`, and `libs/ardour/luaproc.*` provide scripting and Lua DSP/plugin support.

For this goal, the highest-value existing subsystem is the Cue/Trigger layer:

- `Trigger` already has launch styles, quantization, follow actions, follow probabilities, gain, velocity behavior, cue isolation, and state transitions.
- `MIDITrigger` already stores and plays MIDI clip data in beat time.
- `TriggerBox` owns up to 16 triggers per route in non-Mixbus builds, can bang/unbang triggers, stop quantized or immediately, manage cue rows, record cue launches, and maintain custom MIDI trigger bindings.
- `Session` exposes cue row and trigger access through `trigger_cue_row`, `trigger_stop_all`, `triggerbox_at`, `bang_trigger_at`, and `unbang_trigger_at`.

## Best Extension Points

### 1. Native reactive action engine in `libs/ardour`

Recommended future files:

- `libs/ardour/ardour/reactive_action.h`
- `libs/ardour/ardour/reactive_action_engine.h`
- `libs/ardour/reactive_action.cc`
- `libs/ardour/reactive_action_engine.cc`
- `libs/ardour/test/reactive_action_test.{h,cc}`
- `libs/ardour/test/reactive_action_engine_test.{h,cc}`

Why: the state graph, action chains, macro snapshots, and quantized scheduling need direct access to session state, TempoMap conversions, triggerboxes, mixer scenes, automation controls, and safe session events.

Keep the first engine non-realtime unless a specific operation already has an RT-safe Ardour path. The MVP engine should compute plans on a control/session thread, then call existing session APIs such as cue/trigger operations.

### 2. Reuse Generic MIDI for first controller mappings

Recommended first surface path:

- Add static GTK actions or BasicUI-accessible actions such as `Reactive/trigger-action-0`.
- Add `share/midi_maps/reactive-performance-mvp.map`.
- Let Generic MIDI dispatch note/CC events through existing `MIDIAction` and `MIDIFunction` machinery.

Why: Generic MIDI already parses XML map files, supports `note`, `ctl`, `msg`, actions, functions, URIs, banking, and feedback.

### 3. Add a minimal Cue-page UI panel

Recommended future UI files:

- `gtk2_ardour/reactive_performance_panel.h`
- `gtk2_ardour/reactive_performance_panel.cc`
- Small integration in `gtk2_ardour/trigger_page.{h,cc}`.

Why: the existing `TriggerPage`, `CueBoxUI`, `TriggerStrip`, and `TriggerMaster` already present the performance grid. The MVP should add one panel showing reactive action state, macro banks, active/inactive state, and next quantized action preview.

### 4. Prototype rhythm as LuaProc first

Recommended future file:

- `share/scripts/reactive_rhythm_state_mvp.lua`

Why: `LuaProc` can process MIDI events, expose parameters, and receive time/meter fields in DSP context. This is a safer first vertical slice for density, probability, priority filtering, rotation, and quantized parameter changes than changing `MIDITrigger::midi_run`.

Phase 6h source review keeps this recommendation and narrows the insertion path:

- `LuaProc::connect_and_run` already runs through the plugin processor path, maps `midiin` and `midiout` tables, and can expose DSP `time` fields when `dsp_options().time_info` is enabled.
- `PluginInsert::connect_and_run` already handles processor-chain execution, buffer mapping, automation, in-place/non-in-place processing, latency, and MIDI bypass details.
- `Route::add_processor` and processor reconfiguration are the eventual route-insertion tools, but live route mutation remains a separate integration task because it touches process locks, graph configuration, and user-visible processor order.
- `MidiChannelFilter` is a useful in-place MIDI mutation pattern, but it is an established route/track utility rather than a reason to add a bespoke reactive route hook.
- Control surfaces such as Launchpad Pro can filter controller input and should remain controller-feedback/mapping tools for this MVP, not the rhythm stream insertion point.

The MVP scaffold is therefore `ReactiveRhythmInsertionPlanner`: it chooses LuaProc/plugin insertion only when LuaProc, MIDI input/output, and DSP time info are available; it defers native processors and external plugins; it rejects MIDI route hooks and control surfaces as stream-processing targets; and it keeps live route mutation disabled until a later issue wires insertion safely.

Phase 6i packages the first script at `share/scripts/reactive_rhythm_state_mvp.lua`. It is a bundled LuaProc MIDI processor named `Reactive Rhythm State MVP`, requests `time_info`, exposes density/chance/priority/rotation/latching controls, passes non-note MIDI through, and tracks forwarded note-ons to avoid stuck-note releases. Its automated coverage proves discovery, load, insertion as a `PluginInsert`, active processing through the LuaProc harness, event-level behavior through `unit-test-reactive_rhythm_luaproc_harness`, and real Ardour `BufferSet`/`MidiBuffer` processing through `unit-test-reactive_rhythm_luaproc_plugininsert`.

Phase 6l adds `ReactiveRhythmRouteInserter`, a narrow C++ helper that locates the bundled LuaProc script, inserts it into a MIDI route through Ardour's normal `PluginInsert` and `Route::add_processor` path, configures one MIDI input/output, and returns an existing insert on repeated calls. UI commands, controller-triggered insertion, script presets, live route mutation policy, and demo-session routing remain follow-up work.

### 5. Use SessionEvent carefully

`SessionEvent` can schedule transport and realtime operations. It should be considered for later native quantized action execution, but the MVP should avoid adding a new realtime event type until tests prove the engine's allocation and locking behavior. For Phase 3, prefer existing trigger quantization and non-RT action dispatch.

## C++ vs Lua vs Plugin Boundary

Native C++ should own:

- Reactive state snapshot model.
- Declarative action parser and validation.
- Named action registry.
- Action chain execution.
- Quantization calculations against `Temporal::TempoMap`.
- Integration with `Session`, `TriggerBox`, `Route`, mixer scenes, and automation controls.
- UI-facing read model for active state and queued next actions.

Lua should own first experiments where rapid iteration matters:

- Reactive rhythm MIDI processor.
- Small session/editor scripts that prove syntax ideas.
- One-off demo session setup helpers.

Plugin or LuaProc should own:

- MIDI filtering, density, chance, priority, and rotation prototype.
- DSP-adjacent behavior that fits Ardour's existing Lua plugin model.

Do not put the central action engine in Lua for the long term. Lua is useful for proving behavior, but action scheduling, state graph identity, persistence, controller feedback, and UI integration need native types and tests.

## Risks and Constraints

- Realtime safety: do not allocate, parse, do filesystem I/O, or run arbitrary action chains in the audio process callback.
- Scope creep: Ardour already has Cue/Trigger UI; do not redesign the whole DAW.
- Shallow checkout: this workspace is currently a shallow clone. Ardour's build/version flow expects Git metadata; a full clone with tags is likely needed before reliable local builds.
- Python command: `./waf` currently fails on this machine because `python` is missing; `python3 ./waf --help` works.
- Dependency stack: macOS builds may need private Ardour/GTK dependency stacks or installed packages. `wscript` supports `--depstack-root`, `--with-backends`, `--arm64`, `--cxx17`, `--compile-database`, `--test`, and `--run-tests`.
- GitHub fork metadata: GitHub reports `tlambrou/ssosFlow` as a fork, but the CLI JSON did not return a parent object during this run.
- UI technology: the GUI is GTK2/YTK plus Ardour's canvas/widgets, not a modern web/react UI.
- Existing Generic MIDI limitation: some maps note that `msg=""` bindings work for actions/functions but not arbitrary URI controls. The MVP should route reactive commands through actions/functions first.

## What Not To Touch Yet

- Audio/MIDI backend drivers in `libs/backends/`.
- Core graph scheduling in `libs/ardour/graph.*`.
- Plugin scanner processes.
- Existing trigger realtime run loops except for well-tested follow-up changes.
- Session file format migrations beyond adding optional experimental state nodes.
- Broad UI redesign outside the Cue/Trigger page.
- Upstream build/package configuration unless a concrete build blocker requires it.

## MVP Recommendation

Build the MVP as three small vertical slices:

1. Native action core without UI:
   - Parse a small declarative action file.
   - Validate named actions and chains.
   - Execute a small set of existing session operations: cue row, trigger slot, transport stop/play, mixer scene apply/store, macro value set.
   - Compute next quantization time using `TempoMap`, but initially rely on existing trigger quantization for trigger launch.

2. Controller-first operation:
   - Add static reactive actions that Generic MIDI can call.
   - Add one MIDI map file for an 8x8 or 8x16 controller-style layout.
   - Add feedback only where existing Generic MIDI or control surface APIs already support it.

3. Performance panel and rhythm prototype:
   - Add a small Cue-page panel with action bank, macro bank, active state, and next action preview.
   - Add a LuaProc rhythm script with density, chance, priority, rotation, and quantized parameter latching.

## Open Questions

- Should reactive action definitions be session-embedded XML, external `.rpa` text files, or both?
- Which controller should be the first physical target: Launchpad, Push, KeyLab, or generic note/CC?
- Should macro snapshots map to mixer scenes, a new reactive snapshot type, or both?
- How much of TriggerBox's custom MIDI binding should be reused for reactive action bindings?
- Should queued-action preview be visible only in the new panel or also on controller feedback LEDs?
