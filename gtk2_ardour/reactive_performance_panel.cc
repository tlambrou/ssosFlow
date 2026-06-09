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

#include "reactive_performance_panel.h"

#include <sstream>

#include "ardour_ui.h"

#include "pbd/i18n.h"

using namespace ArdourWidgets;

ReactivePerformancePanel::ReactivePerformancePanel ()
{
	set_name ("reactive performance panel");
	set_border_width (2);
	set_spacing (2);

	_slot_row.set_spacing (2);
	_utility_row.set_spacing (2);

	_slot_buttons.reserve (8);
	for (size_t slot = 0; slot < 8; ++slot) {
		ArdourButton* button = Gtk::manage (new ArdourButton (ArdourButton::Text));
		std::ostringstream label;
		label << "R" << slot;
		button->set_name ("generic button");
		button->set_text (label.str ());
		button->set_size_request (52, 36);
		button->signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &ReactivePerformancePanel::trigger_slot), slot));
		_slot_row.pack_start (*button, false, false);
		_slot_buttons.push_back (button);
	}

	_mode_button.set_name ("generic button");
	_mode_button.set_text (_("Mode"));
	_mode_button.set_size_request (76, 32);
	_mode_button.signal_clicked.connect (sigc::mem_fun (*this, &ReactivePerformancePanel::toggle_mode));

	_reload_button.set_name ("generic button");
	_reload_button.set_text (_("Reload"));
	_reload_button.set_size_request (76, 32);
	_reload_button.signal_clicked.connect (sigc::mem_fun (*this, &ReactivePerformancePanel::reload_document));

	_status_button.set_name ("generic button");
	_status_button.set_text (_("Status"));
	_status_button.set_size_request (76, 32);
	_status_button.signal_clicked.connect (sigc::mem_fun (*this, &ReactivePerformancePanel::show_status));

	_utility_row.pack_start (_mode_button, false, false);
	_utility_row.pack_start (_reload_button, false, false);
	_utility_row.pack_start (_status_button, false, false);

	pack_start (_slot_row, false, false);
	pack_start (_utility_row, false, false);
}

void
ReactivePerformancePanel::trigger_slot (size_t slot)
{
	ARDOUR_UI::instance ()->trigger_reactive_action (static_cast<int> (slot));
}

void
ReactivePerformancePanel::toggle_mode ()
{
	ARDOUR_UI::instance ()->toggle_reactive_performance_mode ();
}

void
ReactivePerformancePanel::reload_document ()
{
	ARDOUR_UI::instance ()->reload_reactive_action_document ();
}

void
ReactivePerformancePanel::show_status ()
{
	ARDOUR_UI::instance ()->show_reactive_action_document_status ();
}
