#include "ardour/reactive_action_slot_runner.h"

#include <algorithm>
#include <sstream>

#include "ardour/reactive_action_clock.h"

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

static std::string
format_chain_mode (ReactiveChainMode mode)
{
	switch (mode) {
	case ReactiveChainMode::Sequential:
		return "sequential";
	case ReactiveChainMode::Random:
		return "random";
	case ReactiveChainMode::All:
		break;
	}

	return "all";
}

static std::string
format_bbt_offset (Temporal::BBT_Offset const& offset)
{
	std::ostringstream text;
	text << offset.bars << "|" << offset.beats << "|" << offset.ticks;
	return text.str ();
}

static ReactiveActionPreviewSummary
preview_summary_from_plan (size_t slot, ReactiveAction const& action, ReactiveActionPlan const& plan)
{
	ReactiveActionPreviewSummary preview;
	if (!plan.ok) {
		return preview;
	}

	preview.available = true;
	preview.slot = slot;
	preview.action_name = action.name;
	preview.primary_trigger = primary_trigger_label (action);
	preview.chain_mode = format_chain_mode (plan.chain_mode);
	preview.quantize = format_bbt_offset (plan.quantize);
	preview.quantize_offset = plan.quantize;
	preview.command_count = plan.commands.size ();
	return preview;
}

static bool
contains_name (std::vector<std::string> const& names, std::string const& name)
{
	return std::find (names.begin (), names.end (), name) != names.end ();
}

static std::vector<std::string>
command_names_from_actions (std::vector<ReactiveAction> const& actions, ReactiveCommand::Type command_type, size_t max_slots)
{
	std::vector<std::string> names;
	if (max_slots == 0) {
		return names;
	}

	names.reserve (max_slots);

	for (std::vector<ReactiveAction>::const_iterator action = actions.begin (); action != actions.end (); ++action) {
		for (std::vector<ReactiveCommand>::const_iterator command = action->commands.begin (); command != action->commands.end (); ++command) {
			if (command->type != command_type || command->name.empty () || contains_name (names, command->name)) {
				continue;
			}

			names.push_back (command->name);
			if (names.size () >= max_slots) {
				return names;
			}
		}
	}

	return names;
}

static ReactiveExecutionResult
disabled_execution_result ()
{
	ReactiveExecutionResult result;
	result.error = "Reactive Performance Mode is disabled";
	return result;
}

static int
controller_feedback_value (bool available, bool enabled, bool latest_attempted)
{
	if (!available || !enabled) {
		return 0;
	}

	return latest_attempted ? 127 : 32;
}

static bool
valid_controller_feedback_binding (ReactiveControllerFeedbackBinding const& binding)
{
	if (binding.channel < 1 || binding.channel > 16 || binding.number < 0 || binding.number > 127) {
		return false;
	}

	return binding.type == ReactiveControllerFeedbackBinding::Note ||
		binding.type == ReactiveControllerFeedbackBinding::ControlChange;
}

static unsigned char
controller_feedback_status_byte (ReactiveControllerFeedbackBinding const& binding)
{
	unsigned char const midi_channel = static_cast<unsigned char> (binding.channel - 1);
	unsigned char const message_type = binding.type == ReactiveControllerFeedbackBinding::ControlChange ? 0xb0 : 0x90;
	return static_cast<unsigned char> (message_type | (midi_channel & 0x0f));
}

static unsigned char
bounded_controller_feedback_value (int value)
{
	return static_cast<unsigned char> (std::max (0, std::min (127, value)));
}

static bool
zero_quantize (Temporal::BBT_Offset const& quantize)
{
	return quantize.bars == 0 && quantize.beats == 0 && quantize.ticks == 0;
}

static ReactiveExecutionResult
failed_plan_result (ReactiveActionPlan const& plan)
{
	ReactiveExecutionResult result;
	result.error = plan.error.empty () ? "cannot execute failed reactive action plan" : plan.error;
	return result;
}

static ReactiveActionPlan
plan_from_queued_action (ReactiveQueuedAction const& queued)
{
	ReactiveActionPlan plan;
	plan.ok = true;
	plan.action_name = queued.action_name;
	plan.action_index = queued.slot;
	plan.quantize = queued.quantize;
	plan.commands = queued.commands;
	return plan;
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
	_scheduler.clear ();
	clear_last_execution_status ();
	clear_next_action_preview ();
	error.clear ();
	return true;
}

void
ReactiveActionSlotRunner::clear ()
{
	_engine = ReactiveActionEngine ();
	_scheduler.clear ();
	clear_last_execution_status ();
	clear_next_action_preview ();
	_loaded = false;
}

void
ReactiveActionSlotRunner::set_performance_enabled (bool enabled)
{
	if (_performance_enabled == enabled) {
		return;
	}

	_performance_enabled = enabled;
	if (!_performance_enabled) {
		_scheduler.clear ();
		clear_next_action_preview ();
	}
}

void
ReactiveActionSlotRunner::set_transport_rolling_provider (std::function<bool()> provider)
{
	_transport_rolling_provider = provider;
	refresh_transport_state ();
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

std::vector<ReactivePerformanceControlSummary>
ReactiveActionSlotRunner::performance_control_summary (size_t max_slots) const
{
	std::vector<ReactivePerformanceControlSummary> summary;
	if (max_slots == 0) {
		return summary;
	}

	summary.reserve (max_slots);

	size_t const count = _loaded ? action_count () : 0;
	for (size_t slot = 0; slot < max_slots; ++slot) {
		ReactivePerformanceControlSummary row;
		row.slot = slot;
		if (_loaded && slot < count) {
			ReactiveAction const& action = _engine.document ().actions ()[slot];
			std::ostringstream label;
			if (_last_execution_status.attempted && _last_execution_status.slot == slot) {
				label << "> ";
			}
			label << slot << " " << action.name;
			row.action_name = action.name;
			row.primary_trigger = primary_trigger_label (action);
			row.button_label = label.str ();
			row.available = true;
			row.enabled = _performance_enabled;
		} else {
			std::ostringstream label;
			label << slot << " empty";
			row.button_label = label.str ();
		}
		summary.push_back (row);
	}

	return summary;
}

std::vector<ReactiveControllerFeedbackSummary>
ReactiveActionSlotRunner::controller_feedback_summary (size_t max_slots) const
{
	std::vector<ReactiveControllerFeedbackSummary> summary;
	if (max_slots == 0) {
		return summary;
	}

	summary.reserve (max_slots);

	for (size_t slot = 0; slot < max_slots; ++slot) {
		summary.push_back (controller_feedback_summary_row (slot));
	}

	return summary;
}

std::vector<ReactiveControllerFeedbackMidiMessage>
ReactiveActionSlotRunner::controller_feedback_midi_messages (std::vector<ReactiveControllerFeedbackBinding> const& bindings) const
{
	std::vector<ReactiveControllerFeedbackMidiMessage> messages;
	if (bindings.empty ()) {
		return messages;
	}

	messages.reserve (bindings.size ());

	for (std::vector<ReactiveControllerFeedbackBinding>::const_iterator binding = bindings.begin (); binding != bindings.end (); ++binding) {
		if (!valid_controller_feedback_binding (*binding)) {
			continue;
		}

		ReactiveControllerFeedbackSummary const row = controller_feedback_summary_row (binding->slot);
		ReactiveControllerFeedbackMidiMessage message;
		message.slot = binding->slot;
		message.bytes.reserve (3);
		message.bytes.push_back (controller_feedback_status_byte (*binding));
		message.bytes.push_back (static_cast<unsigned char> (binding->number));
		message.bytes.push_back (bounded_controller_feedback_value (row.value));
		messages.push_back (message);
	}

	return messages;
}

std::vector<ReactiveMacroSlotSummary>
ReactiveActionSlotRunner::macro_bank_summary (size_t max_slots) const
{
	std::vector<ReactiveMacroSlotSummary> summary;
	if (!_loaded || max_slots == 0) {
		return summary;
	}

	std::vector<std::string> const names = command_names_from_actions (_engine.document ().actions (), ReactiveCommand::Macro, max_slots);
	summary.reserve (names.size ());

	for (size_t slot = 0; slot < names.size (); ++slot) {
		ReactiveMacroSlotSummary row;
		row.slot = slot;
		row.name = names[slot];
		row.value = _engine.macro_value (row.name);
		summary.push_back (row);
	}

	return summary;
}

std::vector<ReactiveStateSlotSummary>
ReactiveActionSlotRunner::state_bank_summary (size_t max_slots) const
{
	std::vector<ReactiveStateSlotSummary> summary;
	if (!_loaded || max_slots == 0) {
		return summary;
	}

	std::vector<std::string> const names = command_names_from_actions (_engine.document ().actions (), ReactiveCommand::State, max_slots);
	summary.reserve (names.size ());

	for (size_t slot = 0; slot < names.size (); ++slot) {
		ReactiveStateSlotSummary row;
		row.slot = slot;
		row.name = names[slot];
		row.value = _engine.state_value (row.name);
		summary.push_back (row);
	}

	return summary;
}

ReactiveActionPreviewSummary
ReactiveActionSlotRunner::preview_slot (size_t slot)
{
	if (!_loaded || slot >= action_count ()) {
		return ReactiveActionPreviewSummary ();
	}

	refresh_transport_state ();
	ReactiveAction const& action = _engine.document ().actions ()[slot];
	return preview_summary_from_plan (slot, action, _engine.preview_action (action.name));
}

ReactiveActionPreviewSummary
ReactiveActionSlotRunner::preview_midi_event (ReactiveMidiEvent const& event)
{
	if (!_loaded) {
		return ReactiveActionPreviewSummary ();
	}

	refresh_transport_state ();
	std::vector<ReactiveActionMatch> const matches = _engine.match_midi_event (event);
	if (matches.empty ()) {
		return ReactiveActionPreviewSummary ();
	}

	ReactiveActionMatch const& match = matches.front ();
	if (!match.action) {
		return ReactiveActionPreviewSummary ();
	}

	return preview_summary_from_plan (match.action_index, *match.action, _engine.preview_action (match.action->name));
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_or_queue_slot (
	size_t slot,
	ReactiveActionTarget& target,
	Temporal::BBT_Time const& requested_at,
	Temporal::BBT_Time const& due_at)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		clear_next_action_preview ();
		return record_execution_status (slot, std::string (), result);
	}

	if (slot >= action_count ()) {
		std::ostringstream msg;
		msg << "reactive action slot " << slot << " is out of range";
		result.error = msg.str ();
		clear_next_action_preview ();
		return record_execution_status (slot, std::string (), result);
	}

	std::string const name = action_name (slot);
	if (!_performance_enabled) {
		clear_next_action_preview ();
		return record_execution_status (slot, name, disabled_execution_result ());
	}

	refresh_transport_state ();
	ReactiveAction const& action = _engine.document ().actions ()[slot];
	ReactiveActionPlan plan = _engine.trigger_action (name);
	if (!plan.ok) {
		clear_next_action_preview ();
		return record_execution_status (slot, plan.action_name.empty () ? name : plan.action_name, failed_plan_result (plan));
	}

	if (zero_quantize (plan.quantize)) {
		result = ReactiveActionExecutor::execute (plan, target);
		_next_action_preview = preview_slot (slot);
		return record_execution_status (slot, plan.action_name.empty () ? name : plan.action_name, result);
	}

	result = queue_plan (slot, primary_trigger_label (action), plan, requested_at, due_at);
	_next_action_preview = preview_slot (slot);
	return record_execution_status (slot, plan.action_name.empty () ? name : plan.action_name, result);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_or_queue_slot (
	size_t slot,
	ReactiveActionTarget& target,
	Temporal::TempoMap const& tempo_map,
	Temporal::BBT_Time const& requested_at)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		clear_next_action_preview ();
		return record_execution_status (slot, std::string (), result);
	}

	if (slot >= action_count ()) {
		std::ostringstream msg;
		msg << "reactive action slot " << slot << " is out of range";
		result.error = msg.str ();
		clear_next_action_preview ();
		return record_execution_status (slot, std::string (), result);
	}

	std::string const name = action_name (slot);
	if (!_performance_enabled) {
		clear_next_action_preview ();
		return record_execution_status (slot, name, disabled_execution_result ());
	}

	refresh_transport_state ();
	ReactiveAction const& action = _engine.document ().actions ()[slot];
	ReactiveActionPlan plan = _engine.trigger_action (name);
	if (!plan.ok) {
		clear_next_action_preview ();
		return record_execution_status (slot, plan.action_name.empty () ? name : plan.action_name, failed_plan_result (plan));
	}

	ReactiveActionClockPosition const position = ReactiveActionClock::quantize_bbt (tempo_map, requested_at, plan.quantize);
	if (zero_quantize (plan.quantize)) {
		result = ReactiveActionExecutor::execute (plan, target);
		_next_action_preview = preview_slot (slot);
		return record_execution_status (slot, plan.action_name.empty () ? name : plan.action_name, result);
	}

	result = queue_plan (slot, primary_trigger_label (action), plan, position.requested_at, position.due_at);
	_next_action_preview = preview_slot (slot);
	return record_execution_status (slot, plan.action_name.empty () ? name : plan.action_name, result);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_or_queue_midi_event (
	ReactiveMidiEvent const& event,
	ReactiveActionTarget& target,
	Temporal::BBT_Time const& requested_at,
	Temporal::BBT_Time const& due_at)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	std::vector<ReactiveActionMatch> matches = _engine.match_midi_event (event);
	if (matches.empty ()) {
		result.error = "no reactive action matched MIDI event";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	ReactiveActionMatch const& match = matches.front ();
	std::string const name = match.action ? match.action->name : std::string ();
	if (name.empty ()) {
		result.error = "matched reactive MIDI action has no name";
		clear_next_action_preview ();
		return record_execution_status (match.action_index, std::string (), result);
	}

	if (!_performance_enabled) {
		clear_next_action_preview ();
		return record_execution_status (match.action_index, name, disabled_execution_result ());
	}

	refresh_transport_state ();
	ReactiveActionPlan plan = _engine.trigger_action (name, &event);
	if (!plan.ok) {
		clear_next_action_preview ();
		return record_execution_status (match.action_index, plan.action_name.empty () ? name : plan.action_name, failed_plan_result (plan));
	}

	if (zero_quantize (plan.quantize)) {
		result = ReactiveActionExecutor::execute (plan, target);
		_next_action_preview = preview_midi_event (event);
		return record_execution_status (match.action_index, plan.action_name.empty () ? name : plan.action_name, result);
	}

	result = queue_plan (match.action_index, match.action ? primary_trigger_label (*match.action) : std::string (), plan, requested_at, due_at);
	_next_action_preview = preview_midi_event (event);
	return record_execution_status (match.action_index, plan.action_name.empty () ? name : plan.action_name, result);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_or_queue_midi_event (
	ReactiveMidiEvent const& event,
	ReactiveActionTarget& target,
	Temporal::TempoMap const& tempo_map,
	Temporal::BBT_Time const& requested_at)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	std::vector<ReactiveActionMatch> matches = _engine.match_midi_event (event);
	if (matches.empty ()) {
		result.error = "no reactive action matched MIDI event";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	ReactiveActionMatch const& match = matches.front ();
	std::string const name = match.action ? match.action->name : std::string ();
	if (name.empty ()) {
		result.error = "matched reactive MIDI action has no name";
		clear_next_action_preview ();
		return record_execution_status (match.action_index, std::string (), result);
	}

	if (!_performance_enabled) {
		clear_next_action_preview ();
		return record_execution_status (match.action_index, name, disabled_execution_result ());
	}

	refresh_transport_state ();
	ReactiveActionPlan plan = _engine.trigger_action (name, &event);
	if (!plan.ok) {
		clear_next_action_preview ();
		return record_execution_status (match.action_index, plan.action_name.empty () ? name : plan.action_name, failed_plan_result (plan));
	}

	ReactiveActionClockPosition const position = ReactiveActionClock::quantize_bbt (tempo_map, requested_at, plan.quantize);
	if (zero_quantize (plan.quantize)) {
		result = ReactiveActionExecutor::execute (plan, target);
		_next_action_preview = preview_midi_event (event);
		return record_execution_status (match.action_index, plan.action_name.empty () ? name : plan.action_name, result);
	}

	result = queue_plan (match.action_index, match.action ? primary_trigger_label (*match.action) : std::string (), plan, position.requested_at, position.due_at);
	_next_action_preview = preview_midi_event (event);
	return record_execution_status (match.action_index, plan.action_name.empty () ? name : plan.action_name, result);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_or_queue_midi_bytes (
	unsigned char const* bytes,
	size_t size,
	ReactiveActionTarget& target,
	Temporal::TempoMap const& tempo_map,
	Temporal::BBT_Time const& requested_at)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	ReactiveMidiEvent event;
	if (!ReactiveMidiEvent::from_midi_bytes (bytes, size, event)) {
		result.error = "unsupported reactive MIDI byte message";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	return execute_or_queue_midi_event (event, target, tempo_map, requested_at);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_slot (size_t slot, ReactiveActionTarget& target)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		clear_next_action_preview ();
		return record_execution_status (slot, std::string (), result);
	}

	if (slot >= action_count ()) {
		std::ostringstream msg;
		msg << "reactive action slot " << slot << " is out of range";
		result.error = msg.str ();
		clear_next_action_preview ();
		return record_execution_status (slot, std::string (), result);
	}

	std::string const name = action_name (slot);
	if (!_performance_enabled) {
		clear_next_action_preview ();
		return record_execution_status (slot, name, disabled_execution_result ());
	}

	refresh_transport_state ();
	ReactiveActionPlan plan = _engine.trigger_action (name);
	result = ReactiveActionExecutor::execute (plan, target);
	_next_action_preview = preview_slot (slot);
	return record_execution_status (slot, plan.action_name.empty () ? name : plan.action_name, result);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_midi_event (ReactiveMidiEvent const& event, ReactiveActionTarget& target)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	std::vector<ReactiveActionMatch> matches = _engine.match_midi_event (event);
	if (matches.empty ()) {
		result.error = "no reactive action matched MIDI event";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	ReactiveActionMatch const& match = matches.front ();
	std::string const name = match.action ? match.action->name : std::string ();
	if (name.empty ()) {
		result.error = "matched reactive MIDI action has no name";
		clear_next_action_preview ();
		return record_execution_status (match.action_index, std::string (), result);
	}

	if (!_performance_enabled) {
		clear_next_action_preview ();
		return record_execution_status (match.action_index, name, disabled_execution_result ());
	}

	refresh_transport_state ();
	ReactiveActionPlan plan = _engine.trigger_action (name, &event);
	result = ReactiveActionExecutor::execute (plan, target);
	_next_action_preview = preview_midi_event (event);
	return record_execution_status (match.action_index, plan.action_name.empty () ? name : plan.action_name, result);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::execute_midi_bytes (unsigned char const* bytes, size_t size, ReactiveActionTarget& target)
{
	ReactiveExecutionResult result;

	if (!_loaded) {
		result.error = "no reactive action document loaded";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	ReactiveMidiEvent event;
	if (!ReactiveMidiEvent::from_midi_bytes (bytes, size, event)) {
		result.error = "unsupported reactive MIDI byte message";
		clear_next_action_preview ();
		return record_execution_status (0, std::string (), result);
	}

	return execute_midi_event (event, target);
}

ReactiveExecutionResult
ReactiveActionSlotRunner::release_due_queued_actions (Temporal::BBT_Time const& now, ReactiveActionTarget& target)
{
	ReactiveExecutionResult aggregate;
	aggregate.ok = true;

	if (!_performance_enabled) {
		return disabled_execution_result ();
	}

	std::vector<ReactiveQueuedAction> const due = _scheduler.pop_due (now);
	for (std::vector<ReactiveQueuedAction>::const_iterator queued = due.begin (); queued != due.end (); ++queued) {
		ReactiveActionPlan const plan = plan_from_queued_action (*queued);
		ReactiveExecutionResult result = ReactiveActionExecutor::execute (plan, target);
		record_execution_status (queued->slot, queued->action_name, result);

		if (!result.ok) {
			aggregate.ok = false;
			aggregate.error = result.error;
			aggregate.commands_executed += result.commands_executed;
			return aggregate;
		}

		aggregate.commands_executed += result.commands_executed;
	}

	return aggregate;
}

void
ReactiveActionSlotRunner::refresh_transport_state ()
{
	if (_transport_rolling_provider) {
		_engine.set_transport_rolling (_transport_rolling_provider ());
	}
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
ReactiveActionSlotRunner::format_performance_mode_status () const
{
	return std::string ("Reactive Performance Mode: ") + (_performance_enabled ? "enabled" : "disabled");
}

std::string
ReactiveActionSlotRunner::format_performance_control_summary (size_t max_slots) const
{
	std::vector<ReactivePerformanceControlSummary> const summary = performance_control_summary (max_slots);

	if (summary.empty ()) {
		return "Performance controls: none";
	}

	std::ostringstream text;
	text << "Performance controls: " << (_performance_enabled ? "enabled" : "disabled");
	for (std::vector<ReactivePerformanceControlSummary>::const_iterator i = summary.begin (); i != summary.end (); ++i) {
		text << "\n  " << i->slot << ": ";
		if (!i->available) {
			text << "empty - unavailable";
			continue;
		}

		text << i->action_name << " [" << i->primary_trigger << "] - " << (i->enabled ? "enabled" : "disabled");
	}

	return text.str ();
}

std::string
ReactiveActionSlotRunner::format_next_action_preview () const
{
	if (!_next_action_preview.available) {
		return "Next action preview: none";
	}

	std::ostringstream status;
	status << "Next action preview: slot " << _next_action_preview.slot << " (" << _next_action_preview.action_name << ")";
	if (!_next_action_preview.primary_trigger.empty ()) {
		status << " [" << _next_action_preview.primary_trigger << "]";
	}
	status << " - " << _next_action_preview.chain_mode << ", quantize " << _next_action_preview.quantize << ", " << _next_action_preview.command_count << " command";
	if (_next_action_preview.command_count != 1) {
		status << "s";
	}

	return status.str ();
}

std::string
ReactiveActionSlotRunner::format_queued_action_summary (size_t max_items, Temporal::BBT_Time const& now) const
{
	std::vector<ReactiveQueuedActionSummary> const summary = queued_action_summary (max_items, now);
	if (summary.empty ()) {
		return "Queued actions: none";
	}

	std::ostringstream text;
	text << "Queued actions:";
	for (std::vector<ReactiveQueuedActionSummary>::const_iterator i = summary.begin (); i != summary.end (); ++i) {
		text << "\n  #" << i->id << " slot " << i->slot << " " << i->action_name
		     << " [" << i->primary_trigger << "]"
		     << " due " << i->due_at
		     << " q " << i->quantize
		     << " - " << i->command_count << " command";
		if (i->command_count != 1) {
			text << "s";
		}
		if (i->due) {
			text << " - due";
		}
	}

	return text.str ();
}

std::string
ReactiveActionSlotRunner::format_panel_summary (size_t max_items) const
{
	std::ostringstream text;

	if (_next_action_preview.available) {
		text << "Next: slot " << _next_action_preview.slot << " " << _next_action_preview.action_name
		     << " - " << _next_action_preview.chain_mode << ", q " << _next_action_preview.quantize
		     << ", " << _next_action_preview.command_count << " cmd";
		if (_next_action_preview.command_count != 1) {
			text << "s";
		}
	} else {
		text << "Next: none";
	}

	std::vector<ReactiveMacroSlotSummary> const macros = macro_bank_summary (max_items);
	text << "\nMacros:";
	if (macros.empty ()) {
		text << " none";
	} else {
		for (std::vector<ReactiveMacroSlotSummary>::const_iterator i = macros.begin (); i != macros.end (); ++i) {
			text << (i == macros.begin () ? " " : ", ") << i->name << "=" << i->value;
		}
	}

	std::vector<ReactiveStateSlotSummary> const states = state_bank_summary (max_items);
	text << "\nStates:";
	if (states.empty ()) {
		text << " none";
	} else {
		for (std::vector<ReactiveStateSlotSummary>::const_iterator i = states.begin (); i != states.end (); ++i) {
			text << (i == states.begin () ? " " : ", ") << i->name << "=" << i->value;
		}
	}

	return text.str ();
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

std::string
ReactiveActionSlotRunner::format_macro_bank_summary (size_t max_slots) const
{
	std::vector<ReactiveMacroSlotSummary> const summary = macro_bank_summary (max_slots);

	if (summary.empty ()) {
		return "Macro bank: none";
	}

	std::ostringstream text;
	text << "Macro bank:";
	for (std::vector<ReactiveMacroSlotSummary>::const_iterator i = summary.begin (); i != summary.end (); ++i) {
		text << "\n  " << i->slot << ": " << i->name << " = " << i->value;
	}

	return text.str ();
}

std::string
ReactiveActionSlotRunner::format_state_bank_summary (size_t max_slots) const
{
	std::vector<ReactiveStateSlotSummary> const summary = state_bank_summary (max_slots);

	if (summary.empty ()) {
		return "State bank: none";
	}

	std::ostringstream text;
	text << "State bank:";
	for (std::vector<ReactiveStateSlotSummary>::const_iterator i = summary.begin (); i != summary.end (); ++i) {
		text << "\n  " << i->slot << ": " << i->name << " = " << i->value;
	}

	return text.str ();
}

void
ReactiveActionSlotRunner::clear_last_execution_status ()
{
	_last_execution_status = ReactiveActionSlotExecutionStatus ();
}

void
ReactiveActionSlotRunner::clear_next_action_preview ()
{
	_next_action_preview = ReactiveActionPreviewSummary ();
}

ReactiveControllerFeedbackSummary
ReactiveActionSlotRunner::controller_feedback_summary_row (size_t slot) const
{
	ReactiveControllerFeedbackSummary row;
	row.slot = slot;

	size_t const count = _loaded ? action_count () : 0;
	if (_loaded && slot < count) {
		ReactiveAction const& action = _engine.document ().actions ()[slot];
		row.action_name = action.name;
		row.primary_trigger = primary_trigger_label (action);
		row.available = true;
		row.enabled = _performance_enabled;
		row.latest_attempted = _performance_enabled &&
			_last_execution_status.attempted &&
			_last_execution_status.slot == slot;
	}

	row.value = controller_feedback_value (row.available, row.enabled, row.latest_attempted);
	return row;
}

ReactiveExecutionResult
ReactiveActionSlotRunner::queue_plan (
	size_t slot,
	std::string const& primary_trigger,
	ReactiveActionPlan const& plan,
	Temporal::BBT_Time const& requested_at,
	Temporal::BBT_Time const& due_at)
{
	ReactiveExecutionResult result;
	if (!plan.ok) {
		return failed_plan_result (plan);
	}

	_scheduler.queue_action (slot, primary_trigger, plan, requested_at, due_at);
	result.ok = true;
	return result;
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
