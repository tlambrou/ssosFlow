#include "ardour/reactive_rhythm_insertion_plan.h"

#include <algorithm>

using namespace ARDOUR;

namespace {

static bool
contains_target (std::vector<ReactiveRhythmInsertionTarget> const& targets, ReactiveRhythmInsertionTarget target)
{
	return std::find (targets.begin (), targets.end (), target) != targets.end ();
}

static void
add_unique_target (std::vector<ReactiveRhythmInsertionTarget>& targets, ReactiveRhythmInsertionTarget target)
{
	if (!contains_target (targets, target)) {
		targets.push_back (target);
	}
}

static bool
has_complete_luaproc_path (ReactiveRhythmInsertionCapabilities const& capabilities)
{
	return capabilities.lua_proc_available && capabilities.lua_proc_midi_io && capabilities.lua_proc_time_info;
}

} // namespace

bool
ReactiveRhythmInsertionPlan::defers (ReactiveRhythmInsertionTarget target) const
{
	return contains_target (deferred_targets, target);
}

bool
ReactiveRhythmInsertionPlan::rejects (ReactiveRhythmInsertionTarget target) const
{
	return contains_target (rejected_targets, target);
}

ReactiveRhythmInsertionPlan
ReactiveRhythmInsertionPlanner::choose (ReactiveRhythmInsertionCapabilities const& capabilities)
{
	ReactiveRhythmInsertionPlan plan;
	plan.allow_live_route_mutation = false;

	if (capabilities.native_processor_available) {
		add_unique_target (plan.deferred_targets, ReactiveRhythmInsertionTarget::NativeProcessor);
	}

	if (capabilities.external_plugin_available) {
		add_unique_target (plan.deferred_targets, ReactiveRhythmInsertionTarget::ExternalPlugin);
	}

	if (capabilities.midi_route_hook_available) {
		add_unique_target (plan.rejected_targets, ReactiveRhythmInsertionTarget::MidiRouteHook);
	}

	if (capabilities.control_surface_available) {
		add_unique_target (plan.rejected_targets, ReactiveRhythmInsertionTarget::ControlSurface);
	}

	if (has_complete_luaproc_path (capabilities)) {
		plan.target = ReactiveRhythmInsertionTarget::LuaProc;
		plan.can_process_midi_stream = true;
		plan.rationale = "LuaProc can process MIDI with DSP time information through Ardour's plugin graph; live route insertion remains a later integration step.";
		return plan;
	}

	if (capabilities.lua_proc_available) {
		add_unique_target (plan.rejected_targets, ReactiveRhythmInsertionTarget::LuaProc);
		plan.rationale = "LuaProc is present, but the reactive rhythm MVP requires both MIDI input/output and DSP time information before it can own stream processing.";
	} else {
		plan.rationale = "No low-risk reactive rhythm stream insertion target is available; keep the backend adapter offline until a LuaProc/plugin path is present.";
	}

	return plan;
}
