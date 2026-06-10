#include "ardour/reactive_session_target.h"

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>

#include "ardour/automation_control.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/reactive_rhythm_route_inserter.h"
#include "ardour/region.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/triggerbox.h"

#include "evoral/Parameter.h"

#include "pbd/controllable.h"

#include "temporal/tempo.h"

using namespace ARDOUR;

namespace {

static bool
has_offset (Temporal::BBT_Offset const& after)
{
	return after.bars != 0 || after.beats != 0 || after.ticks != 0;
}

static std::string
compose_failure (std::string const& prefix, int value)
{
	std::ostringstream msg;
	msg << prefix << value << " failed";
	return msg.str ();
}

static bool
rhythm_parameter_target (std::string const& name, std::string& label, double& scale)
{
	scale = 1.0;

	if (name == "density") {
		label = "Density %";
		scale = 100.0;
		return true;
	}

	if (name == "chance") {
		label = "Chance %";
		scale = 100.0;
		return true;
	}

	if (name == "priority" || name == "priority_mode") {
		label = "Priority";
		return true;
	}

	if (name == "rotation") {
		label = "Rotation";
		return true;
	}

	return false;
}

static double
clamp_to_control_range (std::shared_ptr<AutomationControl> const& control, double value)
{
	if (!control) {
		return value;
	}

	if (value < control->lower ()) {
		return control->lower ();
	}

	if (value > control->upper ()) {
		return control->upper ();
	}

	return value;
}

static bool
set_reactive_rhythm_control (
	std::shared_ptr<PluginInsert> const& insert,
	std::string const& label,
	double value,
	std::string& error)
{
	if (!insert || !insert->plugin ()) {
		error = "reactive rhythm insert is unavailable";
		return false;
	}

	for (uint32_t i = 0; i < insert->plugin ()->parameter_count (); ++i) {
		if (!insert->plugin ()->parameter_is_control (i) || !insert->plugin ()->parameter_is_input (i)) {
			continue;
		}

		Evoral::Parameter parameter (PluginAutomation, 0, i);
		if (insert->describe_parameter (parameter) != label) {
			continue;
		}

		std::shared_ptr<AutomationControl> control = insert->automation_control (parameter);
		if (!control) {
			error = "reactive rhythm control " + label + " is unavailable";
			return false;
		}

		control->set_value (clamp_to_control_range (control, value), PBD::Controllable::NoGroup);
		error.clear ();
		return true;
	}

	error = "reactive rhythm control " + label + " was not found";
	return false;
}

static size_t
set_reactive_rhythm_route_controls (
	std::shared_ptr<Route> const& route,
	std::string const& label,
	double value,
	std::string& error)
{
	size_t updated = 0;
	if (!route) {
		return updated;
	}

	route->foreach_processor ([&updated, &label, value, &error] (std::weak_ptr<Processor> wp) {
		std::shared_ptr<Processor> processor = wp.lock ();
		if (!processor || !ReactiveRhythmRouteInserter::is_reactive_rhythm_insert (processor)) {
			return;
		}

		std::shared_ptr<PluginInsert> insert = std::dynamic_pointer_cast<PluginInsert> (processor);
		if (set_reactive_rhythm_control (insert, label, value, error)) {
			++updated;
		}
	});

	return updated;
}

static bool
route_has_reactive_rhythm_insert (std::shared_ptr<Route> const& route)
{
	bool found = false;
	if (!route) {
		return false;
	}

	route->foreach_processor ([&found] (std::weak_ptr<Processor> wp) {
		if (found) {
			return;
		}

		std::shared_ptr<Processor> processor = wp.lock ();
		if (processor && ReactiveRhythmRouteInserter::is_reactive_rhythm_insert (processor)) {
			found = true;
		}
	});

	return found;
}

static std::shared_ptr<PluginInsert>
reactive_rhythm_insert_for_route (std::shared_ptr<Route> const& route)
{
	std::shared_ptr<PluginInsert> found;
	if (!route) {
		return found;
	}

	route->foreach_processor ([&found] (std::weak_ptr<Processor> wp) {
		if (found) {
			return;
		}

		std::shared_ptr<Processor> processor = wp.lock ();
		if (!processor || !ReactiveRhythmRouteInserter::is_reactive_rhythm_insert (processor)) {
			return;
		}

		found = std::dynamic_pointer_cast<PluginInsert> (processor);
	});

	return found;
}

static bool
reactive_rhythm_control_value (std::shared_ptr<PluginInsert> const& insert, std::string const& label, double& value)
{
	if (!insert || !insert->plugin ()) {
		return false;
	}

	for (uint32_t i = 0; i < insert->plugin ()->parameter_count (); ++i) {
		if (!insert->plugin ()->parameter_is_control (i) || !insert->plugin ()->parameter_is_input (i)) {
			continue;
		}

		Evoral::Parameter parameter (PluginAutomation, 0, i);
		if (insert->describe_parameter (parameter) != label) {
			continue;
		}

		std::shared_ptr<AutomationControl> control = insert->automation_control (parameter);
		if (!control) {
			return false;
		}

		value = control->get_value ();
		return true;
	}

	return false;
}

static std::string
format_decimal_value (double value)
{
	std::ostringstream text;
	text << std::fixed << std::setprecision (2) << value;
	return text.str ();
}

static std::string
format_integer_value (double value)
{
	std::ostringstream text;
	text << std::fixed << std::setprecision (0) << value;
	return text.str ();
}

static std::string
format_bbt_time (Temporal::BBT_Time const& bbt)
{
	std::ostringstream text;
	text << bbt.bars << "|" << bbt.beats << "|" << bbt.ticks;
	return text.str ();
}

static bool
populate_reactive_rhythm_route_values (std::shared_ptr<Route> const& route, ReactiveRoutingSlotSummary& row)
{
	std::shared_ptr<PluginInsert> insert = reactive_rhythm_insert_for_route (route);
	if (!insert) {
		return false;
	}

	double density = 0.0;
	double chance = 0.0;
	double priority = 0.0;
	double rotation = 0.0;

	if (!reactive_rhythm_control_value (insert, "Density %", density) ||
	    !reactive_rhythm_control_value (insert, "Chance %", chance) ||
	    !reactive_rhythm_control_value (insert, "Priority", priority) ||
	    !reactive_rhythm_control_value (insert, "Rotation", rotation)) {
		return false;
	}

	row.reactive_rhythm_values_present = true;
	row.rhythm_density = density / 100.0;
	row.rhythm_chance = chance / 100.0;
	row.rhythm_priority = priority;
	row.rhythm_rotation = rotation;
	return true;
}

static std::string
reactive_rhythm_route_status (ReactiveRoutingSlotSummary const& row)
{
	if (!row.reactive_rhythm_values_present) {
		return ReactiveRhythmRouteInserter::lua_proc_name ();
	}

	std::ostringstream status;
	status << ReactiveRhythmRouteInserter::lua_proc_name ()
	       << " density=" << format_decimal_value (row.rhythm_density)
	       << " chance=" << format_decimal_value (row.rhythm_chance)
	       << " priority=" << format_integer_value (row.rhythm_priority)
	       << " rotation=" << format_integer_value (row.rhythm_rotation);
	return status.str ();
}

static std::vector<std::shared_ptr<Route> >
trigger_visible_routes (Session const& session, size_t max_routes)
{
	std::vector<std::shared_ptr<Route> > routes;
	if (max_routes == 0) {
		return routes;
	}

	routes.reserve (max_routes);

	StripableList stripables;
	session.get_stripables (stripables);
	stripables.sort (Stripable::Sorter ());

	for (StripableList::const_iterator i = stripables.begin (); i != stripables.end (); ++i) {
		std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (*i);
		if (!route || !route->triggerbox ()) {
			continue;
		}

		if (!route->presentation_info ().trigger_track ()) {
			continue;
		}

		routes.push_back (route);
		if (routes.size () >= max_routes) {
			break;
		}
	}

	return routes;
}

static size_t
trigger_visible_route_count (Session const& session)
{
	size_t count = 0;

	StripableList stripables;
	session.get_stripables (stripables);
	stripables.sort (Stripable::Sorter ());

	for (StripableList::const_iterator i = stripables.begin (); i != stripables.end (); ++i) {
		std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (*i);
		if (!route || !route->triggerbox ()) {
			continue;
		}

		if (!route->presentation_info ().trigger_track ()) {
			continue;
		}

		++count;
	}

	return count;
}

static std::string
trigger_slot_status (ReactiveTriggerSlotSummary const& row)
{
	if (!row.populated) {
		return "empty";
	}

	std::ostringstream status;
	if (!row.region_name.empty ()) {
		status << row.region_name;
	} else {
		status << "loaded trigger";
	}

	status << (row.playable ? " playable" : " loaded")
	       << " follow=" << row.follow_probability << "%";
	return status.str ();
}

static std::string
session_state_status (ReactiveSessionStateSummary const& row)
{
	if (!row.session_loaded) {
		return "no session";
	}

	std::ostringstream status;
	status << (row.transport_rolling ? "rolling" : "stopped")
	       << " @ " << format_bbt_time (row.bbt)
	       << " speed=" << format_decimal_value (row.transport_speed)
	       << " record=" << (row.record_enabled ? "on" : "off")
	       << " loop=" << (row.loop_enabled ? "on" : "off")
	       << " locate=" << (row.locate_pending ? "pending" : "idle")
	       << " tempo=" << format_decimal_value (row.tempo_quarter_notes_per_minute)
	       << " meter=" << row.meter_divisions_per_bar << "/" << row.meter_note_value
	       << " sample=" << row.transport_sample
	       << " routes=" << row.route_count
	       << " trigger-routes=" << row.trigger_route_count;
	return status.str ();
}

static std::string
track_state_status (ReactiveTrackStateSummary const& row)
{
	std::ostringstream status;
	status << (row.active ? "active" : "inactive")
	       << " " << (row.muted ? "muted" : "unmuted")
	       << " " << (row.soloed ? "soloed" : "unsoloed")
	       << " ";

	if (!row.record_enable_available) {
		status << "rec=-";
	} else if (row.record_enabled) {
		status << "rec";
	} else {
		status << "rec-off";
	}

	status << " gain=" << format_decimal_value (row.gain);
	return status.str ();
}

} // namespace

ReactiveSessionTarget::ReactiveSessionTarget ()
	: _session (0)
{
}

ReactiveSessionTarget::ReactiveSessionTarget (Session& session)
	: _session (&session)
{
}

bool
ReactiveSessionTarget::cue (int row, std::string& error)
{
	session_trigger_cue_row (row);
	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::trigger (int route, int row, std::string& error)
{
	if (!session_bang_trigger_at (route, row, 1.0f)) {
		std::ostringstream msg;
		msg << "trigger route " << route << " row " << row << " failed";
		error = msg.str ();
		return false;
	}

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::trigger_probability (int route, int row, double value, std::string& error)
{
	if (route < 0) {
		error = "trigger probability route index must be non-negative";
		return false;
	}

	if (row < 0) {
		error = "trigger probability row index must be non-negative";
		return false;
	}

	if (value < 0.0 || value > 1.0) {
		error = "trigger probability value must be between 0 and 1";
		return false;
	}

	int const probability = static_cast<int> (std::lround (value * 100.0));
	if (!session_set_trigger_follow_probability (route, row, probability, error)) {
		if (error.empty ()) {
			std::ostringstream msg;
			msg << "trigger probability route " << route << " row " << row << " failed";
			error = msg.str ();
		}
		return false;
	}

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::trigger_stop (int route, std::string& error)
{
	if (!session_stop_triggers_at (route, error)) {
		if (error.empty ()) {
			error = compose_failure ("trigger stop route ", route);
		}
		return false;
	}

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::stop_all (std::string& error)
{
	session_trigger_stop_all (false);
	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::transport_play (std::string& error)
{
	session_request_transport_speed (1.0);
	session_request_roll ();
	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::transport_stop (Temporal::BBT_Offset const& after, std::string& error)
{
	if (has_offset (after)) {
		error = "delayed transport stop is not supported until the reactive scheduler exists";
		return false;
	}

	session_request_stop ();
	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::scene_apply (int index, std::string& error)
{
	if (!session_apply_nth_mixer_scene (index)) {
		error = compose_failure ("scene apply ", index);
		return false;
	}

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::scene_store (int index, std::string& error)
{
	session_store_nth_mixer_scene (index);
	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::macro (std::string const& name, double value, Temporal::BBT_Offset const& ramp, std::string& error)
{
	(void) name;
	(void) value;
	(void) ramp;

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::state (std::string const& name, std::string const& value, std::string& error)
{
	(void) name;
	(void) value;

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::harmony (std::string const& name, std::string const& value, std::string& error)
{
	(void) name;
	(void) value;

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::rhythm (std::string const& name, double value, std::string& error)
{
	if (!_session) {
		error = "reactive session target has no session";
		return false;
	}

	std::string label;
	double scale = 1.0;
	if (!rhythm_parameter_target (name, label, scale)) {
		error = "unknown reactive rhythm parameter: " + name;
		return false;
	}

	size_t updated = 0;
	std::shared_ptr<RouteList const> routes = _session->get_routes ();
	for (RouteList::const_iterator route = routes->begin (); route != routes->end (); ++route) {
		updated += set_reactive_rhythm_route_controls (*route, label, value * scale, error);
	}

	if (updated == 0) {
		if (error.empty ()) {
			error = "no Reactive Rhythm State MVP insert is available";
		}
		return false;
	}

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::rhythm_route (int route, std::string const& name, double value, std::string& error)
{
	if (!_session) {
		error = "reactive session target has no session";
		return false;
	}

	if (route < 0) {
		error = "reactive rhythm route index must be non-negative";
		return false;
	}

	std::string label;
	double scale = 1.0;
	if (!rhythm_parameter_target (name, label, scale)) {
		error = "unknown reactive rhythm parameter: " + name;
		return false;
	}

	std::shared_ptr<Route> target = _session->get_remote_nth_route (static_cast<PresentationInfo::order_t> (route));
	if (!target) {
		std::ostringstream msg;
		msg << "missing route for reactive rhythm route " << route;
		error = msg.str ();
		return false;
	}

	size_t const updated = set_reactive_rhythm_route_controls (target, label, value * scale, error);
	if (updated == 0) {
		if (error.empty ()) {
			std::ostringstream msg;
			msg << "no Reactive Rhythm State MVP insert is available on route " << route;
			error = msg.str ();
		}
		return false;
	}

	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::rhythm_insert (int route, std::string& error)
{
	return session_insert_reactive_rhythm (route, error);
}

std::vector<ReactiveRoutingSlotSummary>
ReactiveSessionTarget::routing_summary (size_t max_routes) const
{
	std::vector<ReactiveRoutingSlotSummary> summary;
	if (!_session || max_routes == 0) {
		return summary;
	}

	summary.reserve (max_routes);
	for (size_t slot = 0; slot < max_routes; ++slot) {
		std::shared_ptr<Route> route = _session->get_remote_nth_route (static_cast<PresentationInfo::order_t> (slot));
		if (!route) {
			break;
		}

		ReactiveRoutingSlotSummary row;
		row.slot = slot;
		row.route_name = route->name ();
		row.reactive_rhythm_insert_present = route_has_reactive_rhythm_insert (route);
		if (row.reactive_rhythm_insert_present) {
			populate_reactive_rhythm_route_values (route, row);
			row.status = reactive_rhythm_route_status (row);
		} else {
			row.status = "no reactive rhythm insert";
		}
		summary.push_back (row);
	}

	return summary;
}

std::string
ReactiveSessionTarget::format_routing_summary (size_t max_routes) const
{
	std::vector<ReactiveRoutingSlotSummary> const summary = routing_summary (max_routes);
	if (summary.empty ()) {
		return "Routing: none";
	}

	std::ostringstream text;
	text << "Routing:";
	for (std::vector<ReactiveRoutingSlotSummary>::const_iterator i = summary.begin (); i != summary.end (); ++i) {
		text << "\n  " << i->slot << ": " << i->route_name << " - " << i->status;
	}

	return text.str ();
}

std::vector<ReactiveTriggerSlotSummary>
ReactiveSessionTarget::trigger_slot_summary (size_t max_routes, size_t max_slots_per_route) const
{
	std::vector<ReactiveTriggerSlotSummary> summary;
	if (!_session || max_routes == 0 || max_slots_per_route == 0) {
		return summary;
	}

	std::vector<std::shared_ptr<Route> > const routes = trigger_visible_routes (*_session, max_routes);
	summary.reserve (routes.size () * max_slots_per_route);

	for (size_t route_index = 0; route_index < routes.size (); ++route_index) {
		std::shared_ptr<Route> const& route = routes[route_index];
		std::shared_ptr<TriggerBox> triggerbox = route->triggerbox ();
		if (!triggerbox) {
			continue;
		}

		for (size_t slot = 0; slot < max_slots_per_route; ++slot) {
			ReactiveTriggerSlotSummary row;
			row.route = route_index;
			row.slot = slot;
			row.route_name = route->name ();
			row.has_triggerbox = true;

			TriggerPtr trigger = triggerbox->trigger (static_cast<TriggerBox::Triggers::size_type> (slot));
			if (trigger) {
				std::shared_ptr<Region> region = trigger->the_region ();
				if (region) {
					row.region_name = region->name ();
				}
				row.playable = trigger->playable ();
				row.populated = region || row.playable;
				row.follow_probability = trigger->follow_action_probability ();
			}

			row.status = trigger_slot_status (row);
			summary.push_back (row);
		}
	}

	return summary;
}

std::string
ReactiveSessionTarget::format_trigger_slot_summary (size_t max_routes, size_t max_slots_per_route) const
{
	std::vector<ReactiveTriggerSlotSummary> const summary = trigger_slot_summary (max_routes, max_slots_per_route);
	if (summary.empty ()) {
		return "Trigger Slots: none";
	}

	std::ostringstream text;
	text << "Trigger Slots:";
	for (std::vector<ReactiveTriggerSlotSummary>::const_iterator i = summary.begin (); i != summary.end (); ++i) {
		text << "\n  " << i->route << "/" << i->slot << ": " << i->route_name << " - " << i->status;
	}

	return text.str ();
}

ReactiveSessionStateSummary
ReactiveSessionTarget::session_state_summary () const
{
	ReactiveSessionStateSummary row;
	if (!_session) {
		row.status = session_state_status (row);
		return row;
	}

	row.session_loaded = true;
	row.transport_rolling = _session->transport_state_rolling ();
	row.transport_speed = _session->transport_speed ();
	row.record_enabled = _session->get_record_enabled ();
	row.loop_enabled = _session->get_play_loop ();
	row.locate_pending = _session->locate_pending ();
	row.transport_sample = _session->transport_sample ();
	row.route_count = _session->nroutes ();
	row.trigger_route_count = trigger_visible_route_count (*_session);

	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use ());
	Temporal::timepos_t const position (row.transport_sample);
	row.bbt = Temporal::BBT_Time (tmap->bbt_at (position));
	row.tempo_quarter_notes_per_minute = tmap->quarters_per_minute_at (position);
	Temporal::TempoMetric metric (tmap->metric_at (position));
	row.meter_divisions_per_bar = metric.meter ().divisions_per_bar ();
	row.meter_note_value = metric.meter ().note_value ();

	row.status = session_state_status (row);
	return row;
}

std::string
ReactiveSessionTarget::format_session_state_summary () const
{
	ReactiveSessionStateSummary const summary = session_state_summary ();
	if (!summary.session_loaded) {
		return "Session State: none";
	}

	return "Session State: " + summary.status;
}

std::vector<ReactiveTrackStateSummary>
ReactiveSessionTarget::track_state_summary (size_t max_routes) const
{
	std::vector<ReactiveTrackStateSummary> summary;
	if (!_session || max_routes == 0) {
		return summary;
	}

	summary.reserve (max_routes);
	for (size_t slot = 0; slot < max_routes; ++slot) {
		std::shared_ptr<Route> route = _session->get_remote_nth_route (static_cast<PresentationInfo::order_t> (slot));
		if (!route) {
			break;
		}

		ReactiveTrackStateSummary row;
		row.slot = slot;
		row.route_name = route->name ();
		row.active = route->active ();
		row.muted = route->muted ();
		row.soloed = route->soloed ();
		if (route->gain_control ()) {
			row.gain = route->gain_control ()->get_value ();
		}

		std::shared_ptr<Track> track = std::dynamic_pointer_cast<Track> (route);
		if (track && track->rec_enable_control ()) {
			row.record_enable_available = true;
			row.record_enabled = track->rec_enable_control ()->get_value ();
		}

		row.status = track_state_status (row);
		summary.push_back (row);
	}

	return summary;
}

std::string
ReactiveSessionTarget::format_track_state_summary (size_t max_routes) const
{
	std::vector<ReactiveTrackStateSummary> const summary = track_state_summary (max_routes);
	if (summary.empty ()) {
		return "Track State: none";
	}

	std::ostringstream text;
	text << "Track State:";
	for (std::vector<ReactiveTrackStateSummary>::const_iterator i = summary.begin (); i != summary.end (); ++i) {
		text << "\n  " << i->slot << ": " << i->route_name << " - " << i->status;
	}

	return text.str ();
}

void
ReactiveSessionTarget::session_trigger_cue_row (int row)
{
	if (_session) {
		_session->trigger_cue_row (row);
	}
}

bool
ReactiveSessionTarget::session_bang_trigger_at (int route, int row, float velocity)
{
	return _session && _session->bang_trigger_at (route, row, velocity);
}

bool
ReactiveSessionTarget::session_set_trigger_follow_probability (int route, int row, int probability, std::string& error)
{
	if (!_session) {
		error = "reactive session target has no session";
		return false;
	}

	std::shared_ptr<TriggerBox> triggerbox = _session->triggerbox_at (route);
	if (!triggerbox) {
		std::ostringstream msg;
		msg << "missing triggerbox for route " << route;
		error = msg.str ();
		return false;
	}

	TriggerPtr trigger = triggerbox->trigger (static_cast<TriggerBox::Triggers::size_type> (row));
	if (!trigger) {
		std::ostringstream msg;
		msg << "missing trigger at route " << route << " row " << row;
		error = msg.str ();
		return false;
	}

	trigger->set_follow_action_probability (probability);
	error.clear ();
	return true;
}

bool
ReactiveSessionTarget::session_stop_triggers_at (int route, std::string& error)
{
	if (!_session) {
		error = "reactive session target has no session";
		return false;
	}

	std::shared_ptr<TriggerBox> triggerbox = _session->triggerbox_at (route);
	if (!triggerbox) {
		std::ostringstream msg;
		msg << "missing triggerbox for route " << route;
		error = msg.str ();
		return false;
	}

	triggerbox->stop_all_quantized ();
	return true;
}

void
ReactiveSessionTarget::session_trigger_stop_all (bool now)
{
	if (_session) {
		_session->trigger_stop_all (now);
	}
}

void
ReactiveSessionTarget::session_request_transport_speed (double speed)
{
	if (_session) {
		_session->request_transport_speed (speed);
	}
}

void
ReactiveSessionTarget::session_request_roll ()
{
	if (_session) {
		_session->request_roll ();
	}
}

void
ReactiveSessionTarget::session_request_stop ()
{
	if (_session) {
		_session->request_stop ();
	}
}

bool
ReactiveSessionTarget::session_apply_nth_mixer_scene (int index)
{
	return _session && _session->apply_nth_mixer_scene (static_cast<size_t> (index));
}

void
ReactiveSessionTarget::session_store_nth_mixer_scene (int index)
{
	if (_session) {
		_session->store_nth_mixer_scene (static_cast<size_t> (index));
	}
}

bool
ReactiveSessionTarget::session_insert_reactive_rhythm (int route, std::string& error)
{
	if (!_session) {
		error = "reactive session target has no session";
		return false;
	}

	if (route < 0) {
		error = "reactive rhythm insert route index must be non-negative";
		return false;
	}

	std::shared_ptr<Route> target = _session->get_remote_nth_route (static_cast<PresentationInfo::order_t> (route));
	if (!target) {
		std::ostringstream msg;
		msg << "missing route for reactive rhythm insert " << route;
		error = msg.str ();
		return false;
	}

	ReactiveRhythmRouteInsertionResult result = ReactiveRhythmRouteInserter::ensure_inserted (*_session, target);
	if (result.ok ()) {
		error.clear ();
		return true;
	}

	std::ostringstream msg;
	msg << "reactive rhythm insert route " << route << " failed: ";
	switch (result.status) {
	case ReactiveRhythmRouteInsertionStatus::MissingLuaProc:
		msg << "Reactive Rhythm State MVP LuaProc script was not discoverable";
		break;
	case ReactiveRhythmRouteInsertionStatus::PluginLoadFailed:
		msg << "Reactive Rhythm State MVP LuaProc script failed to load";
		break;
	case ReactiveRhythmRouteInsertionStatus::ConfigureFailed:
		msg << "PluginInsert MIDI configuration failed";
		break;
	case ReactiveRhythmRouteInsertionStatus::AddFailed:
		msg << "Route::add_processor returned " << result.route_result;
		break;
	case ReactiveRhythmRouteInsertionStatus::InvalidRoute:
		msg << "invalid route";
		break;
	case ReactiveRhythmRouteInsertionStatus::Inserted:
	case ReactiveRhythmRouteInsertionStatus::AlreadyPresent:
		msg << "unexpected successful status";
		break;
	}

	error = msg.str ();
	return false;
}
