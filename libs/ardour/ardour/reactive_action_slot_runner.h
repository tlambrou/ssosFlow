#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action_engine.h"
#include "ardour/reactive_action_executor.h"
#include "ardour/reactive_action_scheduler.h"

namespace Temporal {
class TempoMap;
}

namespace ARDOUR {

struct LIBARDOUR_API ReactiveActionSlotExecutionStatus {
	bool attempted = false;
	size_t slot = 0;
	std::string action_name;
	ReactiveExecutionResult result;
};

struct LIBARDOUR_API ReactiveActionSlotSummary {
	size_t slot = 0;
	std::string action_name;
	std::string primary_trigger;
	size_t command_count = 0;
	bool latest_attempted = false;
};

struct LIBARDOUR_API ReactivePerformanceControlSummary {
	size_t slot = 0;
	std::string action_name;
	std::string primary_trigger;
	std::string button_label;
	bool available = false;
	bool enabled = false;
};

struct LIBARDOUR_API ReactiveControllerFeedbackSummary {
	size_t slot = 0;
	std::string action_name;
	std::string primary_trigger;
	bool available = false;
	bool enabled = false;
	bool latest_attempted = false;
	bool queued = false;
	int value = 0;
};

struct LIBARDOUR_API ReactiveControllerFeedbackBinding {
	enum Type {
		Note,
		ControlChange
	};

	size_t slot = 0;
	Type type = Note;
	int channel = 0;
	int number = 0;
};

struct LIBARDOUR_API ReactiveControllerFeedbackMidiMessage {
	size_t slot = 0;
	std::vector<unsigned char> bytes;
};

struct LIBARDOUR_API ReactiveMacroSlotSummary {
	size_t slot = 0;
	std::string name;
	double value = 0.0;
};

struct LIBARDOUR_API ReactiveStateSlotSummary {
	size_t slot = 0;
	std::string name;
	std::string value;
};

struct LIBARDOUR_API ReactiveHarmonySlotSummary {
	size_t slot = 0;
	std::string name;
	std::string value;
};

struct LIBARDOUR_API ReactiveActionPreviewSummary {
	bool available = false;
	size_t slot = 0;
	std::string action_name;
	std::string primary_trigger;
	std::string chain_mode;
	std::string quantize;
	Temporal::BBT_Offset quantize_offset;
	size_t command_count = 0;
	std::vector<std::string> command_summaries;
};

struct LIBARDOUR_API ReactiveMidiInputSummary {
	bool available = false;
	std::string event_type;
	int channel = 0;
	int number = 0;
	int value = 0;
	bool from_bytes = false;
	size_t matched_action_count = 0;
	size_t matched_slot = 0;
	std::string matched_action_name;
};

class LIBARDOUR_API ReactiveActionSlotRunner {
public:
	bool load_source (std::string const&, std::string& error);
	bool load_document (ReactiveActionDocument const&, std::string& error);
	void clear ();

	bool performance_enabled () const { return _performance_enabled; }
	void set_performance_enabled (bool);
	void set_transport_rolling_provider (std::function<bool()>);
	bool loaded () const { return _loaded; }
	size_t action_count () const;
	std::string action_name (size_t slot) const;
	std::vector<ReactiveActionSlotSummary> action_bank_summary (size_t max_slots) const;
	std::vector<ReactivePerformanceControlSummary> performance_control_summary (size_t max_slots) const;
	std::vector<ReactiveControllerFeedbackSummary> controller_feedback_summary (size_t max_slots) const;
	std::vector<ReactiveControllerFeedbackMidiMessage> controller_feedback_midi_messages (std::vector<ReactiveControllerFeedbackBinding> const&) const;
	std::vector<ReactiveMacroSlotSummary> macro_bank_summary (size_t max_slots) const;
	std::vector<ReactiveStateSlotSummary> state_bank_summary (size_t max_slots) const;
	std::vector<ReactiveHarmonySlotSummary> harmony_bank_summary (size_t max_slots) const;
	ReactiveActionPreviewSummary preview_slot (size_t slot);
	ReactiveActionPreviewSummary preview_midi_event (ReactiveMidiEvent const&);
	ReactiveActionPreviewSummary preview_marker_event (ReactiveMarkerEvent const&);
	ReactiveActionPreviewSummary preview_region_event (ReactiveRegionEvent const&);
	ReactiveActionPreviewSummary preview_scene_event (ReactiveSceneEvent const&);

	ReactiveExecutionResult execute_or_queue_slot (size_t slot, ReactiveActionTarget&, Temporal::BBT_Time const& requested_at, Temporal::BBT_Time const& due_at);
	ReactiveExecutionResult execute_or_queue_slot (size_t slot, ReactiveActionTarget&, Temporal::TempoMap const&, Temporal::BBT_Time const& requested_at);
	ReactiveExecutionResult execute_or_queue_midi_event (ReactiveMidiEvent const&, ReactiveActionTarget&, Temporal::BBT_Time const& requested_at, Temporal::BBT_Time const& due_at);
	ReactiveExecutionResult execute_or_queue_midi_event (ReactiveMidiEvent const&, ReactiveActionTarget&, Temporal::TempoMap const&, Temporal::BBT_Time const& requested_at);
	ReactiveExecutionResult execute_or_queue_midi_bytes (unsigned char const* bytes, size_t size, ReactiveActionTarget&, Temporal::TempoMap const&, Temporal::BBT_Time const& requested_at);
	ReactiveExecutionResult execute_or_queue_marker_event (ReactiveMarkerEvent const&, ReactiveActionTarget&, Temporal::BBT_Time const& requested_at, Temporal::BBT_Time const& due_at);
	ReactiveExecutionResult execute_or_queue_marker_event (ReactiveMarkerEvent const&, ReactiveActionTarget&, Temporal::TempoMap const&, Temporal::BBT_Time const& requested_at);
	ReactiveExecutionResult execute_or_queue_region_event (ReactiveRegionEvent const&, ReactiveActionTarget&, Temporal::BBT_Time const& requested_at, Temporal::BBT_Time const& due_at);
	ReactiveExecutionResult execute_or_queue_region_event (ReactiveRegionEvent const&, ReactiveActionTarget&, Temporal::TempoMap const&, Temporal::BBT_Time const& requested_at);
	ReactiveExecutionResult execute_or_queue_scene_event (ReactiveSceneEvent const&, ReactiveActionTarget&, Temporal::BBT_Time const& requested_at, Temporal::BBT_Time const& due_at);
	ReactiveExecutionResult execute_or_queue_scene_event (ReactiveSceneEvent const&, ReactiveActionTarget&, Temporal::TempoMap const&, Temporal::BBT_Time const& requested_at);
	ReactiveExecutionResult execute_slot (size_t slot, ReactiveActionTarget&);
	ReactiveExecutionResult execute_midi_event (ReactiveMidiEvent const&, ReactiveActionTarget&);
	ReactiveExecutionResult execute_midi_bytes (unsigned char const* bytes, size_t size, ReactiveActionTarget&);
	ReactiveExecutionResult execute_marker_event (ReactiveMarkerEvent const&, ReactiveActionTarget&);
	ReactiveExecutionResult execute_region_event (ReactiveRegionEvent const&, ReactiveActionTarget&);
	ReactiveExecutionResult execute_scene_event (ReactiveSceneEvent const&, ReactiveActionTarget&);
	ReactiveExecutionResult release_due_queued_actions (Temporal::BBT_Time const& now, ReactiveActionTarget&);
	ReactiveActionSlotExecutionStatus const& last_execution_status () const { return _last_execution_status; }
	ReactiveActionPreviewSummary const& next_action_preview () const { return _next_action_preview; }
	ReactiveMidiInputSummary const& last_midi_input_summary () const { return _last_midi_input_summary; }
	size_t queued_action_count () const { return _scheduler.queued_count (); }
	std::vector<ReactiveQueuedActionSummary> queued_action_summary (size_t max_items, Temporal::BBT_Time const& now) const { return _scheduler.queued_action_summary (max_items, now); }
	std::string format_last_execution_status () const;
	std::string format_performance_mode_status () const;
	std::string format_performance_control_summary (size_t max_slots) const;
	std::string format_next_action_preview () const;
	std::string format_midi_input_summary () const;
	std::string format_queued_action_summary (size_t max_items, Temporal::BBT_Time const& now) const;
	std::string format_panel_summary (size_t max_items) const;
	std::string format_action_bank_summary (size_t max_slots) const;
	std::string format_macro_bank_summary (size_t max_slots) const;
	std::string format_state_bank_summary (size_t max_slots) const;
	std::string format_harmony_bank_summary (size_t max_slots) const;

	std::string last_action () const { return _engine.last_action (); }
	double macro_value (std::string const& name) const { return _engine.macro_value (name); }
	std::string state_value (std::string const& name) const { return _engine.state_value (name); }
	std::string harmony_value (std::string const& name) const { return _engine.harmony_value (name); }

private:
	void refresh_transport_state ();
	void clear_last_execution_status ();
	void clear_next_action_preview ();
	void clear_midi_input_summary ();
	void record_midi_input_event (ReactiveMidiEvent const&, bool from_bytes, std::vector<ReactiveActionMatch> const&);
	ReactiveControllerFeedbackSummary controller_feedback_summary_row (size_t slot) const;
	ReactiveExecutionResult queue_plan (size_t slot, std::string const& primary_trigger, ReactiveActionPlan const&, Temporal::BBT_Time const& requested_at, Temporal::BBT_Time const& due_at);
	ReactiveExecutionResult record_execution_status (size_t slot, std::string const& action_name, ReactiveExecutionResult const& result);

	ReactiveActionEngine _engine;
	ReactiveActionScheduler _scheduler;
	ReactiveActionSlotExecutionStatus _last_execution_status;
	ReactiveActionPreviewSummary _next_action_preview;
	ReactiveMidiInputSummary _last_midi_input_summary;
	std::function<bool()> _transport_rolling_provider;
	bool _loaded = false;
	bool _performance_enabled = true;
};

} // namespace ARDOUR
