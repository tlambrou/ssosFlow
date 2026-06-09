#include "ardour/reactive_rhythm_midi_adapter.h"

#include <vector>

using namespace ARDOUR;

ReactiveRhythmMidiEvent
ReactiveRhythmMidiEvent::note_on (size_t step, size_t frame, int channel, int note, int velocity, double chance_value)
{
	ReactiveRhythmMidiEvent event;
	event.type = ReactiveRhythmMidiEventType::NoteOn;
	event.step = step;
	event.frame = frame;
	event.channel = channel;
	event.note = note;
	event.velocity = velocity;
	event.chance_value = chance_value;
	return event;
}

ReactiveRhythmMidiEvent
ReactiveRhythmMidiEvent::note_off (size_t frame, int channel, int note)
{
	ReactiveRhythmMidiEvent event;
	event.type = ReactiveRhythmMidiEventType::NoteOff;
	event.frame = frame;
	event.channel = channel;
	event.note = note;
	return event;
}

ReactiveRhythmMidiAdapter::ReactiveRhythmMidiAdapter ()
{
}

ReactiveRhythmMidiAdapter::ReactiveRhythmMidiAdapter (ReactiveRhythmSettings const& settings)
	: _rhythm (settings)
{
}

void
ReactiveRhythmMidiAdapter::set_settings (ReactiveRhythmSettings const& settings)
{
	_rhythm.set_settings (settings);
}

void
ReactiveRhythmMidiAdapter::queue_settings (ReactiveRhythmSettings const& settings)
{
	_rhythm.queue_settings (settings);
}

void
ReactiveRhythmMidiAdapter::advance_to_step (size_t step)
{
	_rhythm.advance_to_step (step);
}

void
ReactiveRhythmMidiAdapter::clear_note_state ()
{
	_note_history.clear ();
}

std::vector<ReactiveRhythmMidiDecision>
ReactiveRhythmMidiAdapter::process_events (std::vector<ReactiveRhythmMidiEvent> const& events)
{
	std::vector<ReactiveRhythmMidiDecision> decisions;
	std::vector<ReactiveRhythmEvent> rhythm_events;

	decisions.reserve (events.size ());
	rhythm_events.reserve (events.size ());

	for (size_t i = 0; i < events.size (); ++i) {
		ReactiveRhythmMidiDecision decision;
		decision.event = events[i];
		decisions.push_back (decision);

		if (is_note_on (events[i])) {
			rhythm_events.push_back (rhythm_event (events[i]));
		}
	}

	std::vector<ReactiveRhythmDecision> rhythm_decisions = _rhythm.evaluate (rhythm_events);
	size_t note_on_position = 0;

	for (size_t i = 0; i < decisions.size (); ++i) {
		ReactiveRhythmMidiEvent const& event = decisions[i].event;

		if (is_note_on (event)) {
			ReactiveRhythmDecision const& rhythm_decision = rhythm_decisions[note_on_position++];
			decisions[i].rhythm = rhythm_decision;
			decisions[i].forward = rhythm_decision.passes;
			_note_history[note_key (event)].push_back (rhythm_decision.passes);
		} else if (is_note_off (event)) {
			decisions[i].forward = process_note_off (event);
		}
	}

	return decisions;
}

bool
ReactiveRhythmMidiAdapter::is_note_on (ReactiveRhythmMidiEvent const& event)
{
	return event.type == ReactiveRhythmMidiEventType::NoteOn && event.velocity > 0;
}

bool
ReactiveRhythmMidiAdapter::is_note_off (ReactiveRhythmMidiEvent const& event)
{
	return event.type == ReactiveRhythmMidiEventType::NoteOff;
}

ReactiveRhythmEvent
ReactiveRhythmMidiAdapter::rhythm_event (ReactiveRhythmMidiEvent const& event)
{
	ReactiveRhythmEvent rhythm;
	rhythm.step = event.step;
	rhythm.pitch = event.note;
	rhythm.velocity = event.velocity;
	rhythm.chance_value = event.chance_value;
	return rhythm;
}

ReactiveRhythmMidiAdapter::NoteKey
ReactiveRhythmMidiAdapter::note_key (ReactiveRhythmMidiEvent const& event)
{
	return NoteKey (event.channel, event.note);
}

bool
ReactiveRhythmMidiAdapter::process_note_off (ReactiveRhythmMidiEvent const& event)
{
	std::map<NoteKey, std::deque<bool> >::iterator found = _note_history.find (note_key (event));
	if (found == _note_history.end () || found->second.empty ()) {
		return true;
	}

	bool const forward = found->second.front ();
	found->second.pop_front ();
	if (found->second.empty ()) {
		_note_history.erase (found);
	}

	return forward;
}
