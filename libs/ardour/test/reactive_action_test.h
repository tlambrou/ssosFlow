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
	CPPUNIT_TEST (parseMarkerTriggerAndTransportCommands);
	CPPUNIT_TEST (parseSceneStateAndTriggerCommands);
	CPPUNIT_TEST (rejectInvalidQuantize);
	CPPUNIT_TEST (rejectUnknownCommand);
	CPPUNIT_TEST_SUITE_END ();

public:
	void parseMinimalAction ();
	void rejectDuplicateActionNames ();
	void parseMidiNoteTrigger ();
	void parseMidiCCTrigger ();
	void parseSequentialAndRandomChains ();
	void parseMarkerTriggerAndTransportCommands ();
	void parseSceneStateAndTriggerCommands ();
	void rejectInvalidQuantize ();
	void rejectUnknownCommand ();
};
