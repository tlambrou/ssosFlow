#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRhythmTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmTest);
	CPPUNIT_TEST (densityKeepsHighestPriorityEvents);
	CPPUNIT_TEST (chanceAppliesAfterDensity);
	CPPUNIT_TEST (rotationReportsShiftedPatternSteps);
	CPPUNIT_TEST (downbeatPriorityUsesRotatedSteps);
	CPPUNIT_TEST (pendingSettingsApplyOnlyAtQuantizedBoundary);
	CPPUNIT_TEST_SUITE_END ();

public:
	void densityKeepsHighestPriorityEvents ();
	void chanceAppliesAfterDensity ();
	void rotationReportsShiftedPatternSteps ();
	void downbeatPriorityUsesRotatedSteps ();
	void pendingSettingsApplyOnlyAtQuantizedBoundary ();
};
