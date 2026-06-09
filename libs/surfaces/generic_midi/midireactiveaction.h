/*
 * Copyright (C) 2026 Tassos Lambrou
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __gm_midireactiveaction_h__
#define __gm_midireactiveaction_h__

#include <string>

#include "midi++/types.h"

#include "pbd/signals.h"

namespace MIDI {
	class Parser;
}

class GenericMidiControlProtocol;

class MIDIReactiveAction
{
  public:
	MIDIReactiveAction (MIDI::Parser&);
	~MIDIReactiveAction ();

	int init (GenericMidiControlProtocol&, const std::string& reactive_name);

	void bind_midi (MIDI::channel_t, MIDI::eventType, MIDI::byte);
	MIDI::channel_t get_control_channel () { return _control_channel; }
	MIDI::eventType get_control_type () { return _control_type; }
	MIDI::byte get_control_additional () { return _control_additional; }

  private:
	void midi_sense_note_on (MIDI::Parser&, MIDI::EventTwoBytes*);
	void midi_sense_controller (MIDI::Parser&, MIDI::EventTwoBytes*);

	GenericMidiControlProtocol* _ui = 0;
	std::string _reactive_name;
	MIDI::Parser& _parser;
	PBD::ScopedConnection _midi_sense_connection[2];
	MIDI::eventType _control_type = MIDI::none;
	MIDI::byte _control_additional = 0;
	MIDI::channel_t _control_channel = 0;
};

#endif // __gm_midireactiveaction_h__
