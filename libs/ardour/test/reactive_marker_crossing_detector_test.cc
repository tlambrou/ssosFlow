#include "reactive_marker_crossing_detector_test.h"

#include "ardour/reactive_marker_crossing_detector.h"

#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveMarkerCrossingDetectorTest);

using namespace ARDOUR;

namespace {

static ReactiveMarkerObservation
marker (std::string const& name, samplepos_t sample)
{
	return ReactiveMarkerObservation::at (name, sample);
}

} // namespace

void
ReactiveMarkerCrossingDetectorTest::reportsForwardCrossingsInTimelineOrder ()
{
	ReactiveMarkerCrossingDetector detector;
	std::vector<ReactiveMarkerObservation> markers;
	markers.push_back (marker ("B", 300));
	markers.push_back (marker ("A", 200));
	markers.push_back (marker ("C", 400));

	CPPUNIT_ASSERT (detector.poll (100, true, markers).empty ());
	std::vector<ReactiveMarkerCrossing> const hits = detector.poll (350, true, markers);

	CPPUNIT_ASSERT_EQUAL (size_t (2), hits.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("A"), hits[0].name);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (200), hits[0].sample);
	CPPUNIT_ASSERT_EQUAL (std::string ("B"), hits[1].name);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (300), hits[1].sample);
}

void
ReactiveMarkerCrossingDetectorTest::doesNotRepeatAlreadyCrossedMarkers ()
{
	ReactiveMarkerCrossingDetector detector;
	std::vector<ReactiveMarkerObservation> markers;
	markers.push_back (marker ("Drop", 240));

	CPPUNIT_ASSERT (detector.poll (100, true, markers).empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), detector.poll (250, true, markers).size ());
	CPPUNIT_ASSERT (detector.poll (300, true, markers).empty ());
}

void
ReactiveMarkerCrossingDetectorTest::resetsWhenTransportStopsOrMovesBackward ()
{
	ReactiveMarkerCrossingDetector detector;
	std::vector<ReactiveMarkerObservation> markers;
	markers.push_back (marker ("Breakdown", 200));

	CPPUNIT_ASSERT (detector.poll (100, true, markers).empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), detector.poll (250, true, markers).size ());

	CPPUNIT_ASSERT (detector.poll (100, false, markers).empty ());
	std::vector<ReactiveMarkerCrossing> replayed = detector.poll (250, true, markers);
	CPPUNIT_ASSERT_EQUAL (size_t (1), replayed.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Breakdown"), replayed[0].name);

	CPPUNIT_ASSERT (detector.poll (150, true, markers).empty ());
	replayed = detector.poll (250, true, markers);
	CPPUNIT_ASSERT_EQUAL (size_t (1), replayed.size ());
}

void
ReactiveMarkerCrossingDetectorTest::resynchronizesOnExplicitDiscontinuity ()
{
	ReactiveMarkerCrossingDetector detector;
	std::vector<ReactiveMarkerObservation> markers;
	markers.push_back (marker ("Skipped", 200));
	markers.push_back (marker ("Next", 550));

	CPPUNIT_ASSERT (detector.poll (100, true, markers).empty ());
	CPPUNIT_ASSERT (detector.poll (500, true, markers, true).empty ());

	std::vector<ReactiveMarkerCrossing> const hits = detector.poll (600, true, markers);
	CPPUNIT_ASSERT_EQUAL (size_t (1), hits.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Next"), hits[0].name);
}
