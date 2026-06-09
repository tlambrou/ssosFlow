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

struct LIBARDOUR_API ReactiveMacroSlotSummary {
	size_t slot = 0;
	std::string name;
	double value = 0.0;
};

class LIBARDOUR_API ReactiveActionSlotRunner {
public:
	bool load_source (std::string const&, std::string& error);
	bool load_document (ReactiveActionDocument const&, std::string& error);
	void clear ();

	bool loaded () const { return _loaded; }
	size_t action_count () const;
	std::string action_name (size_t slot) const;
	std::vector<ReactiveActionSlotSummary> action_bank_summary (size_t max_slots) const;
	std::vector<ReactiveMacroSlotSummary> macro_bank_summary (size_t max_slots) const;

	ReactiveExecutionResult execute_slot (size_t slot, ReactiveActionTarget&);
	ReactiveExecutionResult execute_midi_event (ReactiveMidiEvent const&, ReactiveActionTarget&);
	ReactiveExecutionResult execute_midi_bytes (unsigned char const* bytes, size_t size, ReactiveActionTarget&);
	ReactiveActionSlotExecutionStatus const& last_execution_status () const { return _last_execution_status; }
	std::string format_last_execution_status () const;
	std::string format_action_bank_summary (size_t max_slots) const;
	std::string format_macro_bank_summary (size_t max_slots) const;

	std::string last_action () const { return _engine.last_action (); }
	double macro_value (std::string const& name) const { return _engine.macro_value (name); }
	std::string state_value (std::string const& name) const { return _engine.state_value (name); }

private:
	void clear_last_execution_status ();
	ReactiveExecutionResult record_execution_status (size_t slot, std::string const& action_name, ReactiveExecutionResult const& result);

	ReactiveActionEngine _engine;
	ReactiveActionSlotExecutionStatus _last_execution_status;
	bool _loaded = false;
};

} // namespace ARDOUR
