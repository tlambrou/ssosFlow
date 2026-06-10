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
	CPPUNIT_TEST (parseMidiValueMacroCommand);
	CPPUNIT_TEST (parseMacroSnapshotCommands);
	CPPUNIT_TEST (parseWhenConditions);
	CPPUNIT_TEST (parseSequentialAndRandomChains);
	CPPUNIT_TEST (parseRhythmInsertCommand);
	CPPUNIT_TEST (parseRouteScopedRhythmCommand);
	CPPUNIT_TEST (parseRouteScopedRhythmCommandWithMidiValue);
	CPPUNIT_TEST (parseMarkerTriggerAndTransportCommands);
	CPPUNIT_TEST (parseSceneTrigger);
	CPPUNIT_TEST (rejectInvalidSceneTrigger);
	CPPUNIT_TEST (parseSceneStateAndTriggerCommands);
	CPPUNIT_TEST (parseHarmonyCommandAndCondition);
	CPPUNIT_TEST (rejectInvalidQuantize);
	CPPUNIT_TEST (rejectInvalidRhythmInsert);
	CPPUNIT_TEST (rejectInvalidRhythmCommand);
	CPPUNIT_TEST (rejectInvalidWhenCondition);
	CPPUNIT_TEST (rejectUnknownCommand);
	CPPUNIT_TEST_SUITE_END ();

public:
	void parseMinimalAction ();
	void rejectDuplicateActionNames ();
	void parseMidiNoteTrigger ();
	void parseMidiCCTrigger ();
	void parseMidiValueMacroCommand ();
	void parseMacroSnapshotCommands ();
	void parseWhenConditions ();
	void parseSequentialAndRandomChains ();
	void parseRhythmInsertCommand ();
	void parseRouteScopedRhythmCommand ();
	void parseRouteScopedRhythmCommandWithMidiValue ();
	void parseMarkerTriggerAndTransportCommands ();
	void parseSceneTrigger ();
	void rejectInvalidSceneTrigger ();
	void parseSceneStateAndTriggerCommands ();
	void parseHarmonyCommandAndCondition ();
	void rejectInvalidQuantize ();
	void rejectInvalidRhythmInsert ();
	void rejectInvalidRhythmCommand ();
	void rejectInvalidWhenCondition ();
	void rejectUnknownCommand ();
};
