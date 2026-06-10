# Reactive Performance MVP Example

Issue: #109, #154, #162, #166, #170, #184, #185, #188, #190, #192, #194.

This folder is a small demo asset for Reactive Performance Mode. It is not a full Ardour session archive yet; it is the source-controlled action document that the bundled `Reactive Performance MVP` session-template script installs into new demo sessions alongside seeded `Breakdown` marker, `Breakdown Loop` region, and first Cue-page trigger clips with simple rhythm notes plus harmony/macro triads.

## Files

- `reactive-actions.txt`: a valid session-local Reactive Performance action document. The template installs matching content into new demo sessions as `reactive-actions.txt`.
- `share/scripts/reactive_performance_mvp_session.lua`: the bundled new-session template script that creates the demo's Cue-page visible MIDI route layout, seeds the `Breakdown` marker, `Breakdown Loop` timeline region, and first six named MIDI trigger clips, then installs the demo action document.
- `docs/reactive-performance-mode/ui-guide/index.html`: a local static UI walkthrough with screenshot-style panels for running and smoke-testing the demo.

## Setup

1. Create a new Ardour session from the `Reactive Performance MVP` factory template. The bundled SessionInit script creates three Cue-page visible MIDI-only routes named `Reactive Rhythm Lane`, `Reactive Harmony Lane`, and `Reactive Macro Lane`, seeds visible `Breakdown` marker and `Breakdown Loop` region landmarks, fills the first six empty Cue-page trigger slots with named MIDI clips, then installs the demo action document next to the `.ardour` session file.
2. Enable the Generic MIDI control surface and select the `Reactive Performance MVP` MIDI map.
3. Connect a controller or MIDI monitor to Ardour's `Generic MIDI Control Out` port if you want to inspect feedback messages.
4. Use `Reactive/reload-action-document` or note 45 in the MVP map after editing the installed file.

You can still use an existing session manually: add at least one MIDI route in controller-facing route slot 0, then copy this folder's `reactive-actions.txt` into that session folder before running the smoke check. Use the same copy step if your Ardour Lua settings disable file I/O for template scripts.

## Controller Map

The bundled MVP map uses:

- Channel 10 notes 36-43 for the first eight action slots in this file.
- Channel 10 note 46 as a live document-level trigger for `demo.cue.when.rolling`.
- Channel 1 CC 22 as a live document-level trigger for `demo.filter.sweep`, driving route 0 rhythm density, route 0 slot 0 trigger follow probability, the `filter` macro read model, and a macro morph between stored `texture`/`space` snapshots.
- A visible session marker named `Breakdown` as a live document-level trigger for `demo.marker.breakdown`; template-created sessions seed this marker automatically.
- A named timeline region `Breakdown Loop` as a live document-level trigger for `demo.region.breakdown.loop`; template-created sessions seed this region landmark automatically on `Reactive Rhythm Lane`.
- Seeded MIDI trigger clips named `Reactive Cue 0 Reset` note 36, `Reactive Cue 1 Tighten` note 38, `Reactive Cue 2 Sparse` note 41, `Reactive Harmony i` notes 48/51/55, `Reactive Macro Filter` notes 60/64/67, and `Reactive Macro Texture` notes 67/72/74.
- Cue row 3 as a live scene trigger for `demo.scene.drop` when launched from the Cue page or registered Trigger Page action.
- Pads 36-39 also update harmony read-model values such as `key`, `scale`, and `chord`.
- Channel 10 note 44 for status.
- Channel 10 note 45 for reload.
- Channel 10 note 47 for mode enable/disable.

## Smoke Check

1. Open `Reactive/show-action-document-status`.
2. Confirm the action document source is `session`, twelve actions are loaded, and the Trigger Slots section shows seeded clips such as `Reactive Cue 0 Reset` plus empty slots.
3. Trigger note 36 and confirm route 0 receives `Reactive Rhythm State MVP`, resets rhythm parameters, updates the harmony bank to `key = C_minor`, `scale = aeolian`, `chord = i`, keeps route 0 trigger slot 0 visible as `Reactive Cue 0 Reset playable`, and launches cue row 0 if it exists.
4. Trigger notes 37, 38, and 39 to change route 0 rhythm density, chance, priority, or rotation, and confirm the harmony bank moves through `iv`, `VI`, and `V`.
5. While transport is stopped, trigger note 40 and confirm transport starts; while transport is rolling, trigger note 41 or live trigger note 46 and confirm cue row 4 launches if it exists.
6. While transport is rolling, trigger note 42 and confirm transport stops; while stopped, trigger note 43 and confirm the state bank reports `transport = stopped-ready`.
7. Move CC 22 on channel 1 and confirm route 0 rhythm density and route 0 slot 0 trigger follow probability follow the controller value while the macro bank shows `filter` changing between `0.0` and `1.0`, `texture` morphing from `0.20` to `0.80`, and `space` morphing from `0.80` to `0.25`; the status or Cue-page preview should show the resolved CC-derived trigger-probability and macro command details.
8. Locate before the template-created `Breakdown` marker, or add a visible `Breakdown` marker in a manual existing-session setup, roll transport across it, and confirm the panel or status dialog reports `demo.marker.breakdown`, `section = breakdown`, `chord = bVII`, `filter = 0.25`, and route 0 rhythm density/chance changes.
9. Locate before the template-created `Breakdown Loop` region, or create, record, draw, or import a non-hidden region with that exact name in a manual existing-session setup, roll transport across it, and confirm the panel or status dialog reports `demo.region.breakdown.loop`, `section = region-breakdown`, `chord = i7`, `filter = 0.55`, and route 0 rhythm density/chance changes.
10. Launch Cue row 3 from the Cue page and confirm the panel or status dialog reports `demo.scene.drop`, `section = scene-drop`, `chord = V7`, `filter = 0.70`, and route 0 rhythm density/chance/rotation changes while the normal cue launch still happens.
11. When a nonzero-quantized action is pending, confirm the panel or status dialog lists the queued action's command details before the action releases.
12. If `Generic MIDI Control Out` is connected, confirm notes 36-43 reflect idle/latest/disabled feedback values.

## Current Limit

This asset deliberately avoids a full `.ardour` session file because Ardour session XML contains many generated IDs, ports, and environment-dependent connections. The bundled SessionInit template is the repeatable demo-session path, creates Cue-page visible MIDI lanes, seeds the `Breakdown` marker, `Breakdown Loop` timeline region, and first six named trigger clips with 12 total notes, and installs the demo action document automatically; packaging a polished generated arrangement remains a later demo-production step once the route/clip layout is stable enough to verify.
