/*
 * Copyright (C) 2026 Tassos Lambrou <tassos@tassoslambrou.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <vector>

#include "pbd/signals.h"

#include <ytkmm/box.h>

#include "widgets/ardour_button.h"

class ReactivePerformancePanel : public Gtk::VBox
{
public:
	ReactivePerformancePanel ();

private:
	void refresh ();
	void on_map ();

	void trigger_slot (size_t slot);
	void toggle_mode ();
	void reload_document ();
	void show_status ();

	Gtk::HBox _slot_row;
	Gtk::HBox _utility_row;

	std::vector<ArdourWidgets::ArdourButton*> _slot_buttons;
	ArdourWidgets::ArdourButton _mode_button;
	ArdourWidgets::ArdourButton _reload_button;
	ArdourWidgets::ArdourButton _status_button;

	PBD::ScopedConnectionList _reactive_performance_connections;
};
