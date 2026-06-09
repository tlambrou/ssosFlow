# Reactive Performance MVP Demo Guide

Issue: #63.

This guide sets up the current Reactive Performance Mode vertical slice: eight Generic MIDI pad actions, session-local action loading, action/macro/state status read models, status-panel trigger controls, and the Reactive Rhythm State MVP LuaProc insertion path.

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

Create or open a small session with the Cue page available.

Recommended minimal layout:

- Route 0: one MIDI track that receives controller or clip MIDI and can host `Reactive Rhythm State MVP`.
- Optional routes 1-3: additional MIDI or instrument tracks for cue-row contrast.
- Cue rows 0-7: simple clips or empty cue rows are enough for action-path validation; audible MIDI clips make rhythm changes easier to hear.

Route numbers use Ardour's controller-facing remote route order. The current action commands resolve route `0` the same way for:

```text
DO rhythm insert 0
DO rhythm route 0 density 0.5
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

Use note 44 to inspect whether Reactive Performance Mode is enabled, the currently loaded action document, latest slot execution, first eight performance controls, first eight action-bank rows, first eight macro-bank rows, and first eight state-bank rows. The status panel also includes eight large slot-trigger buttons and an Enable/Disable Mode button for controller-parity checks during setup. The Cue page also shows the dedicated Reactive Performance panel above the trigger strip grid; its eight slot buttons reflect loaded action names and disable themselves when a slot is unavailable or Reactive Performance Mode is disarmed. Use note 45 after editing `reactive-actions.txt`. Use note 47 to arm or disarm Reactive Performance Mode from the controller; when disabled, performance actions report a disabled status and do not launch cues, mutate rhythm parameters, or update macro/state values.

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

For an explicit route-scoped demo, save this file as:

```text
<session-folder>/reactive-actions.txt
```

```text
ACTION demo.reset
TRIGGER midi note ch=10 note=36
DO rhythm insert 0
DO rhythm route 0 density 1
DO rhythm route 0 chance 1
DO rhythm route 0 priority_mode 0
DO rhythm route 0 rotation 0
DO cue 0
END

ACTION demo.tighten
TRIGGER midi note ch=10 note=37
DO rhythm route 0 density 0.75
DO rhythm route 0 chance 1
DO rhythm route 0 priority_mode 3
DO cue 1
END

ACTION demo.sparse
TRIGGER midi note ch=10 note=38
DO rhythm route 0 density 0.35
DO rhythm route 0 chance 0.5
DO rhythm route 0 priority_mode 1
DO cue 2
END

ACTION demo.rotate
TRIGGER midi note ch=10 note=39
DO rhythm route 0 rotation 4
DO cue 3
END

ACTION demo.filter.sweep
TRIGGER midi cc ch=1 cc=22
DO macro filter midi-value ramp 0|1|0
END
```

Reload with `Reactive/reload-action-document` or the Reload button in `Reactive/show-action-document-status`.

## Smoke Test

Run this checklist after loading the session:

- App boots from `gtk2_ardour/ardev`.
- The Cue page shows a Reactive Performance panel above the trigger strip grid, with slot labels sourced from the loaded action document.
- Empty Cue-page panel slots are disabled, and disarming Reactive Performance Mode disables slot buttons while leaving Mode, Reload, and Status available.
- Utility note 44 opens `Reactive/show-action-document-status` with either the session file path or `built-in MVP fallback`.
- The status panel shows eight slot-trigger buttons, visibly disables empty/unavailable controls, and has an Enable/Disable Mode button.
- Utility note 47 or the panel mode button toggles the status between `Reactive Performance Mode: enabled` and `Reactive Performance Mode: disabled`.
- While disabled, trigger pad 0 and confirm the status panel reports a disabled execution without inserting `Reactive Rhythm State MVP` or launching a cue.
- Toggle note 47 again to re-enable Reactive Performance Mode.
- Trigger pad 0, MIDI channel 10 note 36.
- The status panel reports slot 0, a successful last execution, shows the refreshed next-action preview, shows route 0 with `Reactive Rhythm State MVP`, marks the corresponding action-bank row, and shows macro/state values from the loaded action document.
- Route 0 contains an active LuaProc processor named `Reactive Rhythm State MVP`.
- Cue row 0 launches if the row exists in the session.
- Trigger pad 1 or 2, or press the matching Cue-page/status-panel slot button, and confirm the status panel reports the matching slot/action.
- Move CC 22 on MIDI channel 1 and confirm the macro bank shows `filter` tracking the controller value from `0.0` to `1.0`.
- If using the session-local file, route 0 rhythm controls change without changing rhythm inserts on other routes.
- Utility note 45 triggers `Reactive/reload-action-document` and reports parse errors instead of silently falling back when the session file is invalid.

## Current Limits

- The MVP map binds eight pad notes, three utility notes, and live document-level note/CC trigger examples. MIDI feedback output and native macro-to-plugin parameter routing remain follow-up work.
- The Cue-page panel refreshes after its own slot, Mode, Reload, and Status interactions. Automatic refresh after external MIDI/controller actions, controller feedback, and richer layout remain follow-up work.
- The status panel is a compact diagnostic dialog with first reusable control, action-bank, macro-bank, state-bank, next-action preview, and routing read models.
- The session must be created manually; this repo does not yet package an Ardour demo session archive.
- Route-scoped rhythm actions require the target route to already contain `Reactive Rhythm State MVP`, except for the explicit `DO rhythm insert <route-index>` setup command.
