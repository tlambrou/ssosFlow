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

Phase 6l adds `ReactiveRhythmRouteInserter`, a narrow C++ helper that locates the bundled LuaProc script, inserts it into a MIDI route through Ardour's normal `PluginInsert` and `Route::add_processor` path, configures one MIDI input/output, and returns an existing insert on repeated calls.

Phase 6m exposes that helper through the declarative action path as `DO rhythm insert <route-index>`. The parser treats insertion as distinct from numeric rhythm parameter updates, the executor dispatches it through `ReactiveActionTarget::rhythm_insert`, and `ReactiveSessionTarget` resolves the controller-facing route index with `Session::get_remote_nth_route()` before calling `ReactiveRhythmRouteInserter::ensure_inserted()`.

Phase 6n moves the built-in Reactive Performance MVP fallback action document into libardour and expands it from cue-only actions into a small playable rhythm demo. The eight stable `mvp.cue.0` through `mvp.cue.7` slots still launch cue rows, while slot 0 inserts and resets `Reactive Rhythm State MVP` on controller route 0 and later slots mutate density, chance, priority mode, or rotation.

Phase 6o maps `DO rhythm <param> <value>` actions onto existing `Reactive Rhythm State MVP` LuaProc controls through Ardour's `AutomationControl` path. Density and chance use normalized action values scaled to LuaProc percent controls, while priority/priority_mode and rotation map directly to their named controls. The action target applies these changes to every existing rhythm insert in the session and reports explicit errors for unknown parameters or missing inserts.

Phase 6p adds a native execution-status read model to `ReactiveActionSlotRunner`. The runner now records the latest slot attempt, action name when known, success/failure, error text, and executed command count; status is reset when the document is cleared or replaced. The existing Reactive Performance status dialog shows the summary as a first UI feedback surface. Controller output wiring, script presets, live route mutation policy beyond explicit action commands, and full demo-session routing remain follow-up work.

Phase 6q adds `DO rhythm route <route-index> <param> <value>` for route-scoped rhythm parameter updates. The parser keeps it distinct from broadcast `DO rhythm <param> <value>` and insertion `DO rhythm insert <route-index>`, the executor dispatches it through `ReactiveActionTarget::rhythm_route`, and `ReactiveSessionTarget` resolves the route through `Session::get_remote_nth_route()`. This keeps broadcast changes available for simple demos while allowing a controller action to mutate one live lane without touching every inserted rhythm processor.

Phase 6r extends route-scoped rhythm parameters to accept `midi-value`. The parser stores `DO rhythm route <route-index> <param> midi-value` with the same event-derived value source used by macro commands, and the action engine resolves it from the triggering note velocity or CC value before executor dispatch. The packaged demo uses CC 22 to ensure route 0 has `Reactive Rhythm State MVP`, drive route 0 density, and retain the `filter` macro read model for panel feedback. This gives the MVP one real controller-to-parameter path without solving the broader macro-to-plugin routing problem yet.

Phase 6s exposes TriggerBox follow probability through the same declarative action boundary. `DO trigger probability <route-index> <row-index> <value|midi-value>` parses as a distinct command from trigger launching, uses the existing normalized event-value resolver, dispatches through `ReactiveActionTarget::trigger_probability(...)`, and lets `ReactiveSessionTarget` resolve `Session::triggerbox_at(...)` plus `TriggerBox::trigger(...)` before setting Ardour's `0..100` follow-action probability. This keeps clip-probability mutation outside realtime trigger run loops while making the MVP's clip/scene/probability story more concrete. The packaged CC 22 demo now drives route 0 rhythm density, route 0 slot 0 follow probability, macro state, and macro morph previews from one controller.

Phase 4b connects the existing note/CC trigger matcher to executable loaded action documents through `ReactiveActionSlotRunner::execute_midi_event(...)`. This keeps the first MIDI-trigger path backend-only: parsed `ReactiveMidiEvent` values can execute the first matching action in document order and update the same last-execution status used by numbered slots. A later control-surface or MIDI-port adapter should feed live controller events into this API without doing parser, filesystem, or action-planning work on a realtime path.

Phase 4c adds `ReactiveMidiEvent::from_midi_bytes(...)` as the reusable controller-byte bridge for that backend path. It accepts only 3-byte note-on and control-change messages, converts raw MIDI status channels to the action-file's 1-based `ch=` convention, and ignores note-off, zero-velocity note-on, short, null, and unsupported messages. This keeps low-level MIDI parsing out of the future live adapter and preserves a clear boundary before integrating with Ardour's control-surface or MIDI-port callbacks.

Phase 4d adds `ReactiveActionSlotRunner::execute_midi_bytes(...)`, which is the first runner-level raw-byte entry point. It preserves the existing missing-document error, maps supported byte messages through `ReactiveMidiEvent::from_midi_bytes(...)`, delegates supported messages to `execute_midi_event(...)`, and records unsupported byte messages in the same execution-status model used by slots and parsed MIDI events. A later live adapter should call this entry point rather than duplicating parse, match, execution, or status logic inside a control-surface callback.

Phase 4e reviewed the live MIDI adapter hook. Ardour's Generic MIDI surface already supports fixed controller-to-action mappings: `GenericMidiControlProtocol::create_action()` builds `MIDIAction`, `MIDIInvokable::bind_midi()` subscribes to parsed note/CC/program/sysex parser signals, and `MIDIAction::execute()` dispatches the configured Ardour action string through the surface. This is enough for `share/midi_maps/reactive-performance-mvp.map`, where controller notes trigger stable `Reactive/trigger-action-N` actions. It is not enough for action-document `TRIGGER midi ...` matching because `MIDIAction::execute()` receives only the mapped action name, not the original controller bytes. The next low-risk implementation issue should add the UI-side Reactive Performance entry point plus a Reactive-specific Generic MIDI invokable, or equivalent narrow adapter in the Generic MIDI surface, that subscribes only to note-on and control-change events, reconstructs the original 3-byte message on Ardour's existing MidiUI/control-surface thread, and delegates to `ReactiveActionSlotRunner::execute_midi_bytes(...)`. Do not put parser, file loading, action planning, or session mutation into a realtime audio callback, and do not replace existing Generic MIDI action mappings.

Phase 4f implements that live Generic MIDI hook in the narrow form recommended above. `reactive="trigger"` bindings now coexist with ordinary `action`, `function`, and `uri` bindings in Generic MIDI maps, but only for note-on and control-change messages. The surface reconstructs the matched 3-byte message, emits it through `BasicUI`, and the GTK-side Reactive Performance handler delegates to `ReactiveActionSlotRunner::execute_midi_bytes(...)`. The MVP map adds note 46 on channel 10 and CC 22 on channel 1 as document-level trigger examples while preserving the fixed slot/status/reload bindings.

Phase 4g uses that preserved MIDI event value for dynamic macro commands. The parser accepts `DO macro <name> midi-value [ramp <bbt-offset>]`, the action engine resolves it only during event-triggered planning, and the runner updates macro state plus target dispatch with the normalized CC value or note velocity. This keeps controller-to-macro behavior in the reactive action layer without adding native Ardour parameter routing yet; mapping macros onto concrete controls remains a later routing/design issue.

Phase 3k closes the MVP's explicit morph requirement inside the existing action-engine boundary. `DO macro morph <from-snapshot> <to-snapshot> [amount <0..1|midi-value>] [ramp <bbt-offset>]` interpolates shared macro values between two stored snapshots, expands them into ordinary macro commands, and preserves ramp metadata so existing executor, UI, and feedback paths do not need a new target API. Missing snapshots or snapshots with no shared macro names fail the action plan before live macro state is committed. This proves musician-facing macro morphs without touching native automation lanes or plugin-parameter routing yet.

Phase 4h connects transport-gated actions to live Ardour state without moving session ownership into the pure action engine. `ReactiveActionSlotRunner` now accepts a transport-state provider and refreshes it before manual-slot previews, MIDI-event previews, manual-slot execution, and MIDI-event/MIDI-byte execution. `ARDOUR_UI` installs a provider backed by `Session::transport_state_rolling()`, so `WHEN transport rolling` and `WHEN transport stopped` follow Ardour's transport state machine for both Cue-page/UI paths and Generic MIDI-triggered document actions. The provider stays at the GTK/session bridge boundary so Generic MIDI still sends compact trigger bytes through `BasicUI`, and the realtime audio path remains free of document parsing, action planning, and state-provider calls.

Phase 4i adds the backend marker-trigger bridge without subscribing to live session marker crossings yet. `ReactiveMarkerEvent` carries an exact marker name, `ReactiveActionEngine::match_marker_event(...)` returns matching `TRIGGER marker <name>` actions in document order, and `ReactiveActionSlotRunner` can preview, execute, or quantize/queue those actions through the same status and scheduler paths as manual slots and MIDI triggers. The future Session/Location adapter should detect marker crossings from a safe session/UI context and call this backend API; it should not scan markers, parse documents, or mutate session state from a realtime audio callback.

Phase 4j implements that Session/Location adapter in the smallest non-realtime form. `ReactiveMarkerCrossingDetector` is a pure libardour helper that reports visible named marker crossings only when transport advances forward, resetting on stop, locate, explicit discontinuity, or backward movement. `ARDOUR_UI` scans session locations from the existing Reactive Performance panel/queue poll path, filters out ordinary markers without matching reactive actions, and delegates matched crossings to `ReactiveActionSlotRunner::execute_or_queue_marker_event(...)` with the current `TempoMap`. This proves live marker-triggered reactive actions while keeping marker scans, document lookup, and action planning out of realtime callbacks; sample-accurate marker scheduling through native `SessionEvent` remains a later design.

Phase 4k adds the backend scene-trigger bridge before wiring live Cue row, mixer-scene, or TriggerBox adapters. `ReactiveSceneEvent` carries a numbered scene index, the parser accepts `TRIGGER scene <index>` with non-negative indexes, `ReactiveActionEngine::match_scene_event(...)` returns exact index matches in document order, and `ReactiveActionSlotRunner` can preview, execute, or quantize/queue those actions through the same status and scheduler paths as manual slots, MIDI triggers, and marker triggers. Live adapters should be designed separately from safe GTK/session or control-surface contexts and should not parse documents, plan actions, or mutate session state from a realtime audio callback.

Phase 4l adds the backend region-trigger bridge without wiring live Region/Playlist/editor adapters yet. `ReactiveRegionEvent` carries an exact region name, the parser accepts `TRIGGER region <name>` while preserving multi-word names, `ReactiveActionEngine::match_region_event(...)` returns exact name matches in document order, and `ReactiveActionSlotRunner` can preview, execute, or quantize/queue those actions through the same status and scheduler paths as manual slots, MIDI triggers, marker triggers, and scene triggers. The live adapter should be designed separately from a safe GTK/session or editor context and should not parse documents, plan actions, or mutate session state from a realtime audio callback.

Phase 4m implements that live Region/Playlist adapter in the smallest non-realtime form. `ReactiveRegionCrossingDetector` is a pure libardour helper that reports named region-start crossings only when transport advances forward, preserving route name/order metadata and resetting on stop, locate, explicit discontinuity, or backward movement. `ARDOUR_UI` scans active track playlists from the existing Reactive Performance panel/queue poll path, filters out unnamed, hidden, or unmatched regions, and delegates matched crossings to `ReactiveActionSlotRunner::execute_or_queue_region_event(...)` with the current `TempoMap`. This proves live region-triggered reactive actions while keeping playlist scans, document lookup, and action planning out of realtime callbacks; sample-accurate region scheduling through native `SessionEvent` remains a later design.

Phase 4n implements the first live scene adapter in the smallest non-realtime form. `ARDOUR_UI::trigger_cue_row(...)` and the Cue-page row-click path preview `ReactiveSceneEvent::numbered(row)`, skip unmatched rows quietly, and delegate matched rows to `ReactiveActionSlotRunner::execute_or_queue_scene_event(...)` with the current `TempoMap` before the normal Cue row launch continues. The adapter deliberately leaves `Session::trigger_cue_row(...)` unchanged so reactive `DO cue` commands do not recursively trigger scene actions. Lower-level BasicUI/control-surface cue observation, mixer-scene adapters, TriggerBox signals, and sample-accurate native scheduling remain later design work.

Phase 5a starts the performance-panel work with a backend read model instead of a broad UI redesign. `ReactiveActionSlotRunner` now exposes action-bank rows for the first controller bank, including slot index, action name, primary trigger label, command count, and whether the row is the latest attempted slot. The existing status dialog uses that model to show a compact action bank, while a later Cue-page panel can reuse the same data alongside macro/state/harmony values and queued-action preview.

Phase 5b adds the next panel read model for macro banks. `ReactiveActionSlotRunner` now exposes bounded macro rows with slot index, macro name, and current value. The rows are discovered from loaded action documents in first-seen document order with duplicate macro commands collapsed, and values reflect the current action-engine macro state after execution. The existing status dialog shows this macro bank beneath the action bank. A later Cue-page panel can reuse both models before adding state rows, controller feedback, and quantized next-action preview.

Phase 5c adds the matching panel read model for user-defined performance states. `ReactiveActionSlotRunner` now exposes bounded state rows with slot index, state name, and current value. The rows are discovered from loaded action documents in first-seen document order with duplicate state commands collapsed, and values reflect the current action-engine state after execution. The existing status dialog shows this state bank beneath the macro bank.

Phase 5d adds the first quantized next-action preview read model. `ReactiveActionEngine::preview_action(...)` reads the current action plan without executing target commands, mutating macro/state/harmony values, marking last action, or advancing sequential chains. `ReactiveActionSlotRunner` exposes manual-slot and MIDI-event previews with slot index, action name, primary trigger, chain mode, quantize label, and command count, and refreshes the cached status-dialog preview after slot/MIDI execution. A later Cue-page panel can reuse action, macro, state, harmony, and preview rows before adding controller feedback, visible routing, and true queued-action scheduling.

Phase 5e adds the first visible routing read model. `ReactiveSessionTarget` now summarizes controller-facing routes in the same `Session::get_remote_nth_route(...)` order used by `DO rhythm route <route-index> ...`, including route index, route name, whether `Reactive Rhythm State MVP` is inserted, and a compact status label. The existing status dialog displays this section below the next-action preview. A later Cue-page panel can reuse the routing rows with action, macro, state, and preview rows before adding controller feedback and true queued-action scheduling.

Phase 5r extends the routing read model with live rhythm values. When `Reactive Rhythm State MVP` is present, `ReactiveSessionTarget` reads its existing LuaProc automation controls and exposes normalized density/chance plus priority and rotation on `ReactiveRoutingSlotSummary`. The formatted status now includes those values, so the status dialog and Cue-page panel can show controller-driven rhythm changes without parsing files, mutating the session, or adding another UI ownership path.

Phase 5f adds explicit mode arming at the `ReactiveActionSlotRunner` boundary. This keeps disabled-mode behavior independent of GTK, Generic MIDI, and session routing: action documents can still be loaded/reloaded and inspected, but slot/MIDI execution returns a visible disabled result before target dispatch or action-engine mutation. The Generic MIDI MVP map binds note 47 to `Reactive/toggle-performance-mode` so the performer can arm or disarm the system from a controller after setup.

Phase 5g turns the status dialog from a read-only diagnostic into the first performance-safe control surface. `ReactiveActionSlotRunner` now exposes bounded control rows for the first controller-facing slots with action identity, primary trigger, availability, enabled state, and button labels. The existing status dialog renders those rows as eight large trigger controls plus an Enable/Disable Mode control, using the same slot execution and mode-toggle paths as controller actions and refreshing all read models after each interaction. A dedicated Cue-page panel and controller feedback remain follow-up work.

Phase 5h adds that dedicated Cue-page panel as a thin GTK scaffold instead of moving reactive engine ownership. `ReactivePerformancePanel` is hosted above the trigger strip grid, exposes eight large slot buttons, and calls the same `ARDOUR_UI` slot, mode-toggle, reload, and status-dialog methods that already back Generic MIDI and action bindings. This keeps normal Cue behavior unchanged while establishing the eventual panel location. Live read-model rendering and controller feedback remain follow-up work.

Phase 5i binds the Cue-page panel to the existing control summary without moving ownership of the reactive engine. `ARDOUR_UI::reactive_performance_control_summary(...)` is a narrow public accessor over `ReactiveActionSlotRunner::performance_control_summary(...)`, and the panel uses it to refresh slot labels and sensitivity after local panel interactions. Empty slots and disarmed mode are visibly disabled, while Mode, Reload, and Status remain available.

Phase 5j adds controller-driven panel refresh feedback. `ReactiveActionSlotRunner::performance_control_summary(...)` marks the latest attempted slot with a `> ` label prefix after manual or MIDI-triggered execution, `ARDOUR_UI::ReactivePerformanceChanged` is emitted after Reactive slot execution, MIDI-byte execution, mode toggles, and document reloads, and `ReactivePerformancePanel` observes that signal on the GUI context so external controller-triggered actions refresh the Cue-page controls.

Phase 5j also moves the first compact live read-model summary into the Cue-page panel. `ReactiveActionSlotRunner::format_panel_summary(...)` formats next action preview, macro bank, user state bank, and harmony bank into a bounded performer-facing string, while `ARDOUR_UI::reactive_performance_panel_summary(...)` appends the existing `ReactiveSessionTarget` routing summary when a session is loaded. The modal status dialog remains the detailed/debug view; the panel now carries the always-visible macro/state/harmony/preview/routing signal required for performance use.

Phase 5k adds the first controller-feedback read model. `ReactiveActionSlotRunner::controller_feedback_summary(...)` exposes bounded slot rows with action identity, availability, enabled state, latest-attempted state, and deterministic feedback values: `0` for unavailable or disabled, `32` for enabled idle, and `127` for the latest attempted enabled slot.

Phase 5l maps those controller-feedback rows to raw MIDI output messages without yet changing the Generic MIDI realtime feedback loop. `ReactiveControllerFeedbackBinding` describes note or CC output bindings using musician-facing channels `1` through `16`, and `ReactiveActionSlotRunner::controller_feedback_midi_messages(...)` emits deterministic three-byte note/CC messages, including zero-valued messages for mapped empty slots so controller LEDs can be cleared. Actual Generic MIDI output-port wiring remains follow-up work.

Phase 5m declares those feedback output bindings in the bundled Generic MIDI map. `reactive-performance-mvp.map` now has separate `reactive="feedback"` rows for slots `0` through `7` on the same channel-10 pad notes as the trigger actions, and `GenericMidiControlProtocol` validates and stores those rows as `ReactiveControllerFeedbackBinding` values. Actual output-port writes remain follow-up work so the realtime feedback loop is not changed until the cached-message path is designed.

Phase 5n implements that cached-message path. Generic MIDI emits parsed feedback bindings through `BasicUI`, `ARDOUR_UI` computes feedback bytes with the GTK-owned `ReactiveActionSlotRunner` whenever Reactive Performance state changes, and Generic MIDI stores those bytes in `ReactiveControllerFeedbackMidiCache`. The existing feedback tick writes only cached byte vectors with a try-lock, so the realtime feedback path does not parse documents, plan actions, allocate runner data, or touch filesystem-backed state.

Phase 5o adds the first backend queued-action scheduler without yet touching Ardour's realtime event system. `ReactiveActionScheduler` stores already-planned action commands with slot, action, trigger, quantize, requested BBT, and due BBT metadata; exposes bounded queued-action summary rows for UI/status surfaces, including command counts and command details; and releases due actions deterministically by due BBT and queue id. The scheduler accepts explicit due BBT values instead of calculating them itself, because correct BBT arithmetic depends on the session `TempoMap`. The later integration should let the GTK/session bridge compute due times and pop actions from a safe non-realtime context before considering native `SessionEvent` work.

Phase 5p connects that scheduler to `ReactiveActionSlotRunner` while preserving the non-realtime boundary. The runner can now plan manual-slot and MIDI-triggered actions through `execute_or_queue_slot(...)` / `execute_or_queue_midi_event(...)`; zero-quantize plans still execute immediately, while nonzero-quantize plans are queued with caller-supplied requested/due BBT values and released only when `release_due_queued_actions(...)` is called. Queued MIDI actions preserve event-derived values such as `midi-value` macros and TriggerBox follow probabilities, and queued-action summaries are available for Cue-page/status display. Disabling Reactive Performance Mode or replacing the action document clears pending queued work so disarm/reload behavior stays performance-safe. The next bridge should compute due BBT from Ardour's `TempoMap` and poll/release due actions from a safe GTK/session context before any native `SessionEvent` work.

Phase 5q adds that first non-realtime session-clock bridge. `ReactiveActionClock` computes requested/due BBT values from Ardour's `TempoMap`, with focused coverage for immediate, bar, beat, and sample-derived request positions. `ReactiveActionSlotRunner` exposes TempoMap-backed queue-aware overloads for manual slots, MIDI events, and MIDI byte messages, so UI and controller callers can plan an action once and let the runner derive due timing from the resulting quantize value. `ARDOUR_UI` now routes Cue-page/status actions and Generic MIDI trigger bytes through those overloads, the Reactive Performance panel polls queued actions from GTK space every 100 ms, and both the panel and status dialog show queued-action summaries with bounded command details. This intentionally stays outside realtime callbacks and avoids a native `SessionEvent` type; the remaining risk is polling precision versus eventual sample-accurate scheduling.

Phase 5s answers the queued-action feedback question by keeping it in the existing non-realtime read model. `ReactiveControllerFeedbackSummary` now marks slots with pending queued actions, and `ReactiveActionSlotRunner::controller_feedback_midi_messages(...)` emits a deterministic queued value of `96` between idle `32` and latest-attempted `127`. Latest-attempted still wins for the just-pressed slot, while queued feedback clears through the existing release, disable, clear, and reload queue lifecycle. This lets a controller show "armed for the next boundary" without adding hardware-specific behavior or any realtime action planning.

Phase 5t makes the queued-action read model more performer-readable without changing scheduling. `ReactiveQueuedActionSummary` now carries a bounded list of command summaries copied from the already-planned queued commands, so status and Cue-page surfaces can show details such as the cue row, route-scoped rhythm value, TriggerBox follow probability, or resolved CC-derived macro morph amount that will fire at the quantized boundary. The summaries are display-only and stay outside realtime callbacks.

Phase 3j adds a dedicated harmony state lane to the action engine. `DO harmony <name> <value>` updates key/chord/scale-style values in a separate read model from general `DO state`, `WHEN harmony <name> <value>` can gate future actions, and `ReactiveActionSlotRunner` exposes a bounded harmony-bank summary for the status dialog and Cue-page panel. The target command is intentionally non-session-mutating for now: it proves harmony as reactive state before adding chord generation, MIDI clip mutation, or transport-scheduled harmonic processors.

Phase 7e chooses a repeatable demo-session template script before committing generated session XML. `share/scripts/reactive_performance_mvp_session.lua` is a bundled `SessionInit` script named `Reactive Performance MVP`; Ardour lists it as a factory template in the new-session flow and runs it after creating the empty session. The script creates three MIDI-only controller-facing routes for rhythm, harmony, and macro lanes, then saves the session. This advances the demo-session deliverable without hand-authoring brittle `.ardour` XML IDs, ports, and environment-dependent connections. A complete session archive with richer generated musical cue content remains a later packaging step once the route and cue layout is stable enough to verify.

Phase 7g extends that template to install the demo action document automatically. The SessionInit script writes `reactive-actions.txt` into the new session folder only when file I/O is available and the file is absent, preserving user-edited or pre-existing session-local documents. The script embeds the same action content as `examples/reactive-performance-mvp/reactive-actions.txt`; `LuaScriptTest::reactive_performance_session_init_installs_demo_action_document_test` executes the actual Lua template against a fake session and compares the generated file to the packaged example so future drift is caught in automation. `LuaScriptTest::reactive_performance_session_init_tolerates_unavailable_file_io_test` covers hardened Lua settings that remove file I/O, where the template still creates the route layout and users can copy the packaged action file manually. This keeps the setup path musician-friendly without committing a full generated session archive yet.

Phase 7h makes those template lanes Cue-page visible by passing `trigger_visibility=true` to `Session::new_midi_track(...)` for the rhythm, harmony, and macro lanes. This follows the same Ardour creation flag used by Trigger Page import paths for trigger-facing MIDI tracks while keeping the SessionInit script focused on route layout and action-document installation. The LuaScript test harness now records the trigger-visibility argument for each fake `new_midi_track(...)` call and verifies all three demo lanes request it. At this phase, generated musical note content and a full `.ardour` session archive remained deferred because those assets required stable ID-heavy session XML and a verifiable route/clip layout.

Phase 7i adds live marker coverage to the repeatable demo asset now that the marker bridge exists. The packaged action document and SessionInit-embedded copy include `demo.marker.breakdown`, triggered by a visible session marker named `Breakdown`; it reuses existing rhythm insert/route, macro, harmony, state, and cue commands so it adds demo coverage without changing session ownership or realtime behavior. `ReactiveActionDocumentLoaderTest::packagedDemoSessionActionFileLoads` now requires that marker preview to be available, while the SessionInit install test continues to compare the generated action file against the packaged example.

Phase 7j adds live region coverage to the repeatable demo asset now that the region bridge exists. The packaged action document and SessionInit-embedded copy include `demo.region.breakdown.loop`, triggered by a non-hidden timeline region named `Breakdown Loop`; it reuses existing rhythm insert/route, macro, harmony, state, and cue commands so it adds demo coverage without changing session ownership or realtime behavior. `ReactiveActionDocumentLoaderTest::packagedDemoSessionActionFileLoads` now requires that region preview to be available, while the SessionInit install test continues to compare the generated action file against the packaged example.

Phase 7k adds live scene coverage to the repeatable demo asset now that the Cue-page scene bridge exists. The packaged action document and SessionInit-embedded copy include `demo.scene.drop`, triggered by Cue row 3 through `TRIGGER scene 3`; it reuses existing rhythm insert/route, macro, harmony, and state commands while leaving the ordinary Cue-page launch responsible for row playback. `ReactiveActionDocumentLoaderTest::packagedDemoSessionActionFileLoads` now requires that scene preview to be available, while the SessionInit install test continues to compare the generated action file against the packaged example.

Phase 7l adds the first generated timeline landmark without committing session XML. `ARDOUR.LuaAPI.ensure_session_marker(...)` gives non-realtime SessionInit scripts an idempotent way to create a visible named marker while keeping `Location` allocation in C++; the Reactive Performance MVP template uses it to seed `Breakdown` for the packaged marker-trigger action. `LuaScriptTest` now covers both the helper against a real test session and the template's marker request in the fake-session harness. Richer generated musical cue content and a full `.ardour` session archive remain deferred until that layout is stable enough to verify.

Phase 7m adds the first generated region-trigger landmark without committing session XML. `ARDOUR.LuaAPI.ensure_session_midi_region(...)` gives non-realtime SessionInit scripts an idempotent way to create a visible named MIDI region on a named MIDI route playlist while keeping source, region, and playlist ownership in C++; the Reactive Performance MVP template uses it to seed `Breakdown Loop` on `Reactive Rhythm Lane` for the packaged region-trigger action. `LuaScriptTest` now covers both the helper against a real test session and the template's region request in the fake-session harness. This remains a timeline landmark rather than generated musical note content, so a full `.ardour` session archive remains deferred until the layout is stable enough to verify.

Phase 7n adds the first generated Cue-page trigger clip placeholders without committing session XML. `ARDOUR.LuaAPI.ensure_session_midi_trigger_region(...)` gives non-realtime SessionInit scripts an idempotent way to create a named MIDI region inside a named MIDI route's TriggerBox slot while keeping source, region, and TriggerBox ownership in C++; the Reactive Performance MVP template uses it to seed three rhythm placeholders, one harmony placeholder, and two macro placeholders. A narrow `TriggerBox::set_region_for_setup(...)` path keeps this deterministic during setup while updating active-slot bookkeeping and UI-facing signals that direct `Trigger::set_region(...)` would bypass. `LuaScriptTest` covers both the real helper against an Ardour test session and the template's six trigger-region requests in the fake-session harness. Phase 7o turns those named launchable landmarks into first single-note demo clips.

Phase 7o adds first-note content to the repeatable Cue-page trigger clips without committing session XML. `ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note(...)` creates a named MIDI TriggerBox region and inserts one MIDI note only when the target slot is empty, so existing performer content is left untouched. The Reactive Performance MVP template seeds notes 36, 38, 41, 48, 60, and 67 into the six named trigger clips. `LuaScriptTest` covers both the real helper against an Ardour test session and the template's six trigger-note requests in the fake-session harness. This makes the demo clips no longer empty placeholders while keeping a polished generated arrangement and full `.ardour` session archive as later packaging work.

Phase 7p adds chord-shaped content to the repeatable Cue-page trigger clips without changing live performance paths. `ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_notes(...)` accepts a Lua table of note specs, validates all note timing/channel/note/velocity fields before creating a region, and leaves already-populated TriggerBox slots untouched. The Reactive Performance MVP template keeps the rhythm clips as single-note triggers and seeds the harmony/macro clips as small triads, giving the demo 12 total notes across the six named clips while still avoiding generated session XML.

### 5. Use SessionEvent carefully

`SessionEvent` can schedule transport and realtime operations. It should be considered for later native quantized action execution, but the MVP should avoid adding a new realtime event type until tests prove the engine's allocation and locking behavior. Phase 5o keeps queued actions in a non-realtime scheduler with explicit due BBTs, and Phase 5p lets the runner enqueue/release those plans from an explicit caller path. The next bridge should compute due times from `TempoMap` and poll/release due actions from a safe UI/session context before any realtime `SessionEvent` design.

## C++ vs Lua vs Plugin Boundary

Native C++ should own:

- Reactive state snapshot model.
- Declarative action parser and validation.
- Named action registry.
- Action chain execution.
- Quantization calculations against `Temporal::TempoMap`.
- Integration with `Session`, `TriggerBox`, `Route`, mixer scenes, and automation controls.
- UI-facing read model for active state, last executed action, and queued next actions.

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
- Which queued-feedback LED values should be tuned for the first physical controller once hardware smoke testing begins?
