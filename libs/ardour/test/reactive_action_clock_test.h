#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionClockTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionClockTest);
	CPPUNIT_TEST (zeroQuantizeIsDueAtRequestTime);
	CPPUNIT_TEST (barQuantizeRoundsToNextBarBoundary);
	CPPUNIT_TEST (beatQuantizeRoundsToNextBeatBoundary);
	CPPUNIT_TEST (samplePositionUsesTempoMapForRequestedBbt);
	CPPUNIT_TEST_SUITE_END ();

public:
	void zeroQuantizeIsDueAtRequestTime ();
	void barQuantizeRoundsToNextBarBoundary ();
	void beatQuantizeRoundsToNextBeatBoundary ();
	void samplePositionUsesTempoMapForRequestedBbt ();
};
