#pragma once

#include <cstddef>
#include <string>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action_engine.h"
#include "ardour/reactive_action_executor.h"

namespace ARDOUR {

class LIBARDOUR_API ReactiveActionSlotRunner {
public:
	bool load_source (std::string const&, std::string& error);
	bool load_document (ReactiveActionDocument const&, std::string& error);

	bool loaded () const { return _loaded; }
	size_t action_count () const;
	std::string action_name (size_t slot) const;

	ReactiveExecutionResult execute_slot (size_t slot, ReactiveActionTarget&);

	std::string last_action () const { return _engine.last_action (); }
	double macro_value (std::string const& name) const { return _engine.macro_value (name); }
	std::string state_value (std::string const& name) const { return _engine.state_value (name); }

private:
	ReactiveActionEngine _engine;
	bool _loaded = false;
};

} // namespace ARDOUR
