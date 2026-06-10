#pragma once

#include <cstddef>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_rhythm_midi_adapter.h"

namespace ARDOUR {

struct LIBARDOUR_API ReactiveRhythmMidiBytes {
	static ReactiveRhythmMidiBytes three_byte (
		size_t step,
		size_t frame,
		unsigned char status,
		unsigned char data1,
		unsigned char data2,
		double chance_value);

	size_t step = 0;
	size_t frame = 0;
	size_t size = 0;
	unsigned char data[3] = { 0, 0, 0 };
	double chance_value = 0.0;
};

struct LIBARDOUR_API ReactiveRhythmMidiByteDecision {
	ReactiveRhythmMidiBytes raw;
	bool mapped = false;
	bool forward = true;
	ReactiveRhythmMidiEvent mapped_event;
	ReactiveRhythmMidiDecision adapter_decision;
};

class LIBARDOUR_API ReactiveRhythmMidiByteAdapter {
public:
	ReactiveRhythmMidiByteAdapter ();
	explicit ReactiveRhythmMidiByteAdapter (ReactiveRhythmSettings const&);

	ReactiveRhythmSettings const& settings () const { return _adapter.settings (); }

	void set_settings (ReactiveRhythmSettings const&);
	void queue_settings (ReactiveRhythmSettings const&);
	void advance_to_step (size_t step);
	bool has_pending_settings () const { return _adapter.has_pending_settings (); }
	void clear_note_state ();

	std::vector<ReactiveRhythmMidiByteDecision> process_events (std::vector<ReactiveRhythmMidiBytes> const&);

private:
	static bool map_event (ReactiveRhythmMidiBytes const&, ReactiveRhythmMidiEvent&);

	ReactiveRhythmMidiAdapter _adapter;
};

} // namespace ARDOUR
