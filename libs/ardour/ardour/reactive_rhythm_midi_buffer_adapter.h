#pragma once

#include <vector>

#include "evoral/types.h"

#include "ardour/libardour_visibility.h"
#include "ardour/midi_buffer.h"
#include "ardour/reactive_rhythm_midi_byte_adapter.h"

namespace ARDOUR {

class ReactiveRhythmChanceSource;
class ReactiveRhythmStepSource;

struct LIBARDOUR_API ReactiveRhythmMidiBufferDecision {
	samplepos_t time = 0;
	Evoral::EventType event_type = Evoral::NO_EVENT;
	std::vector<unsigned char> bytes;
	bool forward = true;
	ReactiveRhythmMidiByteDecision byte_decision;
};

class LIBARDOUR_API ReactiveRhythmMidiBufferAdapter {
public:
	ReactiveRhythmMidiBufferAdapter ();
	explicit ReactiveRhythmMidiBufferAdapter (ReactiveRhythmSettings const&);

	ReactiveRhythmSettings const& settings () const { return _adapter.settings (); }

	void set_settings (ReactiveRhythmSettings const&);
	void queue_settings (ReactiveRhythmSettings const&);
	void advance_to_step (size_t step);
	bool has_pending_settings () const { return _adapter.has_pending_settings (); }
	void clear_note_state ();

	std::vector<ReactiveRhythmMidiBufferDecision> process_buffer (MidiBuffer&, std::vector<double> const& chance_values);
	std::vector<ReactiveRhythmMidiBufferDecision> process_buffer (MidiBuffer&, ReactiveRhythmChanceSource&);
	std::vector<ReactiveRhythmMidiBufferDecision> process_buffer (MidiBuffer&, ReactiveRhythmChanceSource&, ReactiveRhythmStepSource const&);

private:
	std::vector<ReactiveRhythmMidiBufferDecision> apply_buffer_decisions (
		MidiBuffer&,
		std::vector<ReactiveRhythmMidiBufferDecision>,
		std::vector<ReactiveRhythmMidiBytes> const&);

	static ReactiveRhythmMidiBytes bytes_for_event (
		Evoral::Event<samplepos_t> const&,
		size_t step,
		double chance_value);

	ReactiveRhythmMidiByteAdapter _adapter;
};

} // namespace ARDOUR
