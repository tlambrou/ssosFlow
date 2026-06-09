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
	error.clear ();
	return true;
}

void
ReactiveActionSlotRunner::clear ()
{
	_engine = ReactiveActionEngine ();
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
		return result;
	}

	if (slot >= action_count ()) {
		std::ostringstream msg;
		msg << "reactive action slot " << slot << " is out of range";
		result.error = msg.str ();
		return result;
	}

	ReactiveActionPlan plan = _engine.trigger_action (action_name (slot));
	return ReactiveActionExecutor::execute (plan, target);
}
