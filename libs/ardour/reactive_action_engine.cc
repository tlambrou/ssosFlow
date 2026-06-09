#include "ardour/reactive_action_engine.h"

#include <cstdlib>

using namespace ARDOUR;

namespace {

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
ReactiveActionEngine::load_document (ReactiveActionDocument const& document, std::string& error)
{
	_document = document;
	_sequential_positions.clear ();
	_macros.clear ();
	_states.clear ();
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

ReactiveActionPlan
ReactiveActionEngine::trigger_action (std::string const& name)
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

	if (action->chain_mode == ReactiveChainMode::Sequential) {
		if (!action->commands.empty ()) {
			size_t& position = _sequential_positions[action->name];
			plan.commands.push_back (action->commands[position % action->commands.size ()]);
			position = (position + 1) % action->commands.size ();
		}
	} else if (action->chain_mode == ReactiveChainMode::Random) {
		if (!action->commands.empty ()) {
			plan.commands.push_back (action->commands[std::rand () % action->commands.size ()]);
		}
	} else {
		plan.commands = action->commands;
	}

	for (std::vector<ReactiveCommand>::const_iterator command = plan.commands.begin (); command != plan.commands.end (); ++command) {
		apply_command_state (*command);
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

void
ReactiveActionEngine::apply_command_state (ReactiveCommand const& command)
{
	if (command.type == ReactiveCommand::Macro) {
		_macros[command.name] = command.value;
	} else if (command.type == ReactiveCommand::State) {
		_states[command.name] = command.text;
	}
}
