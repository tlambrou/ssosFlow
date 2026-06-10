#include "reactive_action_test.h"

#include "ardour/reactive_action.h"

#include <string>

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
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), result.document.actions ().front ().name);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Cue, result.document.actions ().front ().commands.front ().type);
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
	CPPUNIT_ASSERT_EQUAL (ReactiveTrigger::MidiNote, result.document.actions ().front ().triggers.front ().type);
	CPPUNIT_ASSERT_EQUAL (10, result.document.actions ().front ().triggers.front ().channel);
	CPPUNIT_ASSERT_EQUAL (36, result.document.actions ().front ().triggers.front ().number);
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
	CPPUNIT_ASSERT_EQUAL (ReactiveTrigger::MidiCC, result.document.actions ().front ().triggers.front ().type);
	CPPUNIT_ASSERT_EQUAL (22, result.document.actions ().front ().triggers.front ().number);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, result.document.actions ().front ().commands.front ().type);
}

void
ReactiveActionTest::parseMidiValueMacroCommand ()
{
	const char* src =
		"ACTION knob.live\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO macro filter midi-value ramp 0|1|0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);

	ReactiveCommand const& command = result.document.actions ().front ().commands.front ();
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, command.type);
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), command.name);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::MidiEventValue, command.value_source);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, command.value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (1, command.ramp.beats);
}

void
ReactiveActionTest::parseMacroSnapshotCommands ()
{
	const char* src =
		"ACTION snapshots\n"
		"DO macro filter 0.40\n"
		"DO macro snapshot store verse\n"
		"DO macro snapshot recall chorus ramp 0|2|0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);

	ReactiveAction const& action = result.document.actions ().front ();
	CPPUNIT_ASSERT_EQUAL (size_t (3), action.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::MacroSnapshotStore, action.commands[1].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("verse"), action.commands[1].name);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::MacroSnapshotRecall, action.commands[2].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("chorus"), action.commands[2].name);
	CPPUNIT_ASSERT_EQUAL (2, action.commands[2].ramp.beats);
}

void
ReactiveActionTest::parseWhenConditions ()
{
	const char* src =
		"ACTION gated\n"
		"WHEN state section breakdown\n"
		"WHEN macro filter 0.75\n"
		"WHEN transport rolling\n"
		"DO cue 0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);

	ReactiveAction const& action = result.document.actions ().front ();
	CPPUNIT_ASSERT_EQUAL (size_t (3), action.conditions.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCondition::StateEquals, action.conditions[0].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("section"), action.conditions[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), action.conditions[0].text);
	CPPUNIT_ASSERT_EQUAL (ReactiveCondition::MacroEquals, action.conditions[1].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), action.conditions[1].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.75, action.conditions[1].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (ReactiveCondition::TransportRolling, action.conditions[2].type);
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
	CPPUNIT_ASSERT (ReactiveChainMode::Sequential == result.document.actions ()[0].chain_mode);
	CPPUNIT_ASSERT (ReactiveChainMode::Random == result.document.actions ()[1].chain_mode);
}

void
ReactiveActionTest::parseRhythmInsertCommand ()
{
	const char* src =
		"ACTION add.rhythm\n"
		"TRIGGER midi note ch=1 note=60\n"
		"DO rhythm insert 0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::RhythmInsert, result.document.actions ().front ().commands.front ().type);
	CPPUNIT_ASSERT_EQUAL (0, result.document.actions ().front ().commands.front ().first);
}

void
ReactiveActionTest::parseRouteScopedRhythmCommand ()
{
	const char* src =
		"ACTION route.rhythm\n"
		"DO rhythm route 2 density 0.35\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);

	ReactiveCommand const& command = result.document.actions ().front ().commands.front ();
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::RhythmRoute, command.type);
	CPPUNIT_ASSERT_EQUAL (2, command.first);
	CPPUNIT_ASSERT_EQUAL (std::string ("density"), command.name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.35, command.value, 0.0001);
}

void
ReactiveActionTest::parseRouteScopedRhythmCommandWithMidiValue ()
{
	const char* src =
		"ACTION route.rhythm.live\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO rhythm route 0 density midi-value\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);

	ReactiveCommand const& command = result.document.actions ().front ().commands.front ();
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::RhythmRoute, command.type);
	CPPUNIT_ASSERT_EQUAL (0, command.first);
	CPPUNIT_ASSERT_EQUAL (std::string ("density"), command.name);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::MidiEventValue, command.value_source);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, command.value, 0.0001);
}

void
ReactiveActionTest::parseMarkerTriggerAndTransportCommands ()
{
	const char* src =
		"ACTION breakdown\n"
		"TRIGGER marker Breakdown\n"
		"DO transport play\n"
		"DO transport stop after 4|0|0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (ReactiveTrigger::Marker, result.document.actions ().front ().triggers.front ().type);
	CPPUNIT_ASSERT_EQUAL (std::string ("Breakdown"), result.document.actions ().front ().triggers.front ().name);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::TransportPlay, result.document.actions ().front ().commands[0].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::TransportStop, result.document.actions ().front ().commands[1].type);
	CPPUNIT_ASSERT_EQUAL (4, result.document.actions ().front ().commands[1].ramp.bars);
}

void
ReactiveActionTest::parseSceneTrigger ()
{
	const char* src =
		"ACTION drop.scene\n"
		"TRIGGER scene 3\n"
		"DO cue 3\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (ReactiveTrigger::Scene, result.document.actions ().front ().triggers.front ().type);
	CPPUNIT_ASSERT_EQUAL (3, result.document.actions ().front ().triggers.front ().number);
}

void
ReactiveActionTest::rejectInvalidSceneTrigger ()
{
	const char* missing_index =
		"ACTION scene.missing\n"
		"TRIGGER scene\n"
		"DO cue 0\n"
		"END\n";

	ReactiveActionParseResult missing = ReactiveActionDocument::parse (missing_index);
	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("invalid scene trigger") != std::string::npos);

	const char* negative_index =
		"ACTION scene.negative\n"
		"TRIGGER scene -1\n"
		"DO cue 0\n"
		"END\n";

	ReactiveActionParseResult negative = ReactiveActionDocument::parse (negative_index);
	CPPUNIT_ASSERT_EQUAL (false, negative.ok);
	CPPUNIT_ASSERT (negative.error.find ("invalid scene trigger") != std::string::npos);
}

void
ReactiveActionTest::parseSceneStateAndTriggerCommands ()
{
	const char* src =
		"ACTION scene.ops\n"
		"DO trigger 2 4\n"
		"DO trigger-stop 2\n"
		"DO stop-all\n"
		"DO scene apply 3\n"
		"DO scene store 4\n"
		"DO state mode breakdown\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Trigger, result.document.actions ().front ().commands[0].type);
	CPPUNIT_ASSERT_EQUAL (2, result.document.actions ().front ().commands[0].first);
	CPPUNIT_ASSERT_EQUAL (4, result.document.actions ().front ().commands[0].second);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::TriggerStop, result.document.actions ().front ().commands[1].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::StopAll, result.document.actions ().front ().commands[2].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::SceneApply, result.document.actions ().front ().commands[3].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::SceneStore, result.document.actions ().front ().commands[4].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::State, result.document.actions ().front ().commands[5].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("mode"), result.document.actions ().front ().commands[5].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), result.document.actions ().front ().commands[5].text);
}

void
ReactiveActionTest::parseHarmonyCommandAndCondition ()
{
	const char* src =
		"ACTION harmonic.shift\n"
		"WHEN harmony key C_minor\n"
		"DO harmony key E_flat_major\n"
		"DO harmony chord i\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	ReactiveAction const& action = result.document.actions ().front ();
	CPPUNIT_ASSERT_EQUAL (size_t (1), action.conditions.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCondition::HarmonyEquals, action.conditions[0].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("key"), action.conditions[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("C_minor"), action.conditions[0].text);
	CPPUNIT_ASSERT_EQUAL (size_t (2), action.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Harmony, action.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("key"), action.commands[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("E_flat_major"), action.commands[0].text);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Harmony, action.commands[1].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("chord"), action.commands[1].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("i"), action.commands[1].text);
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

void
ReactiveActionTest::rejectInvalidRhythmInsert ()
{
	const char* src =
		"ACTION broken\n"
		"DO rhythm insert potatoes\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("invalid rhythm insert") != std::string::npos);
	CPPUNIT_ASSERT (result.error.find ("line 2") != std::string::npos);
}

void
ReactiveActionTest::rejectInvalidRhythmCommand ()
{
	const char* src =
		"ACTION broken\n"
		"DO rhythm\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("invalid rhythm command") != std::string::npos);
	CPPUNIT_ASSERT (result.error.find ("line 2") != std::string::npos);
}

void
ReactiveActionTest::rejectInvalidWhenCondition ()
{
	const char* src =
		"ACTION broken\n"
		"WHEN potatoes now\n"
		"DO cue 0\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("unknown condition") != std::string::npos);
	CPPUNIT_ASSERT (result.error.find ("line 2") != std::string::npos);
}

void
ReactiveActionTest::rejectUnknownCommand ()
{
	const char* src =
		"ACTION broken\n"
		"DO warp now\n"
		"END\n";

	ReactiveActionParseResult result = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("unknown command") != std::string::npos);
	CPPUNIT_ASSERT (result.error.find ("line 2") != std::string::npos);
}
