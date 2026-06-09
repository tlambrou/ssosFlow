#pragma once

#include <cppunit/extensions/HelperMacros.h>

#include "test_needing_session.h"

class ReactiveRhythmRouteInserterTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmRouteInserterTest);
	CPPUNIT_TEST (insertsReactiveRhythmLuaProcIntoMidiTrack);
	CPPUNIT_TEST (repeatedInsertionReturnsExistingProcessor);
	CPPUNIT_TEST_SUITE_END ();

public:
	void insertsReactiveRhythmLuaProcIntoMidiTrack ();
	void repeatedInsertionReturnsExistingProcessor ();
};
