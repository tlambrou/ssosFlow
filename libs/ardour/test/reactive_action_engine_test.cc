#include "reactive_action_engine_test.h"

#include "ardour/reactive_action.h"
#include "ardour/reactive_action_engine.h"

#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionEngineTest);

using namespace ARDOUR;

static ReactiveActionEngine
engine_from_source (char const* src)
{
	ReactiveActionParseResult parsed = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, parsed.ok);

	ReactiveActionEngine engine;
	std::string error;
	CPPUNIT_ASSERT_EQUAL (true, engine.load_document (parsed.document, error));
	return engine;
}

void
ReactiveActionEngineTest::matchMidiNoteTrigger ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"QUANTIZE 1|0|0\n"
		"CHAIN sequential\n"
		"DO cue 0\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 100));

	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT (matches[0].action != 0);
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT (ReactiveChainMode::Sequential == matches[0].chain_mode);
	CPPUNIT_ASSERT_EQUAL (1, matches[0].quantize.bars);
}

void
ReactiveActionEngineTest::ignoreMismatchedMidiNote ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (9, 36, 100)).empty ());
	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (10, 37, 100)).empty ());
}

void
ReactiveActionEngineTest::ignoreZeroVelocityNoteOn ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 0)).empty ());
}

void
ReactiveActionEngineTest::matchMidiCCThreshold ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION knob.high\n"
		"TRIGGER midi cc ch=1 cc=22 value>63\n"
		"DO macro filter 0.80 ramp 0|1|0\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::control_change (1, 22, 63)).empty ());

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (ReactiveMidiEvent::control_change (1, 22, 64));
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.high"), matches[0].action->name);
}

void
ReactiveActionEngineTest::matchMidiCCWithoutThreshold ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION knob.any\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO macro filter 0.80\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (ReactiveMidiEvent::control_change (1, 22, 0));
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.any"), matches[0].action->name);
}

void
ReactiveActionEngineTest::ignoreMarkerTriggersForMidi ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION breakdown\n"
		"TRIGGER marker Breakdown\n"
		"DO transport stop\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 100)).empty ());
	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::control_change (1, 22, 127)).empty ());
}

void
ReactiveActionEngineTest::keepDocumentOrderForMultipleMatches ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION first\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 1\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 127));

	CPPUNIT_ASSERT_EQUAL (size_t (2), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (std::string ("second"), matches[1].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches[1].action_index);
}

void
ReactiveActionEngineTest::triggerAllChainReturnsAllCommandsAndMetadata ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION intro.drop\n"
		"TRIGGER midi note ch=10 note=36\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 0\n"
		"DO macro filter 0.72 ramp 0|2|0\n"
		"DO scene apply 0\n"
		"END\n");

	ReactiveActionPlan plan = engine.trigger_action ("intro.drop");

	CPPUNIT_ASSERT_EQUAL (true, plan.ok);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), plan.action_name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), plan.action_index);
	CPPUNIT_ASSERT (ReactiveChainMode::All == plan.chain_mode);
	CPPUNIT_ASSERT_EQUAL (1, plan.quantize.bars);
	CPPUNIT_ASSERT_EQUAL (size_t (3), plan.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Cue, plan.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, plan.commands[1].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::SceneApply, plan.commands[2].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), engine.last_action ());
}

void
ReactiveActionEngineTest::rotateSequentialChainCommands ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION drums.mutate\n"
		"CHAIN sequential\n"
		"DO rhythm density 0.35\n"
		"DO rhythm density 0.55\n"
		"DO rhythm density 0.80\n"
		"END\n");

	ReactiveActionPlan first = engine.trigger_action ("drums.mutate");
	ReactiveActionPlan second = engine.trigger_action ("drums.mutate");
	ReactiveActionPlan third = engine.trigger_action ("drums.mutate");
	ReactiveActionPlan wrapped = engine.trigger_action ("drums.mutate");

	CPPUNIT_ASSERT_EQUAL (true, first.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), first.commands.size ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.35, first.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.55, second.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, third.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.35, wrapped.commands[0].value, 0.0001);
}

void
ReactiveActionEngineTest::rejectUnknownActionWithoutChangingLastAction ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION known\n"
		"DO cue 0\n"
		"END\n");

	ReactiveActionPlan known = engine.trigger_action ("known");
	ReactiveActionPlan missing = engine.trigger_action ("missing");

	CPPUNIT_ASSERT_EQUAL (true, known.ok);
	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("unknown action") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (std::string ("known"), engine.last_action ());
}

void
ReactiveActionEngineTest::trackMacroAndStateValuesFromSelectedCommands ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION performance.mode\n"
		"DO macro filter 0.80\n"
		"DO state section breakdown\n"
		"END\n");

	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string (), engine.state_value ("section"));

	ReactiveActionPlan plan = engine.trigger_action ("performance.mode");

	CPPUNIT_ASSERT_EQUAL (true, plan.ok);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), engine.state_value ("section"));
}
