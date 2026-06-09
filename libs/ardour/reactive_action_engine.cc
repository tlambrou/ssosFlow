#include "ardour/reactive_action_engine.h"

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
