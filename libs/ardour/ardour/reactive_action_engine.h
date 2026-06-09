#pragma once

#include <cstddef>
#include <map>
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
	static bool from_midi_bytes (unsigned char const* bytes, size_t size, ReactiveMidiEvent& event);

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

struct LIBARDOUR_API ReactiveActionPlan {
	bool ok = false;
	std::string error;
	std::string action_name;
	size_t action_index = 0;
	ReactiveChainMode chain_mode = ReactiveChainMode::All;
	Temporal::BBT_Offset quantize;
	std::vector<ReactiveCommand> commands;
};

class LIBARDOUR_API ReactiveActionEngine {
public:
	bool load_document (ReactiveActionDocument const&, std::string& error);

	/* MVP helper: returns an allocating vector, so it is not yet suitable for a realtime MIDI path. */
	std::vector<ReactiveActionMatch> match_midi_event (ReactiveMidiEvent const&) const;

	ReactiveActionPlan preview_action (std::string const& name) const;
	ReactiveActionPlan trigger_action (std::string const& name);
	double macro_value (std::string const& name) const;
	std::string state_value (std::string const& name) const;
	std::string last_action () const { return _last_action; }

	ReactiveActionDocument const& document () const { return _document; }

private:
	void apply_command_state (ReactiveCommand const&);

	ReactiveActionDocument _document;
	std::map<std::string, size_t> _sequential_positions;
	std::map<std::string, double> _macros;
	std::map<std::string, std::string> _states;
	std::string _last_action;
};

} // namespace ARDOUR
