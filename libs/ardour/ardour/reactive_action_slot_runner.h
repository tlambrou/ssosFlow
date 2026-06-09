#pragma once

#include <cstddef>
#include <string>

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

class LIBARDOUR_API ReactiveActionSlotRunner {
public:
	bool load_source (std::string const&, std::string& error);
	bool load_document (ReactiveActionDocument const&, std::string& error);
	void clear ();

	bool loaded () const { return _loaded; }
	size_t action_count () const;
	std::string action_name (size_t slot) const;

	ReactiveExecutionResult execute_slot (size_t slot, ReactiveActionTarget&);
	ReactiveExecutionResult execute_midi_event (ReactiveMidiEvent const&, ReactiveActionTarget&);
	ReactiveExecutionResult execute_midi_bytes (unsigned char const* bytes, size_t size, ReactiveActionTarget&);
	ReactiveActionSlotExecutionStatus const& last_execution_status () const { return _last_execution_status; }
	std::string format_last_execution_status () const;

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
