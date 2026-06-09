#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action_engine.h"
#include "ardour/reactive_action_executor.h"

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

struct LIBARDOUR_API ReactiveActionPreviewSummary {
	bool available = false;
	size_t slot = 0;
	std::string action_name;
	std::string primary_trigger;
	std::string chain_mode;
	std::string quantize;
	size_t command_count = 0;
};

class LIBARDOUR_API ReactiveActionSlotRunner {
public:
	bool load_source (std::string const&, std::string& error);
	bool load_document (ReactiveActionDocument const&, std::string& error);
	void clear ();

	bool performance_enabled () const { return _performance_enabled; }
	void set_performance_enabled (bool);
	bool loaded () const { return _loaded; }
	size_t action_count () const;
	std::string action_name (size_t slot) const;
	std::vector<ReactiveActionSlotSummary> action_bank_summary (size_t max_slots) const;
	std::vector<ReactivePerformanceControlSummary> performance_control_summary (size_t max_slots) const;
	std::vector<ReactiveControllerFeedbackSummary> controller_feedback_summary (size_t max_slots) const;
	std::vector<ReactiveControllerFeedbackMidiMessage> controller_feedback_midi_messages (std::vector<ReactiveControllerFeedbackBinding> const&) const;
	std::vector<ReactiveMacroSlotSummary> macro_bank_summary (size_t max_slots) const;
	std::vector<ReactiveStateSlotSummary> state_bank_summary (size_t max_slots) const;
	ReactiveActionPreviewSummary preview_slot (size_t slot) const;
	ReactiveActionPreviewSummary preview_midi_event (ReactiveMidiEvent const&) const;

	ReactiveExecutionResult execute_slot (size_t slot, ReactiveActionTarget&);
	ReactiveExecutionResult execute_midi_event (ReactiveMidiEvent const&, ReactiveActionTarget&);
	ReactiveExecutionResult execute_midi_bytes (unsigned char const* bytes, size_t size, ReactiveActionTarget&);
	ReactiveActionSlotExecutionStatus const& last_execution_status () const { return _last_execution_status; }
	ReactiveActionPreviewSummary const& next_action_preview () const { return _next_action_preview; }
	std::string format_last_execution_status () const;
	std::string format_performance_mode_status () const;
	std::string format_performance_control_summary (size_t max_slots) const;
	std::string format_next_action_preview () const;
	std::string format_action_bank_summary (size_t max_slots) const;
	std::string format_macro_bank_summary (size_t max_slots) const;
	std::string format_state_bank_summary (size_t max_slots) const;

	std::string last_action () const { return _engine.last_action (); }
	double macro_value (std::string const& name) const { return _engine.macro_value (name); }
	std::string state_value (std::string const& name) const { return _engine.state_value (name); }

private:
	void clear_last_execution_status ();
	void clear_next_action_preview ();
	ReactiveControllerFeedbackSummary controller_feedback_summary_row (size_t slot) const;
	ReactiveExecutionResult record_execution_status (size_t slot, std::string const& action_name, ReactiveExecutionResult const& result);

	ReactiveActionEngine _engine;
	ReactiveActionSlotExecutionStatus _last_execution_status;
	ReactiveActionPreviewSummary _next_action_preview;
	bool _loaded = false;
	bool _performance_enabled = true;
};

} // namespace ARDOUR
