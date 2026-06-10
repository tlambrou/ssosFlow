#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRegionCrossingDetectorTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRegionCrossingDetectorTest);
	CPPUNIT_TEST (reportsForwardRegionStartsInTimelineOrder);
	CPPUNIT_TEST (ordersSameSampleRegionsByRouteAndObservation);
	CPPUNIT_TEST (doesNotRepeatAlreadyCrossedRegions);
	CPPUNIT_TEST (resetsWhenTransportStopsOrMovesBackward);
	CPPUNIT_TEST (resynchronizesOnExplicitDiscontinuity);
	CPPUNIT_TEST_SUITE_END ();

public:
	void reportsForwardRegionStartsInTimelineOrder ();
	void ordersSameSampleRegionsByRouteAndObservation ();
	void doesNotRepeatAlreadyCrossedRegions ();
	void resetsWhenTransportStopsOrMovesBackward ();
	void resynchronizesOnExplicitDiscontinuity ();
};
