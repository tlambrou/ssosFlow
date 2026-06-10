/*
 * Copyright (C) 2026 Tassos Lambrou
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <functional>

#include "midi++/parser.h"

#include "generic_midi_control_protocol.h"
#include "midireactiveaction.h"

using namespace MIDI;
using namespace std::placeholders;

MIDIReactiveAction::MIDIReactiveAction (MIDI::Parser& p)
	: _parser (p)
{
}

MIDIReactiveAction::~MIDIReactiveAction ()
{
}

int
MIDIReactiveAction::init (GenericMidiControlProtocol& ui, const std::string& reactive_name)
{
	_ui = &ui;
	_reactive_name = reactive_name;
	return 0;
}

void
MIDIReactiveAction::bind_midi (channel_t chn, eventType ev, MIDI::byte additional)
{
	_midi_sense_connection[0].disconnect ();
	_midi_sense_connection[1].disconnect ();

	_control_type = ev;
	_control_channel = chn;
	_control_additional = additional;

	int const chn_i = chn;
	switch (ev) {
	case MIDI::on:
		_parser.channel_note_on[chn_i].connect_same_thread (_midi_sense_connection[0], std::bind (&MIDIReactiveAction::midi_sense_note_on, this, _1, _2));
		break;
	case MIDI::controller:
		_parser.channel_controller[chn_i].connect_same_thread (_midi_sense_connection[0], std::bind (&MIDIReactiveAction::midi_sense_controller, this, _1, _2));
		break;
	default:
		break;
	}
}

void
MIDIReactiveAction::midi_sense_note_on (Parser&, EventTwoBytes* msg)
{
	if (!_ui || !msg || msg->note_number != _control_additional || msg->velocity == 0) {
		return;
	}

	unsigned char const bytes[] = {
		static_cast<unsigned char> (MIDI::on | (_control_channel & 0x0f)),
		static_cast<unsigned char> (msg->note_number),
		static_cast<unsigned char> (msg->velocity)
	};

	_ui->reactive_midi_bytes (bytes, sizeof (bytes));
}

void
MIDIReactiveAction::midi_sense_controller (Parser&, EventTwoBytes* msg)
{
	if (!_ui || !msg || msg->controller_number != _control_additional) {
		return;
	}

	unsigned char const bytes[] = {
		static_cast<unsigned char> (MIDI::controller | (_control_channel & 0x0f)),
		static_cast<unsigned char> (msg->controller_number),
		static_cast<unsigned char> (msg->value)
	};

	_ui->reactive_midi_bytes (bytes, sizeof (bytes));
}
