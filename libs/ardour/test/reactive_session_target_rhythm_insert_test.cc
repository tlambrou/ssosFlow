#include "reactive_session_target_rhythm_insert_test.h"

#include <list>
#include <memory>
#include <string>

#include "ardour/chan_count.h"
#include "ardour/midi_track.h"
#include "ardour/plugin.h"
#include "ardour/processor.h"
#include "ardour/reactive_rhythm_route_inserter.h"
#include "ardour/reactive_session_target.h"
#include "ardour/route.h"
#include "ardour/session.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveSessionTargetRhythmInsertTest);

using namespace ARDOUR;

namespace {

static std::shared_ptr<Route>
new_midi_route (Session& session)
{
	std::list<std::shared_ptr<MidiTrack> > tracks = session.new_midi_track (
		ChanCount (DataType::MIDI, 1),
		ChanCount (DataType::MIDI, 1),
		false,
		PluginInfoPtr (),
		nullptr,
		std::shared_ptr<RouteGroup> (),
		1,
		"Reactive Session Rhythm MIDI",
		PresentationInfo::max_order,
		Normal,
		false);

	CPPUNIT_ASSERT_EQUAL (size_t (1), tracks.size ());
	return tracks.front ();
}

static size_t
reactive_rhythm_insert_count (std::shared_ptr<Route> const& route)
{
	size_t count = 0;
	route->foreach_processor ([&count] (std::weak_ptr<Processor> wp) {
		std::shared_ptr<Processor> processor = wp.lock ();
		if (processor && ReactiveRhythmRouteInserter::is_reactive_rhythm_insert (processor)) {
			++count;
		}
	});
	return count;
}

} // namespace

void
ReactiveSessionTargetRhythmInsertTest::rhythmInsertAddsLuaProcToRemoteRoute ()
{
	std::shared_ptr<Route> route = new_midi_route (*_session);
	ReactiveSessionTarget target (*_session);
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_insert (0, error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), reactive_rhythm_insert_count (route));

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_insert (0, error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), reactive_rhythm_insert_count (route));
}

void
ReactiveSessionTargetRhythmInsertTest::rhythmInsertReportsMissingRoute ()
{
	ReactiveSessionTarget target (*_session);
	std::string error;

	CPPUNIT_ASSERT_EQUAL (false, target.rhythm_insert (99, error));
	CPPUNIT_ASSERT (error.find ("missing route") != std::string::npos);
}
