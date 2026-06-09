# Reactive Performance MVP Example

Issue: #109.

This folder is a small demo asset for Reactive Performance Mode. It is not a full Ardour session archive yet; it is a session-local action document plus setup notes for the bundled `Reactive Performance MVP` session-template script.

## Files

- `reactive-actions.txt`: a valid session-local Reactive Performance action document.
- `share/scripts/reactive_performance_mvp_session.lua`: the bundled new-session template script that creates the demo's MIDI route layout.

## Setup

1. Create a new Ardour session from the `Reactive Performance MVP` factory template. The bundled SessionInit script creates three MIDI-only routes named `Reactive Rhythm Lane`, `Reactive Harmony Lane`, and `Reactive Macro Lane`.
2. Copy `reactive-actions.txt` into the session folder, next to the `.ardour` session file.
3. Enable the Generic MIDI control surface and select the `Reactive Performance MVP` MIDI map.
4. Connect a controller or MIDI monitor to Ardour's `Generic MIDI Control Out` port if you want to inspect feedback messages.
5. Use `Reactive/reload-action-document` or note 45 in the MVP map after copying or editing the file.

You can still use an existing session manually: add at least one MIDI route in controller-facing route slot 0 before running the smoke check.

## Controller Map

The bundled MVP map uses:

- Channel 10 notes 36-43 for the first eight action slots in this file.
- Channel 10 note 46 as a live document-level trigger for `demo.cue.when.rolling`.
- Channel 1 CC 22 as a live document-level trigger for `demo.filter.sweep`.
- Channel 10 note 44 for status.
- Channel 10 note 45 for reload.
- Channel 10 note 47 for mode enable/disable.

## Smoke Check

1. Open `Reactive/show-action-document-status`.
2. Confirm the action document source is `session` and that nine actions are loaded.
3. Trigger note 36 and confirm route 0 receives `Reactive Rhythm State MVP`, resets rhythm parameters, and launches cue row 0 if it exists.
4. Trigger notes 37, 38, and 39 to change route 0 rhythm density, chance, priority, or rotation.
5. While transport is stopped, trigger note 40 and confirm transport starts; while transport is rolling, trigger note 41 or live trigger note 46 and confirm cue row 4 launches if it exists.
6. While transport is rolling, trigger note 42 and confirm transport stops; while stopped, trigger note 43 and confirm the state bank reports `transport = stopped-ready`.
7. Move CC 22 on channel 1 and confirm the macro bank shows `filter` changing between `0.0` and `1.0`.
8. If `Generic MIDI Control Out` is connected, confirm notes 36-43 reflect idle/latest/disabled feedback values.

## Current Limit

This asset deliberately avoids a full `.ardour` session file because Ardour session XML contains many generated IDs, ports, and environment-dependent connections. The bundled SessionInit template is the first repeatable demo-session path; packaging a complete session archive remains a later demo-production step once the route/clip layout is stable enough to verify.
