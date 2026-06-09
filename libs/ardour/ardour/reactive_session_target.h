#pragma once

#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action_executor.h"

namespace ARDOUR {

class Session;

struct LIBARDOUR_API ReactiveRoutingSlotSummary {
	size_t slot = 0;
	std::string route_name;
	bool reactive_rhythm_insert_present = false;
	std::string status;
};

class LIBARDOUR_API ReactiveSessionTarget : public ReactiveActionTarget {
public:
	explicit ReactiveSessionTarget (Session&);

	bool cue (int row, std::string& error) override;
	bool trigger (int route, int row, std::string& error) override;
	bool trigger_stop (int route, std::string& error) override;
	bool stop_all (std::string& error) override;
	bool transport_play (std::string& error) override;
	bool transport_stop (Temporal::BBT_Offset const& after, std::string& error) override;
	bool scene_apply (int index, std::string& error) override;
	bool scene_store (int index, std::string& error) override;
	bool macro (std::string const& name, double value, Temporal::BBT_Offset const& ramp, std::string& error) override;
	bool state (std::string const& name, std::string const& value, std::string& error) override;
	bool rhythm (std::string const& name, double value, std::string& error) override;
	bool rhythm_route (int route, std::string const& name, double value, std::string& error) override;
	bool rhythm_insert (int route, std::string& error) override;
	std::vector<ReactiveRoutingSlotSummary> routing_summary (size_t max_routes) const;
	std::string format_routing_summary (size_t max_routes) const;

protected:
	ReactiveSessionTarget ();

	virtual void session_trigger_cue_row (int row);
	virtual bool session_bang_trigger_at (int route, int row, float velocity);
	virtual bool session_stop_triggers_at (int route, std::string& error);
	virtual void session_trigger_stop_all (bool now);
	virtual void session_request_transport_speed (double speed);
	virtual void session_request_roll ();
	virtual void session_request_stop ();
	virtual bool session_apply_nth_mixer_scene (int index);
	virtual void session_store_nth_mixer_scene (int index);
	virtual bool session_insert_reactive_rhythm (int route, std::string& error);

private:
	Session* _session;
};

} // namespace ARDOUR
