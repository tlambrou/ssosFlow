# Reactive Performance Mode MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first minimal vertical slice of Ardour Reactive Performance Mode: a declarative action parser, action registry, controller-triggerable action calls, a small Cue-page UI panel, a Lua MIDI rhythm prototype, and demo documentation.

**Architecture:** Add the reactive core as a small native `libs/ardour` module with no audio-thread parsing or filesystem work. Reuse Ardour's existing TriggerBox/Cue APIs, Generic MIDI action maps, TempoMap quantization primitives, LuaProc MIDI processing, and GTK2/YTK Cue page UI.

**Tech Stack:** C++ in `libs/ardour`, CPPUnit via `libs/ardour/test`, GTK2/YTK in `gtk2_ardour`, Ardour Generic MIDI XML maps, Ardour LuaProc scripts, Waf.

---

## Files

- Create: `libs/ardour/ardour/reactive_action.h`
- Create: `libs/ardour/ardour/reactive_action_engine.h`
- Create: `libs/ardour/reactive_action.cc`
- Create: `libs/ardour/reactive_action_engine.cc`
- Create: `libs/ardour/test/reactive_action_test.h`
- Create: `libs/ardour/test/reactive_action_test.cc`
- Modify: `libs/ardour/wscript`
- Create: `share/midi_maps/reactive-performance-mvp.map`
- Create: `gtk2_ardour/reactive_performance_panel.h`
- Create: `gtk2_ardour/reactive_performance_panel.cc`
- Modify: `gtk2_ardour/trigger_page.h`
- Modify: `gtk2_ardour/trigger_page.cc`
- Modify: `gtk2_ardour/wscript`
- Create: `share/scripts/reactive_rhythm_state_mvp.lua`
- Create: `docs/reactive-performance-mode/demo-session.md`

## Task 1: Prepare Full Build Checkout

**Files:**
- No source files changed.

- [ ] **Step 1: Convert shallow checkout to full history**

Run:

```bash
git fetch --unshallow --tags upstream
git fetch --tags origin
```

Expected: `git rev-parse --is-shallow-repository` prints `false`.

- [ ] **Step 2: Verify waf help through Python 3**

Run:

```bash
python3 ./waf --help
```

Expected: command exits 0 and lists `configure`, `build`, `--with-backends`, `--compile-database`, and `--test`.

- [ ] **Step 3: Configure a test-capable macOS build**

Run:

```bash
python3 ./waf configure --with-backends=coreaudio --cxx17 --compile-database --test
```

Expected: configure either succeeds or reports a concrete missing dependency. If a dependency is missing, update issue #1 with the missing package name and stop before code implementation.

- [ ] **Step 4: Commit only if build prerequisites changed**

Do not commit environment-only changes. If no files changed, skip commit.

## Task 2: Add Reactive Action Parser Tests

**Files:**
- Create: `libs/ardour/test/reactive_action_test.h`
- Create: `libs/ardour/test/reactive_action_test.cc`
- Modify: `libs/ardour/wscript`

- [ ] **Step 1: Add the CPPUnit header**

Create `libs/ardour/test/reactive_action_test.h`:

```cpp
#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionTest);
	CPPUNIT_TEST (parseMinimalAction);
	CPPUNIT_TEST (rejectDuplicateActionNames);
	CPPUNIT_TEST (parseMidiNoteTrigger);
	CPPUNIT_TEST (parseMidiCCTrigger);
	CPPUNIT_TEST (parseSequentialAndRandomChains);
	CPPUNIT_TEST (rejectInvalidQuantize);
	CPPUNIT_TEST_SUITE_END ();

public:
	void parseMinimalAction ();
	void rejectDuplicateActionNames ();
	void parseMidiNoteTrigger ();
	void parseMidiCCTrigger ();
	void parseSequentialAndRandomChains ();
	void rejectInvalidQuantize ();
};
```

- [ ] **Step 2: Add failing parser tests**

Create `libs/ardour/test/reactive_action_test.cc`:

```cpp
#include "reactive_action_test.h"

#include "ardour/reactive_action.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionTest);

using namespace ARDOUR;

void
ReactiveActionTest::parseMinimalAction ()
{
	const char* src =
		"ACTION intro.drop\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), result.document.actions().front().name);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Cue, result.document.actions().front().commands.front().type);
}

void
ReactiveActionTest::rejectDuplicateActionNames ()
{
	const char* src =
		"ACTION a\n"
		"DO cue 0\n"
		"END\n"
		"ACTION a\n"
		"DO cue 1\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("duplicate action") != std::string::npos);
}

void
ReactiveActionTest::parseMidiNoteTrigger ()
{
	const char* src =
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (ReactiveTrigger::MidiNote, result.document.actions().front().triggers.front().type);
	CPPUNIT_ASSERT_EQUAL (10, result.document.actions().front().triggers.front().channel);
	CPPUNIT_ASSERT_EQUAL (36, result.document.actions().front().triggers.front().number);
}

void
ReactiveActionTest::parseMidiCCTrigger ()
{
	const char* src =
		"ACTION knob.high\n"
		"TRIGGER midi cc ch=1 cc=22 value>63\n"
		"DO macro filter 0.80 ramp 0|1|0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (ReactiveTrigger::MidiCC, result.document.actions().front().triggers.front().type);
	CPPUNIT_ASSERT_EQUAL (22, result.document.actions().front().triggers.front().number);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, result.document.actions().front().commands.front().type);
}

void
ReactiveActionTest::parseSequentialAndRandomChains ()
{
	const char* src =
		"ACTION mutate\n"
		"CHAIN sequential\n"
		"DO rhythm density 0.35\n"
		"DO rhythm density 0.55\n"
		"END\n"
		"ACTION choose\n"
		"CHAIN random\n"
		"DO cue 1\n"
		"DO cue 2\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (ReactiveChainMode::Sequential, result.document.actions()[0].chain_mode);
	CPPUNIT_ASSERT_EQUAL (ReactiveChainMode::Random, result.document.actions()[1].chain_mode);
}

void
ReactiveActionTest::rejectInvalidQuantize ()
{
	const char* src =
		"ACTION broken\n"
		"QUANTIZE potatoes\n"
		"DO cue 0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("invalid quantize") != std::string::npos);
}
```

- [ ] **Step 3: Register the tests in `libs/ardour/wscript`**

Add these files to the `test_sources` list:

```python
'test/reactive_action_test.cc',
```

- [ ] **Step 4: Run the test and verify it fails**

Run:

```bash
python3 ../../waf --targets=libardour-tests
./run-tests.sh
```

from `libs/ardour`.

Expected: build fails because `ardour/reactive_action.h` does not exist.

## Task 3: Implement Minimal Reactive Action Parser

**Files:**
- Create: `libs/ardour/ardour/reactive_action.h`
- Create: `libs/ardour/reactive_action.cc`
- Modify: `libs/ardour/wscript`

- [ ] **Step 1: Add parser API header**

Create `libs/ardour/ardour/reactive_action.h`:

```cpp
#pragma once

#include <map>
#include <string>
#include <vector>

#include "temporal/bbt_time.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

enum class ReactiveChainMode {
	All,
	Sequential,
	Random
};

struct LIBARDOUR_API ReactiveTrigger {
	enum Type {
		None,
		MidiNote,
		MidiCC,
		Marker
	};

	Type type = None;
	int channel = 0;
	int number = 0;
	int threshold = -1;
	std::string name;
};

struct LIBARDOUR_API ReactiveCommand {
	enum Type {
		Cue,
		Trigger,
		TriggerStop,
		StopAll,
		TransportPlay,
		TransportStop,
		SceneApply,
		SceneStore,
		Macro,
		State,
		Rhythm
	};

	Type type = Cue;
	std::string name;
	double value = 0.0;
	int first = 0;
	int second = 0;
	Temporal::BBT_Offset ramp;
};

struct LIBARDOUR_API ReactiveAction {
	std::string name;
	ReactiveChainMode chain_mode = ReactiveChainMode::All;
	Temporal::BBT_Offset quantize;
	std::vector<ReactiveTrigger> triggers;
	std::vector<ReactiveCommand> commands;
};

class LIBARDOUR_API ReactiveActionDocument {
public:
	static struct ReactiveActionParseResult parse (std::string const&);

	std::vector<ReactiveAction> const& actions () const { return _actions; }
	ReactiveAction const* action_by_name (std::string const&) const;

private:
	std::vector<ReactiveAction> _actions;
	std::map<std::string, size_t> _index;

	friend struct ReactiveActionParseResult;
};

struct LIBARDOUR_API ReactiveActionParseResult {
	bool ok = false;
	std::string error;
	ReactiveActionDocument document;
};

} // namespace ARDOUR
```

- [ ] **Step 2: Implement the parser**

Create `libs/ardour/reactive_action.cc` with a small tokenizer that supports the grammar in `docs/reactive-performance-mode/mvp-spec.md`. Keep parsing non-realtime and return line-numbered errors.

- [ ] **Step 3: Add source to `libs/ardour/wscript`**

Add:

```python
'reactive_action.cc',
```

to the libardour source list.

- [ ] **Step 4: Run parser tests**

Run:

```bash
cd libs/ardour
python3 ../../waf --targets=libardour-tests
./run-tests.sh
```

Expected: `ReactiveActionTest` passes.

- [ ] **Step 5: Commit parser**

Run:

```bash
git add libs/ardour/ardour/reactive_action.h libs/ardour/reactive_action.cc libs/ardour/test/reactive_action_test.h libs/ardour/test/reactive_action_test.cc libs/ardour/wscript
git commit -m "feat: add reactive action parser"
```

## Task 4: Add Minimal Reactive Action Engine

**Files:**
- Create: `libs/ardour/ardour/reactive_action_engine.h`
- Create: `libs/ardour/reactive_action_engine.cc`
- Modify: `libs/ardour/wscript`

- [ ] **Step 1: Add engine interface**

Create `libs/ardour/ardour/reactive_action_engine.h` with:

```cpp
#pragma once

#include <map>
#include <string>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API ReactiveActionEngine {
public:
	explicit ReactiveActionEngine (Session&);

	bool load_document (ReactiveActionDocument const&, std::string& error);
	bool trigger_action (std::string const& name, std::string& error);
	bool set_macro (std::string const& name, double value, std::string& error);
	double macro_value (std::string const& name) const;
	std::string last_action () const { return _last_action; }

private:
	bool execute_command (ReactiveCommand const&, std::string& error);

	Session& _session;
	ReactiveActionDocument _document;
	std::map<std::string, double> _macros;
	std::map<std::string, size_t> _sequential_positions;
	std::string _last_action;
};

} // namespace ARDOUR
```

- [ ] **Step 2: Implement existing-session actions only**

Implement commands by calling existing `Session` APIs:

- `cue`: `Session::trigger_cue_row`
- `trigger`: `Session::bang_trigger_at`
- `trigger-stop`: `Session::triggerbox_at(...)->stop_all_quantized`
- `stop-all`: `Session::trigger_stop_all(false)`
- `scene apply`: `Session::apply_nth_mixer_scene`
- `scene store`: `Session::store_nth_mixer_scene`

Do not implement parameter ramps in realtime yet. Store macro values and expose them to UI.

- [ ] **Step 3: Add source to build**

Add:

```python
'reactive_action_engine.cc',
```

to `libs/ardour/wscript`.

- [ ] **Step 4: Run tests and build**

Run:

```bash
cd libs/ardour
python3 ../../waf --targets=libardour-tests
./run-tests.sh
```

Expected: tests pass.

- [ ] **Step 5: Commit engine**

Run:

```bash
git add libs/ardour/ardour/reactive_action_engine.h libs/ardour/reactive_action_engine.cc libs/ardour/wscript
git commit -m "feat: add reactive action engine skeleton"
```

## Task 5: Expose MIDI-Triggerable Reactive Actions

**Files:**
- Create: `share/midi_maps/reactive-performance-mvp.map`
- Modify: `gtk2_ardour/ardour_ui_ed.cc` or the smallest existing action-registration location that owns Cue/Reactive actions.

- [ ] **Step 1: Add static action names**

Register actions:

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

Each action should call the reactive engine with the matching action slot from the loaded document.

- [ ] **Step 2: Add MIDI map**

Create `share/midi_maps/reactive-performance-mvp.map`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ArdourMIDIBindings version="1.0.0" name="Reactive Performance MVP" manufacturer="Generic">
  <DeviceInfo bank-size="8" motorised="no"/>
  <Binding channel="10" note="36" action="Reactive/trigger-action-0" momentary="yes"/>
  <Binding channel="10" note="37" action="Reactive/trigger-action-1" momentary="yes"/>
  <Binding channel="10" note="38" action="Reactive/trigger-action-2" momentary="yes"/>
  <Binding channel="10" note="39" action="Reactive/trigger-action-3" momentary="yes"/>
  <Binding channel="10" note="40" action="Reactive/trigger-action-4" momentary="yes"/>
  <Binding channel="10" note="41" action="Reactive/trigger-action-5" momentary="yes"/>
  <Binding channel="10" note="42" action="Reactive/trigger-action-6" momentary="yes"/>
  <Binding channel="10" note="43" action="Reactive/trigger-action-7" momentary="yes"/>
</ArdourMIDIBindings>
```

- [ ] **Step 3: Smoke test action registration**

Run Ardour, enable Generic MIDI, load "Reactive Performance MVP", and confirm the actions appear in MIDI binding/action lookup. If the action group does not appear, move registration to the same layer used by `Cues/trigger-cue-*`.

- [ ] **Step 4: Commit MIDI action exposure**

Run:

```bash
git add share/midi_maps/reactive-performance-mvp.map gtk2_ardour/ardour_ui_ed.cc
git commit -m "feat: expose reactive actions to generic MIDI"
```

## Task 6: Add Minimal Reactive Performance Panel

**Files:**
- Create: `gtk2_ardour/reactive_performance_panel.h`
- Create: `gtk2_ardour/reactive_performance_panel.cc`
- Modify: `gtk2_ardour/trigger_page.h`
- Modify: `gtk2_ardour/trigger_page.cc`
- Modify: `gtk2_ardour/wscript`

- [ ] **Step 1: Create panel class**

Create a GTK2/YTK widget with:

- enable toggle
- reload button
- eight macro labels
- last action label
- next action preview label
- parser error label

- [ ] **Step 2: Embed the panel in TriggerPage sidebar**

Add the panel as a sidebar page in `TriggerPage`, next to existing trigger source/region/route list pages.

- [ ] **Step 3: Build GUI target**

Run:

```bash
python3 ./waf build --targets=gtk2_ardour
```

Expected: GUI target builds.

- [ ] **Step 4: Manual smoke test**

Open the Cue page and confirm the panel is visible, does not resize the trigger grid badly, and shows a validation error for a malformed action file.

- [ ] **Step 5: Commit panel**

Run:

```bash
git add gtk2_ardour/reactive_performance_panel.h gtk2_ardour/reactive_performance_panel.cc gtk2_ardour/trigger_page.h gtk2_ardour/trigger_page.cc gtk2_ardour/wscript
git commit -m "feat: add reactive performance panel"
```

## Task 7: Add Reactive Rhythm LuaProc Prototype

**Files:**
- Create: `share/scripts/reactive_rhythm_state_mvp.lua`

- [ ] **Step 1: Create LuaProc script**

Create a Lua DSP script with parameters:

```lua
ardour {
	["type"] = "dsp",
	name = "Reactive Rhythm State MVP",
	category = "MIDI",
	license = "MIT",
	author = "ssosFlow",
	description = "MIDI density, chance, priority, and rotation prototype for Reactive Performance Mode"
}
```

Implement `dsp_ioconfig`, `dsp_params`, and `dsp_runmap` so MIDI input events are copied to MIDI output according to density/chance/priority/rotation.

- [ ] **Step 2: Verify Lua script is discoverable**

Run Ardour and confirm the script appears as a Lua DSP plugin. If script scanning fails, run the Lua script manager and record the parser error in issue #1.

- [ ] **Step 3: Smoke test MIDI behavior**

Create a MIDI track, insert the LuaProc, play a dense MIDI clip, and confirm density 0 drops note-ons while density 1 passes them. Verify note-offs do not hang.

- [ ] **Step 4: Commit rhythm prototype**

Run:

```bash
git add share/scripts/reactive_rhythm_state_mvp.lua
git commit -m "feat: add reactive rhythm Lua prototype"
```

## Task 8: Demo Session and Documentation

**Files:**
- Create: `docs/reactive-performance-mode/demo-session.md`

- [ ] **Step 1: Document demo session setup**

Create `docs/reactive-performance-mode/demo-session.md` with:

- required Ardour build/run command
- track layout
- cue row layout
- Generic MIDI setup
- reactive action file contents
- rhythm LuaProc insertion steps
- smoke-test checklist

- [ ] **Step 2: Run end-to-end smoke test**

Run Ardour, load the demo setup, trigger action 0 from MIDI note 36, confirm cue row 0 launches, and confirm the panel shows last action.

- [ ] **Step 3: Update issue #1**

Post a GitHub issue comment with:

- completed work
- commands run
- local build result
- smoke-test result
- known risks
- follow-up issues needed

- [ ] **Step 4: Commit docs**

Run:

```bash
git add docs/reactive-performance-mode/demo-session.md
git commit -m "docs: add reactive performance demo guide"
```

## Task 9: Open PR to Staging

**Files:**
- No source files changed.

- [ ] **Step 1: Push feature branch**

Run:

```bash
GIT_CONFIG_GLOBAL=/dev/null git push -u origin feature/codex-1-reactive-performance-research
```

- [ ] **Step 2: Open PR targeting staging**

Run:

```bash
gh pr create --repo tlambrou/ssosFlow --base staging --head feature/codex-1-reactive-performance-research --title "Research Reactive Performance Mode MVP" --body "Closes #1"
```

- [ ] **Step 3: Verify PR metadata**

Confirm:

- PR targets `staging`.
- PR links issue #1.
- PR body lists build/test commands.
- PR includes known risks and follow-up issues.
