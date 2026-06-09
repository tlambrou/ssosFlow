#pragma once

#include <cstddef>
#include <string>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action_engine.h"

namespace ARDOUR {

class LIBARDOUR_API ReactiveActionTarget {
public:
	virtual ~ReactiveActionTarget () {}

	virtual bool cue (int row, std::string& error) = 0;
	virtual bool trigger (int route, int row, std::string& error) = 0;
	virtual bool trigger_stop (int route, std::string& error) = 0;
	virtual bool stop_all (std::string& error) = 0;
	virtual bool transport_play (std::string& error) = 0;
	virtual bool transport_stop (Temporal::BBT_Offset const& after, std::string& error) = 0;
	virtual bool scene_apply (int index, std::string& error) = 0;
	virtual bool scene_store (int index, std::string& error) = 0;
	virtual bool macro (std::string const& name, double value, Temporal::BBT_Offset const& ramp, std::string& error) = 0;
	virtual bool state (std::string const& name, std::string const& value, std::string& error) = 0;
	virtual bool rhythm (std::string const& name, double value, std::string& error) = 0;
};

struct LIBARDOUR_API ReactiveExecutionResult {
	bool ok = false;
	std::string error;
	size_t commands_executed = 0;
};

class LIBARDOUR_API ReactiveActionExecutor {
public:
	static ReactiveExecutionResult execute (ReactiveActionPlan const&, ReactiveActionTarget&);
};

} // namespace ARDOUR
