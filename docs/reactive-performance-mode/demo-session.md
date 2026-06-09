# Reactive Performance MVP Demo Guide

Issue: #63.

This guide sets up the current Reactive Performance Mode vertical slice: eight Generic MIDI pad actions, session-local action loading, the status panel, and the Reactive Rhythm State MVP LuaProc insertion path.

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

Two utility buttons are also mapped:

```text
note 44 -> Reactive/show-action-document-status
note 45 -> Reactive/reload-action-document
```

Use note 44 to inspect the currently loaded action document, latest slot execution, and first eight action-bank rows. Use note 45 after editing `reactive-actions.txt`.

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
```

Reload with `Reactive/reload-action-document` or the Reload button in `Reactive/show-action-document-status`.

## Smoke Test

Run this checklist after loading the session:

- App boots from `gtk2_ardour/ardev`.
- Utility note 44 opens `Reactive/show-action-document-status` with either the session file path or `built-in MVP fallback`.
- Trigger pad 0, MIDI channel 10 note 36.
- The status panel reports slot 0, a successful last execution, and marks the corresponding action-bank row.
- Route 0 contains an active LuaProc processor named `Reactive Rhythm State MVP`.
- Cue row 0 launches if the row exists in the session.
- Trigger pad 1 or 2 and confirm the status panel reports the matching slot/action.
- If using the session-local file, route 0 rhythm controls change without changing rhythm inserts on other routes.
- Utility note 45 triggers `Reactive/reload-action-document` and reports parse errors instead of silently falling back when the session file is invalid.

## Current Limits

- The MVP map binds eight pad notes and two utility notes. Macro CCs and MIDI feedback output remain follow-up work.
- The status panel is a compact diagnostic dialog with the first reusable action-bank read model, not the final Cue-page performance panel.
- The session must be created manually; this repo does not yet package an Ardour demo session archive.
- Route-scoped rhythm actions require the target route to already contain `Reactive Rhythm State MVP`, except for the explicit `DO rhythm insert <route-index>` setup command.
