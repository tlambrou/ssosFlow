#include "reactive_session_target_rhythm_insert_test.h"

#include <list>
#include <memory>
#include <string>

#include "ardour/automation_control.h"
#include "ardour/chan_count.h"
#include "ardour/midi_track.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/reactive_rhythm_route_inserter.h"
#include "ardour/reactive_session_target.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "evoral/Parameter.h"

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

static std::shared_ptr<PluginInsert>
reactive_rhythm_insert (std::shared_ptr<Route> const& route)
{
	std::shared_ptr<PluginInsert> found;
	route->foreach_processor ([&found] (std::weak_ptr<Processor> wp) {
		if (found) {
			return;
		}

		std::shared_ptr<Processor> processor = wp.lock ();
		if (processor && ReactiveRhythmRouteInserter::is_reactive_rhythm_insert (processor)) {
			found = std::dynamic_pointer_cast<PluginInsert> (processor);
		}
	});
	return found;
}

static double
reactive_rhythm_control_value (std::shared_ptr<PluginInsert> const& insert, std::string const& label)
{
	CPPUNIT_ASSERT (insert);
	CPPUNIT_ASSERT (insert->plugin ());

	for (uint32_t i = 0; i < insert->plugin ()->parameter_count (); ++i) {
		if (!insert->plugin ()->parameter_is_control (i) || !insert->plugin ()->parameter_is_input (i)) {
			continue;
		}

		Evoral::Parameter parameter (PluginAutomation, 0, i);
		if (insert->describe_parameter (parameter) != label) {
			continue;
		}

		std::shared_ptr<AutomationControl> control = insert->automation_control (parameter);
		CPPUNIT_ASSERT (control);
		return control->get_value ();
	}

	CPPUNIT_FAIL ("missing Reactive Rhythm State MVP control: " + label);
	return 0.0;
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
ReactiveSessionTargetRhythmInsertTest::rhythmParameterActionsUpdateInsertedLuaProcControls ()
{
	std::shared_ptr<Route> route = new_midi_route (*_session);
	ReactiveSessionTarget target (*_session);
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_insert (0, error));
	std::shared_ptr<PluginInsert> insert = reactive_rhythm_insert (route);
	CPPUNIT_ASSERT (insert);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm ("density", 0.25, error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (25.0, reactive_rhythm_control_value (insert, "Density %"), 0.0001);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm ("chance", 0.5, error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (50.0, reactive_rhythm_control_value (insert, "Chance %"), 0.0001);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm ("priority_mode", 2.0, error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (2.0, reactive_rhythm_control_value (insert, "Priority"), 0.0001);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm ("priority", 3.0, error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (3.0, reactive_rhythm_control_value (insert, "Priority"), 0.0001);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm ("rotation", 7.0, error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (7.0, reactive_rhythm_control_value (insert, "Rotation"), 0.0001);
}

void
ReactiveSessionTargetRhythmInsertTest::routeScopedRhythmParameterActionsUpdateOnlyTargetRoute ()
{
	std::shared_ptr<Route> first_route = new_midi_route (*_session);
	std::shared_ptr<Route> second_route = new_midi_route (*_session);
	ReactiveSessionTarget target (*_session);
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_insert (0, error));
	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_insert (1, error));

	std::shared_ptr<PluginInsert> first_insert = reactive_rhythm_insert (first_route);
	std::shared_ptr<PluginInsert> second_insert = reactive_rhythm_insert (second_route);
	CPPUNIT_ASSERT (first_insert);
	CPPUNIT_ASSERT (second_insert);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_route (1, "density", 0.25, error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (100.0, reactive_rhythm_control_value (first_insert, "Density %"), 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (25.0, reactive_rhythm_control_value (second_insert, "Density %"), 0.0001);
}

void
ReactiveSessionTargetRhythmInsertTest::routeScopedRhythmParameterActionsReportMissingTargets ()
{
	ReactiveSessionTarget target (*_session);
	std::string error;

	CPPUNIT_ASSERT_EQUAL (false, target.rhythm_route (99, "density", 0.25, error));
	CPPUNIT_ASSERT (error.find ("missing route") != std::string::npos);

	std::shared_ptr<Route> route = new_midi_route (*_session);
	CPPUNIT_ASSERT (route);

	error.clear ();
	CPPUNIT_ASSERT_EQUAL (false, target.rhythm_route (0, "density", 0.25, error));
	CPPUNIT_ASSERT (error.find ("no Reactive Rhythm State MVP insert") != std::string::npos);

	error.clear ();
	CPPUNIT_ASSERT_EQUAL (false, target.rhythm_route (0, "swing", 0.5, error));
	CPPUNIT_ASSERT (error.find ("unknown reactive rhythm parameter") != std::string::npos);
}

void
ReactiveSessionTargetRhythmInsertTest::rhythmParameterActionsReportMissingInsertAndUnknownName ()
{
	ReactiveSessionTarget target (*_session);
	std::string error;

	CPPUNIT_ASSERT_EQUAL (false, target.rhythm ("density", 0.25, error));
	CPPUNIT_ASSERT (error.find ("no Reactive Rhythm State MVP insert") != std::string::npos);

	error.clear ();
	CPPUNIT_ASSERT_EQUAL (false, target.rhythm ("swing", 0.5, error));
	CPPUNIT_ASSERT (error.find ("unknown reactive rhythm parameter") != std::string::npos);
}

void
ReactiveSessionTargetRhythmInsertTest::rhythmInsertReportsMissingRoute ()
{
	ReactiveSessionTarget target (*_session);
	std::string error;

	CPPUNIT_ASSERT_EQUAL (false, target.rhythm_insert (99, error));
	CPPUNIT_ASSERT (error.find ("missing route") != std::string::npos);
}

void
ReactiveSessionTargetRhythmInsertTest::summarizeReactiveRhythmRoutingStatus ()
{
	std::shared_ptr<Route> first_route = new_midi_route (*_session);
	std::shared_ptr<Route> second_route = new_midi_route (*_session);
	ReactiveSessionTarget target (*_session);
	std::string error;

	std::vector<ReactiveRoutingSlotSummary> summary = target.routing_summary (8);

	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (first_route->name (), summary[0].route_name);
	CPPUNIT_ASSERT_EQUAL (false, summary[0].reactive_rhythm_insert_present);
	CPPUNIT_ASSERT_EQUAL (std::string ("no reactive rhythm insert"), summary[0].status);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].slot);
	CPPUNIT_ASSERT_EQUAL (second_route->name (), summary[1].route_name);
	CPPUNIT_ASSERT_EQUAL (false, summary[1].reactive_rhythm_insert_present);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_insert (1, error));
	CPPUNIT_ASSERT (error.empty ());

	summary = target.routing_summary (8);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (false, summary[0].reactive_rhythm_insert_present);
	CPPUNIT_ASSERT_EQUAL (std::string ("no reactive rhythm insert"), summary[0].status);
	CPPUNIT_ASSERT_EQUAL (true, summary[1].reactive_rhythm_insert_present);
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Rhythm State MVP"), summary[1].status);

	summary = target.routing_summary (1);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);

	std::string const formatted = target.format_routing_summary (8);
	CPPUNIT_ASSERT (formatted.find ("Routing:") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("0: " + first_route->name () + " - no reactive rhythm insert") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("1: " + second_route->name () + " - Reactive Rhythm State MVP") != std::string::npos);
}

void
ReactiveSessionTargetRhythmInsertTest::formatEmptyReactiveRhythmRoutingStatus ()
{
	ReactiveSessionTarget target (*_session);

	CPPUNIT_ASSERT (target.routing_summary (8).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Routing: none"), target.format_routing_summary (8));
	CPPUNIT_ASSERT_EQUAL (std::string ("Routing: none"), target.format_routing_summary (0));
}
