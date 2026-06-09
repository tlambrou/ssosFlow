#include "ardour/reactive_rhythm_midi_byte_adapter.h"

#include <vector>

using namespace ARDOUR;

namespace {

static unsigned char const midi_status_mask = 0xf0;
static unsigned char const midi_channel_mask = 0x0f;
static unsigned char const midi_note_off = 0x80;
static unsigned char const midi_note_on = 0x90;

} // namespace

ReactiveRhythmMidiBytes
ReactiveRhythmMidiBytes::three_byte (
	size_t step,
	size_t frame,
	unsigned char status,
	unsigned char data1,
	unsigned char data2,
	double chance_value)
{
	ReactiveRhythmMidiBytes bytes;
	bytes.step = step;
	bytes.frame = frame;
	bytes.size = 3;
	bytes.data[0] = status;
	bytes.data[1] = data1;
	bytes.data[2] = data2;
	bytes.chance_value = chance_value;
	return bytes;
}

ReactiveRhythmMidiByteAdapter::ReactiveRhythmMidiByteAdapter ()
{
}

ReactiveRhythmMidiByteAdapter::ReactiveRhythmMidiByteAdapter (ReactiveRhythmSettings const& settings)
	: _adapter (settings)
{
}

void
ReactiveRhythmMidiByteAdapter::set_settings (ReactiveRhythmSettings const& settings)
{
	_adapter.set_settings (settings);
}

void
ReactiveRhythmMidiByteAdapter::queue_settings (ReactiveRhythmSettings const& settings)
{
	_adapter.queue_settings (settings);
}

void
ReactiveRhythmMidiByteAdapter::advance_to_step (size_t step)
{
	_adapter.advance_to_step (step);
}

void
ReactiveRhythmMidiByteAdapter::clear_note_state ()
{
	_adapter.clear_note_state ();
}

std::vector<ReactiveRhythmMidiByteDecision>
ReactiveRhythmMidiByteAdapter::process_events (std::vector<ReactiveRhythmMidiBytes> const& raw_events)
{
	std::vector<ReactiveRhythmMidiByteDecision> decisions;
	std::vector<ReactiveRhythmMidiEvent> mapped_events;
	std::vector<size_t> mapped_indices;

	decisions.reserve (raw_events.size ());
	mapped_events.reserve (raw_events.size ());
	mapped_indices.reserve (raw_events.size ());

	for (size_t i = 0; i < raw_events.size (); ++i) {
		ReactiveRhythmMidiByteDecision decision;
		decision.raw = raw_events[i];

		ReactiveRhythmMidiEvent mapped_event;
		if (map_event (raw_events[i], mapped_event)) {
			decision.mapped = true;
			decision.mapped_event = mapped_event;
			mapped_events.push_back (mapped_event);
			mapped_indices.push_back (i);
		}

		decisions.push_back (decision);
	}

	std::vector<ReactiveRhythmMidiDecision> adapter_decisions = _adapter.process_events (mapped_events);
	for (size_t i = 0; i < mapped_indices.size (); ++i) {
		size_t const decision_index = mapped_indices[i];
		decisions[decision_index].adapter_decision = adapter_decisions[i];
		decisions[decision_index].mapped_event = adapter_decisions[i].event;
		decisions[decision_index].forward = adapter_decisions[i].forward;
	}

	return decisions;
}

bool
ReactiveRhythmMidiByteAdapter::map_event (ReactiveRhythmMidiBytes const& raw, ReactiveRhythmMidiEvent& event)
{
	if (raw.size != 3) {
		return false;
	}

	unsigned char const status = raw.data[0] & midi_status_mask;
	int const channel = raw.data[0] & midi_channel_mask;
	int const note = raw.data[1];
	int const velocity = raw.data[2];

	if (status == midi_note_on && velocity > 0) {
		event = ReactiveRhythmMidiEvent::note_on (raw.step, raw.frame, channel, note, velocity, raw.chance_value);
		return true;
	}

	if (status == midi_note_off || status == midi_note_on) {
		event = ReactiveRhythmMidiEvent::note_off (raw.frame, channel, note);
		event.step = raw.step;
		event.chance_value = raw.chance_value;
		return true;
	}

	return false;
}
