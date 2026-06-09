#include "ardour/reactive_session_target.h"

#include <memory>
#include <sstream>

#include "ardour/automation_control.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/reactive_rhythm_route_inserter.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/triggerbox.h"

#include "evoral/Parameter.h"

#include "pbd/controllable.h"

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
		(*route)->foreach_processor ([&updated, &label, value, scale, &error] (std::weak_ptr<Processor> wp) {
			std::shared_ptr<Processor> processor = wp.lock ();
			if (!processor || !ReactiveRhythmRouteInserter::is_reactive_rhythm_insert (processor)) {
				return;
			}

			std::shared_ptr<PluginInsert> insert = std::dynamic_pointer_cast<PluginInsert> (processor);
			if (set_reactive_rhythm_control (insert, label, value * scale, error)) {
				++updated;
			}
		});
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
ReactiveSessionTarget::rhythm_insert (int route, std::string& error)
{
	return session_insert_reactive_rhythm (route, error);
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
