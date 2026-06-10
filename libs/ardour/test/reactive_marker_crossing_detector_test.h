#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveMarkerCrossingDetectorTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveMarkerCrossingDetectorTest);
	CPPUNIT_TEST (reportsForwardCrossingsInTimelineOrder);
	CPPUNIT_TEST (doesNotRepeatAlreadyCrossedMarkers);
	CPPUNIT_TEST (resetsWhenTransportStopsOrMovesBackward);
	CPPUNIT_TEST (resynchronizesOnExplicitDiscontinuity);
	CPPUNIT_TEST_SUITE_END ();

public:
	void reportsForwardCrossingsInTimelineOrder ();
	void doesNotRepeatAlreadyCrossedMarkers ();
	void resetsWhenTransportStopsOrMovesBackward ();
	void resynchronizesOnExplicitDiscontinuity ();
};
