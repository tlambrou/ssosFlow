# Reactive Performance MVP Demo Guide

Issue: #63, #123, #154, #162.

This guide sets up the current Reactive Performance Mode vertical slice: the bundled demo-session template, eight Generic MIDI pad actions, session-local action loading, marker-trigger and region-trigger demo actions, action/macro/state/harmony status read models, status-panel trigger controls, cached Generic MIDI controller feedback, and the Reactive Rhythm State MVP LuaProc insertion path.

## Build And Run

Build the app and the focused reactive test targets:

```bash
python3 ./waf build --targets=unit-test-reactive_action,unit-test-reactive_action_document_loader,unit-test-reactive_action_executor,unit-test-reactive_action_slot_runner,unit-test-reactive_session_target,unit-test-reactive_session_target_rhythm_insert,libardour-tests,ardour-9.7.37
```

Run the development build:

```bash
gtk2_ardour/ardev
```

If the app was not configured yet, use the configure command in `docs/reactive-performance-mode/build-notes.md` first.

## Demo Session Layout

Recommended path: create a new Ardour session from the bundled `Reactive Performance MVP` factory template. The template is provided by `share/scripts/reactive_performance_mvp_session.lua` and creates three MIDI-only controller-facing routes that are visible on the Cue page:

- `Reactive Rhythm Lane`
- `Reactive Harmony Lane`
- `Reactive Macro Lane`

The template also installs the demo `reactive-actions.txt` next to the new `.ardour` session file when Lua file I/O is available. It is the repeatable MVP setup path, but it is not a full `.ardour` session archive with generated clips, cue contents, ports, and environment-specific connections.

Manual fallback: create or open any small session with the Cue page available.

Minimal manual layout:

- Route 0: one MIDI track that receives controller or clip MIDI and can host `Reactive Rhythm State MVP`.
- Optional routes 1-3: additional MIDI or instrument tracks for cue-row contrast.
- Cue rows 0-7: simple clips or empty cue rows are enough for action-path validation; audible MIDI clips make rhythm changes easier to hear.

Route numbers use Ardour's controller-facing remote route order. The current action commands resolve route `0` the same way for:

```text
DO rhythm insert 0
DO rhythm route 0 density 0.5
DO rhythm route 0 density midi-value
```

## Controller Map

Enable the Generic MIDI control surface and select:

```text
Reactive Performance MVP
```

The map lives at:

```text
share/midi_maps/reactive-performance-mvp.map
```

It binds MIDI channel 10 notes 36-43 to:

```text
Reactive/trigger-action-0
Reactive/trigger-action-1
Reactive/trigger-action-2
Reactive/trigger-action-3
Reactive/trigger-action-4
Reactive/trigger-action-5
Reactive/trigger-action-6
Reactive/trigger-action-7
```

Three utility buttons are also mapped:

```text
note 44 -> Reactive/show-action-document-status
note 45 -> Reactive/reload-action-document
note 47 -> Reactive/toggle-performance-mode
```

Use note 44 to inspect whether Reactive Performance Mode is enabled, the currently loaded action document, latest slot execution, first eight performance controls, first eight action-bank rows, first eight macro-bank rows, first eight state-bank rows, and first eight harmony-bank rows. The status panel also includes eight large slot-trigger buttons and an Enable/Disable Mode button for controller-parity checks during setup. The Cue page also shows the dedicated Reactive Performance panel above the trigger strip grid; its eight slot buttons reflect loaded action names and disable themselves when a slot is unavailable or Reactive Performance Mode is disarmed. Use note 45 after editing `reactive-actions.txt`. Use note 47 to arm or disarm Reactive Performance Mode from the controller; when disabled, performance actions report a disabled status and do not launch cues, mutate rhythm parameters, or update macro/state/harmony values.

The map also declares feedback rows for slots 0-7 on channel 10 notes 36-43. When Ardour's Generic MIDI Control Out port is connected to a controller or monitor, the current implementation sends zero for unavailable or disabled slots, a low value for enabled idle slots, and full value for the latest attempted enabled slot.

## Built-In Fallback Behavior

If no configured `reactive-actions.txt` exists, the app loads the built-in MVP fallback. Pad 0 installs and resets the rhythm processor on route 0, then launches cue row 0. Pads 1-7 launch cue rows 1-7 while changing rhythm state.

| Pad | Note | Action | Expected result |
| --- | --- | --- | --- |
| 0 | 36 | `mvp.cue.0` | Insert `Reactive Rhythm State MVP` on route 0, reset density/chance/priority/rotation, launch cue 0 |
| 1 | 37 | `mvp.cue.1` | Density 0.75, chance 1.0, velocity priority, launch cue 1 |
| 2 | 38 | `mvp.cue.2` | Density 0.5, downbeat priority, launch cue 2 |
| 3 | 39 | `mvp.cue.3` | Rotation 4, launch cue 3 |
| 4 | 40 | `mvp.cue.4` | Chance 0.5, launch cue 4 |
| 5 | 41 | `mvp.cue.5` | Density 0.25, rotation 8, launch cue 5 |
| 6 | 42 | `mvp.cue.6` | Chance 0.25, pitch priority, launch cue 6 |
| 7 | 43 | `mvp.cue.7` | Density 1.0, chance 1.0, rotation 0, launch cue 7 |

## Session-Local Action File

For an explicit route-scoped demo in an existing session, use the packaged example asset:

```text
examples/reactive-performance-mvp/
```

Copy its action document into your Ardour session folder:

```text
examples/reactive-performance-mvp/reactive-actions.txt
  ->
<session-folder>/reactive-actions.txt
```

The packaged action file is also covered by `ReactiveActionDocumentLoaderTest::packagedDemoSessionActionFileLoads`, so parser drift fails in automated tests. The `Reactive Performance MVP` factory template writes matching content into new sessions and is covered by `LuaScriptTest::reactive_performance_session_init_installs_demo_action_document_test`, so template/example drift fails in automated tests. Reload with `Reactive/reload-action-document` or the Reload button in `Reactive/show-action-document-status`.

When using the `Reactive Performance MVP` factory template, no manual copy is needed unless Lua file I/O is disabled for template scripts or you want to replace the installed document with an edited version.

The packaged file also includes `demo.marker.breakdown`, a live marker-trigger action bound to a visible session marker named `Breakdown`. It is intentionally a demo action rather than a new controller binding: add the marker to the timeline, locate before it, roll transport across it, and Reactive Performance Mode will queue or execute the action through the same marker bridge used by ordinary session markers.

It also includes `demo.region.breakdown.loop`, a live region-trigger action bound to a non-hidden timeline region named `Breakdown Loop`. Create, record, draw, or import a region with that exact name on a visible track, locate before the region start, roll transport across it, and Reactive Performance Mode will queue or execute the action through the same region bridge used by ordinary active track playlists.

It also includes `demo.scene.drop`, a live scene-trigger action bound to Cue row 3. Launch Cue row 3 from the Cue page or a registered Trigger Page row action, and Reactive Performance Mode will queue or execute the action through the same scene bridge while preserving the normal cue launch. Lower-level BasicUI/control-surface/session observation remains follow-up work.

## Smoke Test

Run this checklist after creating the session and loading the Generic MIDI map. If you use the manual existing-session fallback, copy `reactive-actions.txt` into that session first.

- App boots from `gtk2_ardour/ardev`.
- A template-created session contains Cue-page visible `Reactive Rhythm Lane`, `Reactive Harmony Lane`, and `Reactive Macro Lane`; a manually created session has at least one MIDI route in controller-facing route slot 0.
- The Cue page shows a Reactive Performance panel above the trigger strip grid, with slot labels sourced from the loaded action document.
- Empty Cue-page panel slots are disabled, and disarming Reactive Performance Mode disables slot buttons while leaving Mode, Reload, and Status available.
- Utility note 44 opens `Reactive/show-action-document-status` with either the session file path or `built-in MVP fallback`.
- The status panel shows eight slot-trigger buttons, visibly disables empty/unavailable controls, and has an Enable/Disable Mode button.
- Utility note 47 or the panel mode button toggles the status between `Reactive Performance Mode: enabled` and `Reactive Performance Mode: disabled`.
- While disabled, trigger pad 0 and confirm the status panel reports a disabled execution without inserting `Reactive Rhythm State MVP` or launching a cue.
- Toggle note 47 again to re-enable Reactive Performance Mode.
- Trigger pad 0, MIDI channel 10 note 36.
- The status panel reports slot 0, a successful last execution, shows the refreshed next-action preview, shows route 0 with `Reactive Rhythm State MVP`, marks the corresponding action-bank row, and shows macro/state/harmony values from the loaded action document.
- Route 0 contains an active LuaProc processor named `Reactive Rhythm State MVP`.
- Cue row 0 launches if the row exists in the session.
- Trigger pad 1 or 2, or press the matching Cue-page/status-panel slot button, and confirm the status panel reports the matching slot/action.
- If Generic MIDI Control Out is connected to a controller or MIDI monitor, confirm the matching pad feedback changes after trigger, reload, and mode-toggle actions.
- Move CC 22 on MIDI channel 1 and confirm route 0 rhythm density follows the controller value while the routing summary shows the density value and the macro bank shows `filter` tracking from `0.0` to `1.0`.
- Add a visible session marker named `Breakdown`, locate before it, roll transport across it, and confirm the panel or status dialog reports `demo.marker.breakdown`, `section = breakdown`, `chord = bVII`, `filter = 0.25`, and route 0 rhythm density/chance changes.
- Add or rename a non-hidden timeline region to `Breakdown Loop`, locate before its start, roll transport across it, and confirm the panel or status dialog reports `demo.region.breakdown.loop`, `section = region-breakdown`, `chord = i7`, `filter = 0.55`, and route 0 rhythm density/chance changes.
- Launch Cue row 3 from the Cue page and confirm the panel or status dialog reports `demo.scene.drop`, `section = scene-drop`, `chord = V7`, `filter = 0.70`, and route 0 rhythm density/chance/rotation changes while the normal cue launch still happens.
- If using the session-local file, route 0 rhythm controls change without changing rhythm inserts on other routes.
- Utility note 45 triggers `Reactive/reload-action-document` and reports parse errors instead of silently falling back when the session file is invalid.

## Current Limits

- The MVP map binds eight pad notes, matching pad feedback declarations, three utility notes, and live document-level note/CC trigger examples. libardour can turn Reactive slot feedback into note/CC feedback bytes, and Generic MIDI now writes those cached bytes through its output port when feedback is enabled. CC-derived values can drive route-scoped rhythm parameters; general macro-to-plugin parameter routing remains follow-up work.
- The Cue-page panel refreshes after its own slot, Mode, Reload, Status, and external MIDI/controller-triggered Reactive actions. Controller LED behavior depends on the selected controller, MIDI routing, and feedback mode configuration; richer layout remains follow-up work.
- The status panel is a compact diagnostic dialog with first reusable control, action-bank, macro-bank, state-bank, harmony-bank, next-action preview, and routing read models.
- The repo packages a repeatable `Reactive Performance MVP` SessionInit template plus a session-local action document under `examples/reactive-performance-mvp/`. The template installs the demo action document into new sessions without overwriting an existing `reactive-actions.txt` when Lua file I/O is available, but this repo does not yet package a full `.ardour` demo session archive.
- Live scene triggers currently cover Cue-page row launches and the registered Trigger Page row actions. Generic MIDI fixed slot actions, lower-level BasicUI/control-surface cue calls, and native session observation remain follow-up work.
- Route-scoped rhythm actions require the target route to already contain `Reactive Rhythm State MVP`, except for the explicit `DO rhythm insert <route-index>` setup command.
- Harmony commands are currently a reactive read model only. They expose key/chord/scale state to the panel and conditions, but they do not yet generate chords, alter clips, or route MIDI notes.
