#include "ardour/reactive_action_executor.h"

using namespace ARDOUR;

namespace {

static bool
execute_command (ReactiveCommand const& command, ReactiveActionTarget& target, std::string& error)
{
	switch (command.type) {
	case ReactiveCommand::Cue:
		return target.cue (command.first, error);
	case ReactiveCommand::Trigger:
		return target.trigger (command.first, command.second, error);
	case ReactiveCommand::TriggerStop:
		return target.trigger_stop (command.first, error);
	case ReactiveCommand::StopAll:
		return target.stop_all (error);
	case ReactiveCommand::TransportPlay:
		return target.transport_play (error);
	case ReactiveCommand::TransportStop:
		return target.transport_stop (command.ramp, error);
	case ReactiveCommand::SceneApply:
		return target.scene_apply (command.first, error);
	case ReactiveCommand::SceneStore:
		return target.scene_store (command.first, error);
	case ReactiveCommand::Macro:
		return target.macro (command.name, command.value, command.ramp, error);
	case ReactiveCommand::MacroSnapshotStore:
	case ReactiveCommand::MacroSnapshotRecall:
		error = "unexpanded macro snapshot command";
		return false;
	case ReactiveCommand::MacroMorph:
		error = "unexpanded macro morph command";
		return false;
	case ReactiveCommand::State:
		return target.state (command.name, command.text, error);
	case ReactiveCommand::Harmony:
		return target.harmony (command.name, command.text, error);
	case ReactiveCommand::Rhythm:
		return target.rhythm (command.name, command.value, error);
	case ReactiveCommand::RhythmRoute:
		return target.rhythm_route (command.first, command.name, command.value, error);
	case ReactiveCommand::RhythmInsert:
		return target.rhythm_insert (command.first, error);
	}

	error = "unknown reactive command";
	return false;
}

} // namespace

ReactiveExecutionResult
ReactiveActionExecutor::execute (ReactiveActionPlan const& plan, ReactiveActionTarget& target)
{
	ReactiveExecutionResult result;

	if (!plan.ok) {
		result.error = plan.error.empty () ? "cannot execute failed reactive action plan" : plan.error;
		return result;
	}

	for (std::vector<ReactiveCommand>::const_iterator command = plan.commands.begin (); command != plan.commands.end (); ++command) {
		std::string error;
		if (!execute_command (*command, target, error)) {
			result.error = error.empty () ? "reactive command target failed" : error;
			return result;
		}

		++result.commands_executed;
	}

	result.ok = true;
	return result;
}
