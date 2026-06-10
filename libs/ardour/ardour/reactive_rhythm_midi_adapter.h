#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <utility>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_rhythm.h"

namespace ARDOUR {

enum class ReactiveRhythmMidiEventType {
	NoteOn,
	NoteOff
};

struct LIBARDOUR_API ReactiveRhythmMidiEvent {
	static ReactiveRhythmMidiEvent note_on (size_t step, size_t frame, int channel, int note, int velocity, double chance_value);
	static ReactiveRhythmMidiEvent note_off (size_t frame, int channel, int note);

	ReactiveRhythmMidiEventType type = ReactiveRhythmMidiEventType::NoteOn;
	size_t step = 0;
	size_t frame = 0;
	int channel = 0;
	int note = 0;
	int velocity = 0;
	double chance_value = 0.0;
};

struct LIBARDOUR_API ReactiveRhythmMidiDecision {
	ReactiveRhythmMidiEvent event;
	bool forward = false;
	ReactiveRhythmDecision rhythm;
};

class LIBARDOUR_API ReactiveRhythmMidiAdapter {
public:
	ReactiveRhythmMidiAdapter ();
	explicit ReactiveRhythmMidiAdapter (ReactiveRhythmSettings const&);

	ReactiveRhythmSettings const& settings () const { return _rhythm.settings (); }

	void set_settings (ReactiveRhythmSettings const&);
	void queue_settings (ReactiveRhythmSettings const&);
	void advance_to_step (size_t step);
	bool has_pending_settings () const { return _rhythm.has_pending_settings (); }
	void clear_note_state ();

	std::vector<ReactiveRhythmMidiDecision> process_events (std::vector<ReactiveRhythmMidiEvent> const&);

private:
	typedef std::pair<int, int> NoteKey;

	static bool is_note_on (ReactiveRhythmMidiEvent const&);
	static bool is_note_off (ReactiveRhythmMidiEvent const&);
	static ReactiveRhythmEvent rhythm_event (ReactiveRhythmMidiEvent const&);
	static NoteKey note_key (ReactiveRhythmMidiEvent const&);

	bool process_note_off (ReactiveRhythmMidiEvent const&);

	ReactiveRhythmState _rhythm;
	std::map<NoteKey, std::deque<bool> > _note_history;
};

} // namespace ARDOUR
