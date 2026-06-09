#pragma once

#include <cppunit/extensions/HelperMacros.h>

#include "test_needing_session.h"

class ReactiveSessionTargetRhythmInsertTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (ReactiveSessionTargetRhythmInsertTest);
	CPPUNIT_TEST (rhythmInsertAddsLuaProcToRemoteRoute);
	CPPUNIT_TEST (rhythmParameterActionsUpdateInsertedLuaProcControls);
	CPPUNIT_TEST (rhythmParameterActionsReportMissingInsertAndUnknownName);
	CPPUNIT_TEST (rhythmInsertReportsMissingRoute);
	CPPUNIT_TEST_SUITE_END ();

public:
	void rhythmInsertAddsLuaProcToRemoteRoute ();
	void rhythmParameterActionsUpdateInsertedLuaProcControls ();
	void rhythmParameterActionsReportMissingInsertAndUnknownName ();
	void rhythmInsertReportsMissingRoute ();
};
