#include "ardour/reactive_action_slot_runner.h"

#include <sstream>

using namespace ARDOUR;

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
