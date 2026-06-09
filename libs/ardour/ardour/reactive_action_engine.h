#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action.h"

namespace ARDOUR {

struct LIBARDOUR_API ReactiveMidiEvent {
	enum Type {
		NoteOn,
		ControlChange
	};

	static ReactiveMidiEvent note_on (int channel, int note, int velocity);
	static ReactiveMidiEvent control_change (int channel, int controller, int value);

	Type type = NoteOn;
	int channel = 0;
	int number = 0;
	int value = 0;
};

struct LIBARDOUR_API ReactiveActionMatch {
	ReactiveAction const* action = 0;
	size_t action_index = 0;
	ReactiveChainMode chain_mode = ReactiveChainMode::All;
	Temporal::BBT_Offset quantize;
};

class LIBARDOUR_API ReactiveActionEngine {
public:
	bool load_document (ReactiveActionDocument const&, std::string& error);

	/* MVP helper: returns an allocating vector, so it is not yet suitable for a realtime MIDI path. */
	std::vector<ReactiveActionMatch> match_midi_event (ReactiveMidiEvent const&) const;

	ReactiveActionDocument const& document () const { return _document; }

private:
	ReactiveActionDocument _document;
};

} // namespace ARDOUR
