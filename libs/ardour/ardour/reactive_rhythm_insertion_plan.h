#pragma once

#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

enum class ReactiveRhythmInsertionTarget {
	None,
	LuaProc,
	NativeProcessor,
	MidiRouteHook,
	ControlSurface,
	ExternalPlugin
};

struct LIBARDOUR_API ReactiveRhythmInsertionCapabilities {
	bool lua_proc_available = false;
	bool lua_proc_midi_io = false;
	bool lua_proc_time_info = false;
	bool native_processor_available = false;
	bool midi_route_hook_available = false;
	bool control_surface_available = false;
	bool external_plugin_available = false;
};

struct LIBARDOUR_API ReactiveRhythmInsertionPlan {
	ReactiveRhythmInsertionTarget target = ReactiveRhythmInsertionTarget::None;
	bool can_process_midi_stream = false;
	bool allow_live_route_mutation = false;
	std::string rationale;
	std::vector<ReactiveRhythmInsertionTarget> deferred_targets;
	std::vector<ReactiveRhythmInsertionTarget> rejected_targets;

	bool defers (ReactiveRhythmInsertionTarget) const;
	bool rejects (ReactiveRhythmInsertionTarget) const;
};

class LIBARDOUR_API ReactiveRhythmInsertionPlanner
{
public:
	static ReactiveRhythmInsertionPlan choose (ReactiveRhythmInsertionCapabilities const&);
};

} // namespace ARDOUR
