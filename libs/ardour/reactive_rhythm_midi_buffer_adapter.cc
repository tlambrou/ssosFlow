#include "ardour/reactive_rhythm_midi_buffer_adapter.h"

#include <vector>

using namespace ARDOUR;

namespace {

static double
chance_value_at (std::vector<double> const& chance_values, size_t index)
{
	if (index >= chance_values.size ()) {
		return 0.0;
	}

	return chance_values[index];
}

} // namespace

ReactiveRhythmMidiBufferAdapter::ReactiveRhythmMidiBufferAdapter ()
{
}

ReactiveRhythmMidiBufferAdapter::ReactiveRhythmMidiBufferAdapter (ReactiveRhythmSettings const& settings)
	: _adapter (settings)
{
}

void
ReactiveRhythmMidiBufferAdapter::set_settings (ReactiveRhythmSettings const& settings)
{
	_adapter.set_settings (settings);
}

void
ReactiveRhythmMidiBufferAdapter::queue_settings (ReactiveRhythmSettings const& settings)
{
	_adapter.queue_settings (settings);
}

void
ReactiveRhythmMidiBufferAdapter::advance_to_step (size_t step)
{
	_adapter.advance_to_step (step);
}

void
ReactiveRhythmMidiBufferAdapter::clear_note_state ()
{
	_adapter.clear_note_state ();
}

std::vector<ReactiveRhythmMidiBufferDecision>
ReactiveRhythmMidiBufferAdapter::process_buffer (MidiBuffer& buffer, std::vector<double> const& chance_values)
{
	std::vector<ReactiveRhythmMidiBufferDecision> decisions;
	std::vector<ReactiveRhythmMidiBytes> raw_events;

	size_t event_index = 0;
	for (MidiBuffer::iterator i = buffer.begin (); i != buffer.end (); ++i, ++event_index) {
		Evoral::Event<samplepos_t> event (*i, false);
		ReactiveRhythmMidiBufferDecision decision;
		decision.time = event.time ();
		decision.event_type = event.event_type ();
		decision.bytes.assign (event.buffer (), event.buffer () + event.size ());
		decisions.push_back (decision);

		raw_events.push_back (bytes_for_event (event, event_index, chance_value_at (chance_values, event_index)));
	}

	std::vector<ReactiveRhythmMidiByteDecision> byte_decisions = _adapter.process_events (raw_events);
	for (size_t i = 0; i < decisions.size (); ++i) {
		decisions[i].byte_decision = byte_decisions[i];
		decisions[i].forward = byte_decisions[i].forward;
	}

	buffer.clear ();
	for (std::vector<ReactiveRhythmMidiBufferDecision>::const_iterator decision = decisions.begin (); decision != decisions.end (); ++decision) {
		if (!decision->forward || decision->bytes.empty ()) {
			continue;
		}

		buffer.push_back (
			decision->time,
			decision->event_type,
			decision->bytes.size (),
			&decision->bytes.front ());
	}

	return decisions;
}

ReactiveRhythmMidiBytes
ReactiveRhythmMidiBufferAdapter::bytes_for_event (
	Evoral::Event<samplepos_t> const& event,
	size_t step,
	double chance_value)
{
	unsigned char status = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;

	if (event.size () > 0) {
		status = event.buffer ()[0];
	}
	if (event.size () > 1) {
		data1 = event.buffer ()[1];
	}
	if (event.size () > 2) {
		data2 = event.buffer ()[2];
	}

	ReactiveRhythmMidiBytes bytes = ReactiveRhythmMidiBytes::three_byte (
		step,
		static_cast<size_t> (event.time ()),
		status,
		data1,
		data2,
		chance_value);
	bytes.size = event.size ();
	return bytes;
}
