#pragma once

#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action_executor.h"
#include "temporal/bbt_time.h"
#include "temporal/types.h"

namespace ARDOUR {

class Session;

struct LIBARDOUR_API ReactiveRoutingSlotSummary {
	size_t slot = 0;
	std::string route_name;
	bool reactive_rhythm_insert_present = false;
	bool reactive_rhythm_values_present = false;
	double rhythm_density = 0.0;
	double rhythm_chance = 0.0;
	double rhythm_priority = 0.0;
	double rhythm_rotation = 0.0;
	std::string status;
};

struct LIBARDOUR_API ReactiveTriggerSlotSummary {
	size_t route = 0;
	size_t slot = 0;
	std::string route_name;
	std::string region_name;
	bool has_triggerbox = false;
	bool populated = false;
	bool playable = false;
	int follow_probability = 0;
	std::string status;
};

struct LIBARDOUR_API ReactiveSessionStateSummary {
	bool session_loaded = false;
	bool transport_rolling = false;
	Temporal::samplepos_t transport_sample = 0;
	Temporal::BBT_Time bbt;
	double tempo_quarter_notes_per_minute = 0.0;
	int meter_divisions_per_bar = 0;
	int meter_note_value = 0;
	size_t route_count = 0;
	size_t trigger_route_count = 0;
	std::string status;
};

class LIBARDOUR_API ReactiveSessionTarget : public ReactiveActionTarget {
public:
	explicit ReactiveSessionTarget (Session&);

	bool cue (int row, std::string& error) override;
	bool trigger (int route, int row, std::string& error) override;
	bool trigger_probability (int route, int row, double value, std::string& error) override;
	bool trigger_stop (int route, std::string& error) override;
	bool stop_all (std::string& error) override;
	bool transport_play (std::string& error) override;
	bool transport_stop (Temporal::BBT_Offset const& after, std::string& error) override;
	bool scene_apply (int index, std::string& error) override;
	bool scene_store (int index, std::string& error) override;
	bool macro (std::string const& name, double value, Temporal::BBT_Offset const& ramp, std::string& error) override;
	bool state (std::string const& name, std::string const& value, std::string& error) override;
	bool harmony (std::string const& name, std::string const& value, std::string& error) override;
	bool rhythm (std::string const& name, double value, std::string& error) override;
	bool rhythm_route (int route, std::string const& name, double value, std::string& error) override;
	bool rhythm_insert (int route, std::string& error) override;
	std::vector<ReactiveRoutingSlotSummary> routing_summary (size_t max_routes) const;
	std::string format_routing_summary (size_t max_routes) const;
	std::vector<ReactiveTriggerSlotSummary> trigger_slot_summary (size_t max_routes, size_t max_slots_per_route) const;
	std::string format_trigger_slot_summary (size_t max_routes, size_t max_slots_per_route) const;
	ReactiveSessionStateSummary session_state_summary () const;
	std::string format_session_state_summary () const;

protected:
	ReactiveSessionTarget ();

	virtual void session_trigger_cue_row (int row);
	virtual bool session_bang_trigger_at (int route, int row, float velocity);
	virtual bool session_set_trigger_follow_probability (int route, int row, int probability, std::string& error);
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
