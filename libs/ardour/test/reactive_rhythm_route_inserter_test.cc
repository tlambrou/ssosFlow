#include "reactive_rhythm_route_inserter_test.h"

#include <list>
#include <memory>

#include "ardour/chan_count.h"
#include "ardour/midi_track.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/reactive_rhythm_route_inserter.h"
#include "ardour/route.h"
#include "ardour/session.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmRouteInserterTest);

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
		"Reactive Rhythm Test MIDI",
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
ReactiveRhythmRouteInserterTest::insertsReactiveRhythmLuaProcIntoMidiTrack ()
{
	std::shared_ptr<Route> route = new_midi_route (*_session);

	ReactiveRhythmRouteInsertionResult result = ReactiveRhythmRouteInserter::ensure_inserted (*_session, route);

	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmRouteInsertionStatus::Inserted, result.status);
	CPPUNIT_ASSERT (result.insert);
	CPPUNIT_ASSERT (result.insert->active ());
	CPPUNIT_ASSERT (result.insert->plugin ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Rhythm State MVP"), std::string (result.insert->plugin ()->name ()));
	CPPUNIT_ASSERT_EQUAL (size_t (1), reactive_rhythm_insert_count (route));

	ChanCount configured_in;
	ChanCount configured_out;
	result.insert->configured_io (configured_in, configured_out);
	CPPUNIT_ASSERT_EQUAL (uint32_t (1), configured_in.n_midi ());
	CPPUNIT_ASSERT_EQUAL (uint32_t (1), configured_out.n_midi ());
}

void
ReactiveRhythmRouteInserterTest::repeatedInsertionReturnsExistingProcessor ()
{
	std::shared_ptr<Route> route = new_midi_route (*_session);

	ReactiveRhythmRouteInsertionResult first = ReactiveRhythmRouteInserter::ensure_inserted (*_session, route);
	ReactiveRhythmRouteInsertionResult second = ReactiveRhythmRouteInserter::ensure_inserted (*_session, route);

	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmRouteInsertionStatus::Inserted, first.status);
	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmRouteInsertionStatus::AlreadyPresent, second.status);
	CPPUNIT_ASSERT (first.insert);
	CPPUNIT_ASSERT (second.insert);
	CPPUNIT_ASSERT (first.insert == second.insert);
	CPPUNIT_ASSERT_EQUAL (size_t (1), reactive_rhythm_insert_count (route));
}
