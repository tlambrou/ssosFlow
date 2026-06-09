#include "ardour/reactive_session_target.h"

#include <memory>
#include <sstream>

#include "ardour/session.h"
#include "ardour/triggerbox.h"

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
	(void) name;
	(void) value;

	error.clear ();
	return true;
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
