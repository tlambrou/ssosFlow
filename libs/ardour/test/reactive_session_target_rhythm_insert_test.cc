#include "reactive_session_target_rhythm_insert_test.h"

#include <list>
#include <memory>
#include <string>

#include "ardour/automation_control.h"
#include "ardour/chan_count.h"
#include "ardour/lua_api.h"
#include "ardour/midi_track.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/reactive_rhythm_route_inserter.h"
#include "ardour/reactive_session_target.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/track.h"

#include "evoral/Parameter.h"
#include "pbd/controllable.h"

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

static std::shared_ptr<Route>
new_trigger_visible_midi_route (Session& session, std::string const& name)
{
	std::list<std::shared_ptr<MidiTrack> > tracks = session.new_midi_track (
		ChanCount (DataType::MIDI, 1),
		ChanCount (DataType::MIDI, 1),
		false,
		PluginInfoPtr (),
		nullptr,
		std::shared_ptr<RouteGroup> (),
		1,
		name,
		PresentationInfo::max_order,
		Normal,
		false,
		true);

	CPPUNIT_ASSERT_EQUAL (size_t (1), tracks.size ());
	return tracks.front ();
}

static std::shared_ptr<Route>
new_audio_bus (Session& session, std::string const& name)
{
	RouteList routes = session.new_audio_route (
		1,
		2,
		std::shared_ptr<RouteGroup> (),
		1,
		name,
		PresentationInfo::AudioBus,
		PresentationInfo::max_order);

	CPPUNIT_ASSERT_EQUAL (size_t (1), routes.size ());
	return routes.front ();
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
	CPPUNIT_ASSERT_EQUAL (false, summary[1].reactive_rhythm_values_present);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_insert (1, error));
	CPPUNIT_ASSERT (error.empty ());

	summary = target.routing_summary (8);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (false, summary[0].reactive_rhythm_insert_present);
	CPPUNIT_ASSERT_EQUAL (std::string ("no reactive rhythm insert"), summary[0].status);
	CPPUNIT_ASSERT_EQUAL (true, summary[1].reactive_rhythm_insert_present);
	CPPUNIT_ASSERT_EQUAL (true, summary[1].reactive_rhythm_values_present);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, summary[1].rhythm_density, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, summary[1].rhythm_chance, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, summary[1].rhythm_priority, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, summary[1].rhythm_rotation, 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Rhythm State MVP density=1.00 chance=1.00 priority=0 rotation=0"), summary[1].status);

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_route (1, "density", 0.25, error));
	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_route (1, "chance", 0.5, error));
	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_route (1, "priority_mode", 2.0, error));
	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_route (1, "rotation", 4.0, error));

	summary = target.routing_summary (8);
	CPPUNIT_ASSERT_EQUAL (true, summary[1].reactive_rhythm_values_present);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.25, summary[1].rhythm_density, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.5, summary[1].rhythm_chance, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (2.0, summary[1].rhythm_priority, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (4.0, summary[1].rhythm_rotation, 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Rhythm State MVP density=0.25 chance=0.50 priority=2 rotation=4"), summary[1].status);

	summary = target.routing_summary (1);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);

	std::string const formatted = target.format_routing_summary (8);
	CPPUNIT_ASSERT (formatted.find ("Routing:") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("0: " + first_route->name () + " - no reactive rhythm insert") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("1: " + second_route->name () + " - Reactive Rhythm State MVP density=0.25 chance=0.50 priority=2 rotation=4") != std::string::npos);
}

void
ReactiveSessionTargetRhythmInsertTest::formatEmptyReactiveRhythmRoutingStatus ()
{
	ReactiveSessionTarget target (*_session);

	CPPUNIT_ASSERT (target.routing_summary (8).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Routing: none"), target.format_routing_summary (8));
	CPPUNIT_ASSERT_EQUAL (std::string ("Routing: none"), target.format_routing_summary (0));
}

void
ReactiveSessionTargetRhythmInsertTest::summarizeReactiveTriggerSlotStatus ()
{
	std::shared_ptr<Route> route = new_trigger_visible_midi_route (*_session, "Reactive Trigger Lane");
	ReactiveSessionTarget target (*_session);
	std::string error;

	CPPUNIT_ASSERT (ARDOUR::LuaAPI::ensure_session_midi_trigger_region (
		_session,
		"Reactive Trigger Lane",
		0,
		"Reactive Cue 0 Reset",
		Temporal::timecnt_t (48000 * 4)));
	CPPUNIT_ASSERT_EQUAL (true, target.trigger_probability (0, 0, 0.65, error));

	std::vector<ReactiveTriggerSlotSummary> summary = target.trigger_slot_summary (4, 2);

	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].route);
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (route->name (), summary[0].route_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Cue 0 Reset"), summary[0].region_name);
	CPPUNIT_ASSERT_EQUAL (true, summary[0].has_triggerbox);
	CPPUNIT_ASSERT_EQUAL (true, summary[0].populated);
	CPPUNIT_ASSERT_EQUAL (true, summary[0].playable);
	CPPUNIT_ASSERT_EQUAL (65, summary[0].follow_probability);
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Cue 0 Reset playable follow=65%"), summary[0].status);

	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[1].route);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].slot);
	CPPUNIT_ASSERT_EQUAL (route->name (), summary[1].route_name);
	CPPUNIT_ASSERT (summary[1].region_name.empty ());
	CPPUNIT_ASSERT_EQUAL (true, summary[1].has_triggerbox);
	CPPUNIT_ASSERT_EQUAL (false, summary[1].populated);
	CPPUNIT_ASSERT_EQUAL (false, summary[1].playable);
	CPPUNIT_ASSERT_EQUAL (0, summary[1].follow_probability);
	CPPUNIT_ASSERT_EQUAL (std::string ("empty"), summary[1].status);

	std::string const formatted = target.format_trigger_slot_summary (4, 2);
	CPPUNIT_ASSERT (formatted.find ("Trigger Slots:") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("0/0: Reactive Trigger Lane - Reactive Cue 0 Reset playable follow=65%") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("0/1: Reactive Trigger Lane - empty") != std::string::npos);
}

void
ReactiveSessionTargetRhythmInsertTest::formatEmptyReactiveTriggerSlotStatus ()
{
	ReactiveSessionTarget target (*_session);

	CPPUNIT_ASSERT (target.trigger_slot_summary (4, 4).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Trigger Slots: none"), target.format_trigger_slot_summary (4, 4));
	CPPUNIT_ASSERT_EQUAL (std::string ("Trigger Slots: none"), target.format_trigger_slot_summary (0, 4));
	CPPUNIT_ASSERT_EQUAL (std::string ("Trigger Slots: none"), target.format_trigger_slot_summary (4, 0));
}

void
ReactiveSessionTargetRhythmInsertTest::summarizeReactiveSessionClockState ()
{
	ReactiveSessionTarget target (*_session);

	ReactiveSessionStateSummary const summary = target.session_state_summary ();

	CPPUNIT_ASSERT_EQUAL (true, summary.session_loaded);
	CPPUNIT_ASSERT_EQUAL (false, summary.transport_rolling);
	CPPUNIT_ASSERT_EQUAL (Temporal::samplepos_t (0), summary.transport_sample);
	CPPUNIT_ASSERT_EQUAL (1, summary.bbt.bars);
	CPPUNIT_ASSERT_EQUAL (1, summary.bbt.beats);
	CPPUNIT_ASSERT_EQUAL (0, summary.bbt.ticks);
	CPPUNIT_ASSERT (summary.tempo_quarter_notes_per_minute > 0.0);
	CPPUNIT_ASSERT (summary.meter_divisions_per_bar > 0);
	CPPUNIT_ASSERT (summary.meter_note_value > 0);
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary.route_count);
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary.trigger_route_count);
	CPPUNIT_ASSERT (summary.status.find ("stopped @ 1|1|0") != std::string::npos);
	CPPUNIT_ASSERT (summary.status.find ("routes=0 trigger-routes=0") != std::string::npos);
	CPPUNIT_ASSERT (target.format_session_state_summary ().find ("Session State: stopped @ 1|1|0") != std::string::npos);
}

void
ReactiveSessionTargetRhythmInsertTest::sessionClockStateCountsTriggerVisibleRoutes ()
{
	std::shared_ptr<Route> ordinary_route = new_midi_route (*_session);
	std::shared_ptr<Route> trigger_route = new_trigger_visible_midi_route (*_session, "Reactive Trigger Lane");
	ReactiveSessionTarget target (*_session);

	CPPUNIT_ASSERT (ordinary_route);
	CPPUNIT_ASSERT (trigger_route);

	ReactiveSessionStateSummary const summary = target.session_state_summary ();

	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.route_count);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.trigger_route_count);
	CPPUNIT_ASSERT (summary.status.find ("routes=2 trigger-routes=1") != std::string::npos);
	CPPUNIT_ASSERT (target.format_session_state_summary ().find ("routes=2 trigger-routes=1") != std::string::npos);
}

void
ReactiveSessionTargetRhythmInsertTest::formatEmptyReactiveTrackState ()
{
	ReactiveSessionTarget target (*_session);

	CPPUNIT_ASSERT (target.track_state_summary (8).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Track State: none"), target.format_track_state_summary (8));
	CPPUNIT_ASSERT_EQUAL (std::string ("Track State: none"), target.format_track_state_summary (0));
}

void
ReactiveSessionTargetRhythmInsertTest::summarizeReactiveTrackStateForRoutes ()
{
	std::shared_ptr<Route> midi_route = new_midi_route (*_session);
	std::shared_ptr<Route> solo_bus = new_audio_bus (*_session, "Reactive Solo Bus");
	std::shared_ptr<Route> inactive_bus = new_audio_bus (*_session, "Reactive Inactive Bus");
	ReactiveSessionTarget target (*_session);

	CPPUNIT_ASSERT (midi_route);
	CPPUNIT_ASSERT (solo_bus);
	CPPUNIT_ASSERT (inactive_bus);

	std::shared_ptr<Track> midi_track = std::dynamic_pointer_cast<Track> (midi_route);
	CPPUNIT_ASSERT (midi_track);
	CPPUNIT_ASSERT (midi_track->rec_enable_control ());

	midi_route->mute_control ()->set_mute_points (MuteMaster::AllPoints);
	midi_route->mute_master ()->set_muted_by_self (true);
	midi_route->gain_control ()->set_value (0.5, PBD::Controllable::NoGroup);
	midi_track->rec_enable_control ()->set_value (1.0, PBD::Controllable::NoGroup);
	solo_bus->solo_control ()->mod_solo_by_others_downstream (1);
	inactive_bus->set_active (false, this);

	std::vector<ReactiveTrackStateSummary> const summary = target.track_state_summary (8);

	CPPUNIT_ASSERT_EQUAL (size_t (3), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (midi_route->name (), summary[0].route_name);
	CPPUNIT_ASSERT_EQUAL (true, summary[0].active);
	CPPUNIT_ASSERT_EQUAL (true, summary[0].muted);
	CPPUNIT_ASSERT_EQUAL (false, summary[0].soloed);
	CPPUNIT_ASSERT_EQUAL (true, summary[0].record_enable_available);
	CPPUNIT_ASSERT_EQUAL (false, summary[0].record_enabled);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.5, summary[0].gain, 0.0001);
	CPPUNIT_ASSERT (summary[0].status.find ("active muted unsoloed rec-off gain=0.50") != std::string::npos);

	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].slot);
	CPPUNIT_ASSERT_EQUAL (solo_bus->name (), summary[1].route_name);
	CPPUNIT_ASSERT_EQUAL (true, summary[1].soloed);
	CPPUNIT_ASSERT_EQUAL (false, summary[1].record_enable_available);
	CPPUNIT_ASSERT_EQUAL (false, summary[1].record_enabled);
	CPPUNIT_ASSERT (summary[1].status.find ("rec=-") != std::string::npos);

	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[2].slot);
	CPPUNIT_ASSERT_EQUAL (inactive_bus->name (), summary[2].route_name);
	CPPUNIT_ASSERT_EQUAL (false, summary[2].active);
	CPPUNIT_ASSERT_EQUAL (false, summary[2].record_enable_available);
	CPPUNIT_ASSERT (summary[2].status.find ("inactive") != std::string::npos);

	std::string const formatted = target.format_track_state_summary (8);
	CPPUNIT_ASSERT (formatted.find ("Track State:") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("0: " + midi_route->name () + " - active muted unsoloed rec-off gain=0.50") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("1: " + solo_bus->name () + " - active unmuted soloed rec=-") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("2: " + inactive_bus->name () + " - inactive") != std::string::npos);
}
