#include "ardour/reactive_action_slot_runner.h"

#include <algorithm>
#include <sstream>

using namespace ARDOUR;

namespace {

static std::string
format_trigger_label (ReactiveTrigger const& trigger)
{
	std::ostringstream label;

	switch (trigger.type) {
	case ReactiveTrigger::MidiNote:
		label << "MIDI note ch=" << trigger.channel << " note=" << trigger.number;
		return label.str ();
	case ReactiveTrigger::MidiCC:
		label << "MIDI cc ch=" << trigger.channel << " cc=" << trigger.number;
		if (trigger.threshold >= 0) {
			label << " value>" << trigger.threshold;
		}
		return label.str ();
	case ReactiveTrigger::Marker:
		label << "marker " << trigger.name;
		return label.str ();
	case ReactiveTrigger::None:
		break;
	}

	return "manual";
}

static std::string
primary_trigger_label (ReactiveAction const& action)
{
	if (action.triggers.empty ()) {
		return "manual";
	}

	return format_trigger_label (action.triggers.front ());
}

} // namespace

bool
ReactiveActionSlotRunner::load_source (std::string const& source, std::string& error)
{
	ReactiveActionParseResult parsed = ReactiveActionDocument::parse (source);
	if (!parsed.ok) {
		clear ();
		error = parsed.error.empty () ? "failed to parse reactive action source" : parsed.error;
		return false;
	}

	return load_document (parsed.document, error);
}

bool
ReactiveActionSlotRunner::load_document (ReactiveActionDocument const& document, std::string& error)
{
	if (!_engine.load_document (document, error)) {
		clear ();
		return false;
	}

	_loaded = true;
	clear_last_execution_status ();
	error.clear ();
	return true;
}

void
ReactiveActionSlotRunner::clear ()
{
	_engine = ReactiveActionEngine ();
	clear_last_execution_status ();
	_loaded = false;
}

size_t
ReactiveActionSlotRunner::action_count () const
{
	return _loaded ? _engine.document ().actions ().size () : 0;
}

std::string
ReactiveActionSlotRunner::action_name (size_t slot) const
{
	if (!_loaded || slot >= action_count ()) {
		return std::string ();
	}

	return _engine.document ().actions ()[slot].name;
}

std::vector<ReactiveActionSlotSummary>
ReactiveActionSlotRunner::action_bank_summary (size_t max_slots) const
{
	std::vector<ReactiveActionSlotSummary> summary;
	if (!_loaded || max_slots == 0) {
		return summary;
	}

	std::vector<ReactiveAction> const& actions = _engine.document ().actions ();
	size_t const count = std::min (max_slots, actions.size ());
	summary.reserve (count);

	for (size_t slot = 0; slot < count; ++slot) {
		ReactiveAction const& action = actions[slot];
		ReactiveActionSlotSummary row;
		row.slot = slot;
		row.action_name = action.name;
		row.primary_trigger = primary_trigger_label (action);
		row.command_count = action.commands.size ();
		row.latest_attempted = _last_execution_status.attempted && _last_execution_status.slot == slot;
		summary.push_back (row);
	}

	return summary;
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_slot (size_t slot, ReactiveActionTarget& target)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		return record_execution_status (slot, std::string (), result);
	}

	if (slot >= action_count ()) {
		std::ostringstream msg;
		msg << "reactive action slot " << slot << " is out of range";
		result.error = msg.str ();
		return record_execution_status (slot, std::string (), result);
	}

	std::string const name = action_name (slot);
	ReactiveActionPlan plan = _engine.trigger_action (name);
	result = ReactiveActionExecutor::execute (plan, target);
	return record_execution_status (slot, plan.action_name.empty () ? name : plan.action_name, result);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_midi_event (ReactiveMidiEvent const& event, ReactiveActionTarget& target)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		return record_execution_status (0, std::string (), result);
	}

	std::vector<ReactiveActionMatch> matches = _engine.match_midi_event (event);
	if (matches.empty ()) {
		result.error = "no reactive action matched MIDI event";
		return record_execution_status (0, std::string (), result);
	}

	ReactiveActionMatch const& match = matches.front ();
	std::string const name = match.action ? match.action->name : std::string ();
	if (name.empty ()) {
		result.error = "matched reactive MIDI action has no name";
		return record_execution_status (match.action_index, std::string (), result);
	}

	ReactiveActionPlan plan = _engine.trigger_action (name);
	result = ReactiveActionExecutor::execute (plan, target);
	return record_execution_status (match.action_index, plan.action_name.empty () ? name : plan.action_name, result);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_midi_bytes (unsigned char const* bytes, size_t size, ReactiveActionTarget& target)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		return record_execution_status (0, std::string (), result);
	}

	ReactiveMidiEvent event;
	if (!ReactiveMidiEvent::from_midi_bytes (bytes, size, event)) {
		result.error = "unsupported reactive MIDI byte message";
		return record_execution_status (0, std::string (), result);
	}

	return execute_midi_event (event, target);
}

std::string
ReactiveActionSlotRunner::format_last_execution_status () const
{
	if (!_last_execution_status.attempted) {
		return "Last execution: none";
	}

	std::ostringstream status;
	status << "Last execution: slot " << _last_execution_status.slot;
	if (!_last_execution_status.action_name.empty ()) {
		status << " (" << _last_execution_status.action_name << ")";
	}

	if (_last_execution_status.result.ok) {
		status << ": OK, " << _last_execution_status.result.commands_executed << " command";
		if (_last_execution_status.result.commands_executed != 1) {
			status << "s";
		}
		return status.str ();
	}

	status << ": failed";
	if (!_last_execution_status.result.error.empty ()) {
		status << " - " << _last_execution_status.result.error;
	}

	return status.str ();
}

std::string
ReactiveActionSlotRunner::format_action_bank_summary (size_t max_slots) const
{
	std::vector<ReactiveActionSlotSummary> const summary = action_bank_summary (max_slots);

	if (summary.empty ()) {
		return "Action bank: none";
	}

	std::ostringstream text;
	text << "Action bank:";
	for (std::vector<ReactiveActionSlotSummary>::const_iterator i = summary.begin (); i != summary.end (); ++i) {
		text << "\n";
		if (i->latest_attempted) {
			text << "> ";
		} else {
			text << "  ";
		}
		text << i->slot << ": " << i->action_name << " [" << i->primary_trigger << "] - " << i->command_count << " command";
		if (i->command_count != 1) {
			text << "s";
		}
	}

	return text.str ();
}

void
ReactiveActionSlotRunner::clear_last_execution_status ()
{
	_last_execution_status = ReactiveActionSlotExecutionStatus ();
}

ReactiveExecutionResult
ReactiveActionSlotRunner::record_execution_status (
	size_t slot,
	std::string const& action_name,
	ReactiveExecutionResult const& result)
{
	_last_execution_status.attempted = true;
	_last_execution_status.slot = slot;
	_last_execution_status.action_name = action_name;
	_last_execution_status.result = result;
	return result;
}
