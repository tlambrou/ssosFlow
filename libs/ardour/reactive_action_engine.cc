#include "ardour/reactive_action_engine.h"

#include <cmath>
#include <cstdlib>

using namespace ARDOUR;

namespace {

static unsigned char const midi_status_mask = 0xf0;
static unsigned char const midi_channel_mask = 0x0f;
static unsigned char const midi_note_on = 0x90;
static unsigned char const midi_control_change = 0xb0;
static double const condition_value_tolerance = 0.0001;

static bool
matches_midi_note (ReactiveTrigger const& trigger, ReactiveMidiEvent const& event)
{
	return event.type == ReactiveMidiEvent::NoteOn &&
	       event.value > 0 &&
	       trigger.type == ReactiveTrigger::MidiNote &&
	       trigger.channel == event.channel &&
	       trigger.number == event.number;
}

static bool
matches_midi_cc (ReactiveTrigger const& trigger, ReactiveMidiEvent const& event)
{
	if (event.type != ReactiveMidiEvent::ControlChange ||
	    trigger.type != ReactiveTrigger::MidiCC ||
	    trigger.channel != event.channel ||
	    trigger.number != event.number) {
		return false;
	}

	return trigger.threshold < 0 || event.value > trigger.threshold;
}

static bool
matches_midi_event (ReactiveTrigger const& trigger, ReactiveMidiEvent const& event)
{
	return matches_midi_note (trigger, event) || matches_midi_cc (trigger, event);
}

static bool
matches_marker_event (ReactiveTrigger const& trigger, ReactiveMarkerEvent const& event)
{
	return trigger.type == ReactiveTrigger::Marker &&
	       !trigger.name.empty () &&
	       trigger.name == event.name;
}

static bool
matches_region_event (ReactiveTrigger const& trigger, ReactiveRegionEvent const& event)
{
	return trigger.type == ReactiveTrigger::Region &&
	       !trigger.name.empty () &&
	       trigger.name == event.name;
}

static bool
matches_scene_event (ReactiveTrigger const& trigger, ReactiveSceneEvent const& event)
{
	return trigger.type == ReactiveTrigger::Scene &&
	       trigger.number == event.number;
}

static double
normalized_midi_value (ReactiveMidiEvent const& event)
{
	if (event.value <= 0) {
		return 0.0;
	}

	if (event.value >= 127) {
		return 1.0;
	}

	return event.value / 127.0;
}

static ReactiveCommand
resolve_command_value (ReactiveCommand const& command, ReactiveMidiEvent const* event)
{
	if ((command.type != ReactiveCommand::Macro && command.type != ReactiveCommand::RhythmRoute) ||
	    command.value_source != ReactiveCommand::MidiEventValue ||
	    !event) {
		return command;
	}

	ReactiveCommand resolved = command;
	resolved.value = normalized_midi_value (*event);
	return resolved;
}

} // namespace

ReactiveMidiEvent
ReactiveMidiEvent::note_on (int channel, int note, int velocity)
{
	ReactiveMidiEvent event;
	event.type = NoteOn;
	event.channel = channel;
	event.number = note;
	event.value = velocity;
	return event;
}

ReactiveMidiEvent
ReactiveMidiEvent::control_change (int channel, int controller, int value)
{
	ReactiveMidiEvent event;
	event.type = ControlChange;
	event.channel = channel;
	event.number = controller;
	event.value = value;
	return event;
}

bool
ReactiveMidiEvent::from_midi_bytes (unsigned char const* bytes, size_t size, ReactiveMidiEvent& event)
{
	if (!bytes || size < 3) {
		return false;
	}

	unsigned char const status = bytes[0] & midi_status_mask;
	int const channel = (bytes[0] & midi_channel_mask) + 1;
	int const number = bytes[1];
	int const value = bytes[2];

	if (status == midi_note_on && value > 0) {
		event = note_on (channel, number, value);
		return true;
	}

	if (status == midi_control_change) {
		event = control_change (channel, number, value);
		return true;
	}

	return false;
}

ReactiveMarkerEvent
ReactiveMarkerEvent::named (std::string const& name)
{
	ReactiveMarkerEvent event;
	event.name = name;
	return event;
}

ReactiveRegionEvent
ReactiveRegionEvent::named (std::string const& name)
{
	ReactiveRegionEvent event;
	event.name = name;
	return event;
}

ReactiveSceneEvent
ReactiveSceneEvent::numbered (int number)
{
	ReactiveSceneEvent event;
	event.number = number;
	return event;
}

bool
ReactiveActionEngine::load_document (ReactiveActionDocument const& document, std::string& error)
{
	_document = document;
	_sequential_positions.clear ();
	_macros.clear ();
	_macro_snapshots.clear ();
	_states.clear ();
	_harmony.clear ();
	_last_action.clear ();
	error.clear ();
	return true;
}

std::vector<ReactiveActionMatch>
ReactiveActionEngine::match_midi_event (ReactiveMidiEvent const& event) const
{
	std::vector<ReactiveActionMatch> matches;
	std::vector<ReactiveAction> const& actions = _document.actions ();

	for (size_t action_index = 0; action_index < actions.size (); ++action_index) {
		ReactiveAction const& action = actions[action_index];

		for (std::vector<ReactiveTrigger>::const_iterator trigger = action.triggers.begin (); trigger != action.triggers.end (); ++trigger) {
			if (!matches_midi_event (*trigger, event)) {
				continue;
			}

			ReactiveActionMatch match;
			match.action = &action;
			match.action_index = action_index;
			match.chain_mode = action.chain_mode;
			match.quantize = action.quantize;
			matches.push_back (match);
			break;
		}
	}

	return matches;
}

std::vector<ReactiveActionMatch>
ReactiveActionEngine::match_marker_event (ReactiveMarkerEvent const& event) const
{
	std::vector<ReactiveActionMatch> matches;
	std::vector<ReactiveAction> const& actions = _document.actions ();

	for (size_t action_index = 0; action_index < actions.size (); ++action_index) {
		ReactiveAction const& action = actions[action_index];

		for (std::vector<ReactiveTrigger>::const_iterator trigger = action.triggers.begin (); trigger != action.triggers.end (); ++trigger) {
			if (!matches_marker_event (*trigger, event)) {
				continue;
			}

			ReactiveActionMatch match;
			match.action = &action;
			match.action_index = action_index;
			match.chain_mode = action.chain_mode;
			match.quantize = action.quantize;
			matches.push_back (match);
			break;
		}
	}

	return matches;
}

std::vector<ReactiveActionMatch>
ReactiveActionEngine::match_region_event (ReactiveRegionEvent const& event) const
{
	std::vector<ReactiveActionMatch> matches;
	std::vector<ReactiveAction> const& actions = _document.actions ();

	for (size_t action_index = 0; action_index < actions.size (); ++action_index) {
		ReactiveAction const& action = actions[action_index];

		for (std::vector<ReactiveTrigger>::const_iterator trigger = action.triggers.begin (); trigger != action.triggers.end (); ++trigger) {
			if (!matches_region_event (*trigger, event)) {
				continue;
			}

			ReactiveActionMatch match;
			match.action = &action;
			match.action_index = action_index;
			match.chain_mode = action.chain_mode;
			match.quantize = action.quantize;
			matches.push_back (match);
			break;
		}
	}

	return matches;
}

std::vector<ReactiveActionMatch>
ReactiveActionEngine::match_scene_event (ReactiveSceneEvent const& event) const
{
	std::vector<ReactiveActionMatch> matches;
	std::vector<ReactiveAction> const& actions = _document.actions ();

	for (size_t action_index = 0; action_index < actions.size (); ++action_index) {
		ReactiveAction const& action = actions[action_index];

		for (std::vector<ReactiveTrigger>::const_iterator trigger = action.triggers.begin (); trigger != action.triggers.end (); ++trigger) {
			if (!matches_scene_event (*trigger, event)) {
				continue;
			}

			ReactiveActionMatch match;
			match.action = &action;
			match.action_index = action_index;
			match.chain_mode = action.chain_mode;
			match.quantize = action.quantize;
			matches.push_back (match);
			break;
		}
	}

	return matches;
}

ReactiveActionPlan
ReactiveActionEngine::preview_action (std::string const& name) const
{
	ReactiveActionPlan plan;
	std::vector<ReactiveAction> const& actions = _document.actions ();
	ReactiveAction const* action = 0;

	for (size_t action_index = 0; action_index < actions.size (); ++action_index) {
		if (actions[action_index].name == name) {
			action = &actions[action_index];
			plan.action_index = action_index;
			break;
		}
	}

	if (!action) {
		plan.error = "unknown action '" + name + "'";
		return plan;
	}

	plan.ok = true;
	plan.action_name = action->name;
	plan.chain_mode = action->chain_mode;
	plan.quantize = action->quantize;

	if (!conditions_match (*action, plan.error)) {
		plan.ok = false;
		return plan;
	}

	if (action->chain_mode == ReactiveChainMode::Sequential) {
		if (!action->commands.empty ()) {
			size_t position = 0;
			std::map<std::string, size_t>::const_iterator found = _sequential_positions.find (action->name);
			if (found != _sequential_positions.end ()) {
				position = found->second;
			}
			plan.commands.push_back (action->commands[position % action->commands.size ()]);
		}
	} else if (action->chain_mode == ReactiveChainMode::Random) {
		if (!action->commands.empty ()) {
			plan.commands.push_back (action->commands.front ());
		}
	} else {
		plan.commands = action->commands;
	}

	std::vector<ReactiveCommand> raw_commands = plan.commands;
	if (!preview_plan_commands (raw_commands, 0, plan.commands, plan.error)) {
		plan.commands.clear ();
		plan.ok = false;
	}

	return plan;
}

ReactiveActionPlan
ReactiveActionEngine::trigger_action (std::string const& name)
{
	return trigger_action (name, 0);
}

ReactiveActionPlan
ReactiveActionEngine::trigger_action (std::string const& name, ReactiveMidiEvent const* event)
{
	ReactiveActionPlan plan;
	std::vector<ReactiveAction> const& actions = _document.actions ();
	ReactiveAction const* action = 0;

	for (size_t action_index = 0; action_index < actions.size (); ++action_index) {
		if (actions[action_index].name == name) {
			action = &actions[action_index];
			plan.action_index = action_index;
			break;
		}
	}

	if (!action) {
		plan.error = "unknown action '" + name + "'";
		return plan;
	}

	plan.ok = true;
	plan.action_name = action->name;
	plan.chain_mode = action->chain_mode;
	plan.quantize = action->quantize;

	if (!conditions_match (*action, plan.error)) {
		plan.ok = false;
		return plan;
	}

	std::vector<ReactiveCommand> raw_commands;
	bool advance_sequential = false;
	size_t next_sequential_position = 0;

	if (action->chain_mode == ReactiveChainMode::Sequential) {
		if (!action->commands.empty ()) {
			size_t position = 0;
			std::map<std::string, size_t>::const_iterator found = _sequential_positions.find (action->name);
			if (found != _sequential_positions.end ()) {
				position = found->second;
			}
			raw_commands.push_back (action->commands[position % action->commands.size ()]);
			next_sequential_position = (position + 1) % action->commands.size ();
			advance_sequential = true;
		}
	} else if (action->chain_mode == ReactiveChainMode::Random) {
		if (!action->commands.empty ()) {
			raw_commands.push_back (action->commands[std::rand () % action->commands.size ()]);
		}
	} else {
		raw_commands = action->commands;
	}

	if (!trigger_plan_commands (raw_commands, event, plan.commands, plan.error)) {
		plan.commands.clear ();
		plan.ok = false;
		return plan;
	}

	if (advance_sequential) {
		_sequential_positions[action->name] = next_sequential_position;
	}

	_last_action = action->name;
	return plan;
}

double
ReactiveActionEngine::macro_value (std::string const& name) const
{
	std::map<std::string, double>::const_iterator found = _macros.find (name);
	if (found == _macros.end ()) {
		return 0.0;
	}

	return found->second;
}

std::string
ReactiveActionEngine::state_value (std::string const& name) const
{
	std::map<std::string, std::string>::const_iterator found = _states.find (name);
	if (found == _states.end ()) {
		return std::string ();
	}

	return found->second;
}

std::string
ReactiveActionEngine::harmony_value (std::string const& name) const
{
	std::map<std::string, std::string>::const_iterator found = _harmony.find (name);
	if (found == _harmony.end ()) {
		return std::string ();
	}

	return found->second;
}

bool
ReactiveActionEngine::preview_plan_commands (std::vector<ReactiveCommand> const& input, ReactiveMidiEvent const* event, std::vector<ReactiveCommand>& output, std::string& error) const
{
	output.clear ();
	error.clear ();

	for (std::vector<ReactiveCommand>::const_iterator command = input.begin (); command != input.end (); ++command) {
		ReactiveCommand resolved = resolve_command_value (*command, event);
		if (resolved.type == ReactiveCommand::MacroSnapshotStore) {
			continue;
		}
		if (resolved.type == ReactiveCommand::MacroSnapshotRecall) {
			std::map<std::string, MacroSnapshot>::const_iterator snapshot = _macro_snapshots.find (resolved.name);
			if (snapshot == _macro_snapshots.end ()) {
				error = "unknown macro snapshot '" + resolved.name + "'";
				output.clear ();
				return false;
			}
			for (MacroSnapshot::const_iterator value = snapshot->second.begin (); value != snapshot->second.end (); ++value) {
				ReactiveCommand macro;
				macro.type = ReactiveCommand::Macro;
				macro.name = value->first;
				macro.value = value->second;
				macro.ramp = resolved.ramp;
				output.push_back (macro);
			}
			continue;
		}
		output.push_back (resolved);
	}

	return true;
}

bool
ReactiveActionEngine::conditions_match (ReactiveAction const& action, std::string& error) const
{
	for (std::vector<ReactiveCondition>::const_iterator condition = action.conditions.begin (); condition != action.conditions.end (); ++condition) {
		bool matches = false;

		switch (condition->type) {
		case ReactiveCondition::StateEquals: {
			std::map<std::string, std::string>::const_iterator found = _states.find (condition->name);
			matches = found != _states.end () && found->second == condition->text;
			break;
		}
		case ReactiveCondition::HarmonyEquals: {
			std::map<std::string, std::string>::const_iterator found = _harmony.find (condition->name);
			matches = found != _harmony.end () && found->second == condition->text;
			break;
		}
		case ReactiveCondition::MacroEquals:
			matches = std::fabs (macro_value (condition->name) - condition->value) <= condition_value_tolerance;
			break;
		case ReactiveCondition::TransportRolling:
			matches = _transport_rolling;
			break;
		case ReactiveCondition::TransportStopped:
			matches = !_transport_rolling;
			break;
		}

		if (!matches) {
			error = "unmet condition for action '" + action.name + "'";
			return false;
		}
	}

	error.clear ();
	return true;
}

bool
ReactiveActionEngine::trigger_plan_commands (std::vector<ReactiveCommand> const& input, ReactiveMidiEvent const* event, std::vector<ReactiveCommand>& output, std::string& error)
{
	std::vector<ReactiveCommand> planned;
	std::map<std::string, double> macros = _macros;
	std::map<std::string, MacroSnapshot> macro_snapshots = _macro_snapshots;
	std::map<std::string, std::string> states = _states;
	std::map<std::string, std::string> harmony = _harmony;

	error.clear ();

	for (std::vector<ReactiveCommand>::const_iterator command = input.begin (); command != input.end (); ++command) {
		ReactiveCommand resolved = resolve_command_value (*command, event);
		if (resolved.type == ReactiveCommand::MacroSnapshotStore) {
			macro_snapshots[resolved.name] = macros;
			continue;
		}
		if (resolved.type == ReactiveCommand::MacroSnapshotRecall) {
			std::map<std::string, MacroSnapshot>::const_iterator snapshot = macro_snapshots.find (resolved.name);
			if (snapshot == macro_snapshots.end ()) {
				error = "unknown macro snapshot '" + resolved.name + "'";
				output.clear ();
				return false;
			}
			for (MacroSnapshot::const_iterator value = snapshot->second.begin (); value != snapshot->second.end (); ++value) {
				ReactiveCommand macro;
				macro.type = ReactiveCommand::Macro;
				macro.name = value->first;
				macro.value = value->second;
				macro.ramp = resolved.ramp;
				macros[macro.name] = macro.value;
				planned.push_back (macro);
			}
			continue;
		}

		if (resolved.type == ReactiveCommand::Macro) {
			macros[resolved.name] = resolved.value;
		} else if (resolved.type == ReactiveCommand::State) {
			states[resolved.name] = resolved.text;
		} else if (resolved.type == ReactiveCommand::Harmony) {
			harmony[resolved.name] = resolved.text;
		}

		planned.push_back (resolved);
	}

	_macros = macros;
	_macro_snapshots = macro_snapshots;
	_states = states;
	_harmony = harmony;
	output = planned;
	return true;
}
