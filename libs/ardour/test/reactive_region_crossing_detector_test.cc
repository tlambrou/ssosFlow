#include "reactive_region_crossing_detector_test.h"

#include "ardour/reactive_region_crossing_detector.h"

#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRegionCrossingDetectorTest);

using namespace ARDOUR;

namespace {

static ReactiveRegionObservation
region (std::string const& name, samplepos_t sample, std::string const& route = std::string (), size_t route_order = 0)
{
	return ReactiveRegionObservation::at (name, sample, route, route_order);
}

} // namespace

void
ReactiveRegionCrossingDetectorTest::reportsForwardRegionStartsInTimelineOrder ()
{
	ReactiveRegionCrossingDetector detector;
	std::vector<ReactiveRegionObservation> regions;
	regions.push_back (region ("Drop", 480, "Drums", 0));
	regions.push_back (region ("Breakdown Loop", 240, "Bass", 1));
	regions.push_back (region ("Build", 360, "Keys", 2));

	CPPUNIT_ASSERT (detector.poll (100, true, regions).empty ());
	std::vector<ReactiveRegionCrossing> const hits = detector.poll (400, true, regions);

	CPPUNIT_ASSERT_EQUAL (size_t (2), hits.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Breakdown Loop"), hits[0].name);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (240), hits[0].sample);
	CPPUNIT_ASSERT_EQUAL (std::string ("Bass"), hits[0].route_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("Build"), hits[1].name);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (360), hits[1].sample);
	CPPUNIT_ASSERT_EQUAL (std::string ("Keys"), hits[1].route_name);
}

void
ReactiveRegionCrossingDetectorTest::ordersSameSampleRegionsByRouteAndObservation ()
{
	ReactiveRegionCrossingDetector detector;
	std::vector<ReactiveRegionObservation> regions;
	regions.push_back (region ("Keys Hit", 240, "Keys", 2));
	regions.push_back (region ("Drums Hit", 240, "Drums", 0));
	regions.push_back (region ("Bass Hit", 240, "Bass", 1));
	regions.push_back (region ("Bass Layer", 240, "Bass", 1));

	CPPUNIT_ASSERT (detector.poll (100, true, regions).empty ());
	std::vector<ReactiveRegionCrossing> const hits = detector.poll (300, true, regions);

	CPPUNIT_ASSERT_EQUAL (size_t (4), hits.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Drums Hit"), hits[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("Bass Hit"), hits[1].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("Bass Layer"), hits[2].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("Keys Hit"), hits[3].name);
}

void
ReactiveRegionCrossingDetectorTest::doesNotRepeatAlreadyCrossedRegions ()
{
	ReactiveRegionCrossingDetector detector;
	std::vector<ReactiveRegionObservation> regions;
	regions.push_back (region ("Drop", 240));

	CPPUNIT_ASSERT (detector.poll (100, true, regions).empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), detector.poll (250, true, regions).size ());
	CPPUNIT_ASSERT (detector.poll (300, true, regions).empty ());
}

void
ReactiveRegionCrossingDetectorTest::resetsWhenTransportStopsOrMovesBackward ()
{
	ReactiveRegionCrossingDetector detector;
	std::vector<ReactiveRegionObservation> regions;
	regions.push_back (region ("Breakdown Loop", 200));

	CPPUNIT_ASSERT (detector.poll (100, true, regions).empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), detector.poll (250, true, regions).size ());

	CPPUNIT_ASSERT (detector.poll (100, false, regions).empty ());
	std::vector<ReactiveRegionCrossing> replayed = detector.poll (250, true, regions);
	CPPUNIT_ASSERT_EQUAL (size_t (1), replayed.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Breakdown Loop"), replayed[0].name);

	CPPUNIT_ASSERT (detector.poll (150, true, regions).empty ());
	replayed = detector.poll (250, true, regions);
	CPPUNIT_ASSERT_EQUAL (size_t (1), replayed.size ());
}

void
ReactiveRegionCrossingDetectorTest::resynchronizesOnExplicitDiscontinuity ()
{
	ReactiveRegionCrossingDetector detector;
	std::vector<ReactiveRegionObservation> regions;
	regions.push_back (region ("Skipped", 200));
	regions.push_back (region ("Next", 550));

	CPPUNIT_ASSERT (detector.poll (100, true, regions).empty ());
	CPPUNIT_ASSERT (detector.poll (500, true, regions, true).empty ());

	std::vector<ReactiveRegionCrossing> const hits = detector.poll (600, true, regions);
	CPPUNIT_ASSERT_EQUAL (size_t (1), hits.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Next"), hits[0].name);
}
