#include "ardour/reactive_action_scheduler.h"

#include <algorithm>
#include <sstream>

using namespace ARDOUR;

namespace {

static bool
queued_action_due_less (ReactiveQueuedAction const& a, ReactiveQueuedAction const& b)
{
	if (a.due_at != b.due_at) {
		return a.due_at < b.due_at;
	}

	return a.id < b.id;
}

static std::string
format_preview_value (double value)
{
	std::ostringstream text;
	text << value;
	return text.str ();
}

static std::string
format_command_bbt_offset (Temporal::BBT_Offset const& offset)
{
	std::ostringstream text;
	text << offset.bars << "|" << offset.beats << "|" << offset.ticks;
	return text.str ();
}

static std::string
format_command_summary (ReactiveCommand const& command)
{
	std::ostringstream text;

	switch (command.type) {
	case ReactiveCommand::Cue:
		text << "cue row " << command.first;
		return text.str ();
	case ReactiveCommand::Trigger:
		text << "trigger route " << command.first << " row " << command.second;
		return text.str ();
	case ReactiveCommand::TriggerStop:
		text << "stop triggers on route " << command.first;
		return text.str ();
	case ReactiveCommand::StopAll:
		return "stop all triggers";
	case ReactiveCommand::TransportPlay:
		return "transport play";
	case ReactiveCommand::TransportStop:
		text << "transport stop after " << format_command_bbt_offset (command.ramp);
		return text.str ();
	case ReactiveCommand::SceneApply:
		text << "scene apply " << command.first;
		return text.str ();
	case ReactiveCommand::SceneStore:
		text << "scene store " << command.first;
		return text.str ();
	case ReactiveCommand::Macro:
		text << "macro " << command.name << " = " << format_preview_value (command.value) << " ramp " << format_command_bbt_offset (command.ramp);
		return text.str ();
	case ReactiveCommand::MacroSnapshotStore:
		text << "macro snapshot store " << command.name;
		return text.str ();
	case ReactiveCommand::MacroSnapshotRecall:
		text << "macro snapshot recall " << command.name << " ramp " << format_command_bbt_offset (command.ramp);
		return text.str ();
	case ReactiveCommand::MacroMorph:
		text << "macro morph " << command.name << " -> " << command.text << " amount " << format_preview_value (command.value) << " ramp " << format_command_bbt_offset (command.ramp);
		return text.str ();
	case ReactiveCommand::State:
		text << "state " << command.name << " = " << command.text;
		return text.str ();
	case ReactiveCommand::Harmony:
		text << "harmony " << command.name << " = " << command.text;
		return text.str ();
	case ReactiveCommand::Rhythm:
		text << "rhythm " << command.name << " = " << format_preview_value (command.value);
		return text.str ();
	case ReactiveCommand::RhythmRoute:
		text << "rhythm route " << command.first << " " << command.name << " = " << format_preview_value (command.value);
		return text.str ();
	case ReactiveCommand::RhythmInsert:
		text << "insert rhythm on route " << command.first;
		return text.str ();
	}

	return "unknown command";
}

static std::vector<std::string>
command_summaries_from_commands (std::vector<ReactiveCommand> const& commands)
{
	static size_t const max_command_summaries = 4;

	std::vector<std::string> summaries;
	if (commands.empty ()) {
		return summaries;
	}

	size_t const visible = commands.size () > max_command_summaries ? max_command_summaries - 1 : commands.size ();
	summaries.reserve (std::min (commands.size (), max_command_summaries));
	for (size_t i = 0; i < visible; ++i) {
		summaries.push_back (format_command_summary (commands[i]));
	}

	if (visible < commands.size ()) {
		std::ostringstream more;
		more << "... " << (commands.size () - visible) << " more command";
		if ((commands.size () - visible) != 1) {
			more << "s";
		}
		summaries.push_back (more.str ());
	}

	return summaries;
}

} // namespace

size_t
ReactiveActionScheduler::queue_action (
	size_t slot,
	std::string const& primary_trigger,
	ReactiveActionPlan const& plan,
	Temporal::BBT_Time const& requested_at,
	Temporal::BBT_Time const& due_at)
{
	ReactiveQueuedAction queued;
	queued.id = _next_id++;
	queued.slot = slot;
	queued.action_name = plan.action_name;
	queued.primary_trigger = primary_trigger;
	queued.quantize = plan.quantize;
	queued.requested_at = requested_at;
	queued.due_at = zero_quantize (plan.quantize) ? requested_at : due_at;
	queued.commands = plan.commands;
	_queued.push_back (queued);
	return queued.id;
}

std::vector<ReactiveQueuedActionSummary>
ReactiveActionScheduler::queued_action_summary (size_t max_items, Temporal::BBT_Time const& now) const
{
	std::vector<ReactiveQueuedAction> ordered = _queued;
	std::sort (ordered.begin (), ordered.end (), queued_action_due_less);

	size_t const count = std::min (max_items, ordered.size ());
	std::vector<ReactiveQueuedActionSummary> summary;
	summary.reserve (count);

	for (size_t i = 0; i < count; ++i) {
		ReactiveQueuedAction const& queued = ordered[i];
		ReactiveQueuedActionSummary row;
		row.id = queued.id;
		row.slot = queued.slot;
		row.action_name = queued.action_name;
		row.primary_trigger = queued.primary_trigger;
		row.quantize = format_bbt_offset (queued.quantize);
		row.requested_at = queued.requested_at.str ();
		row.due_at = queued.due_at.str ();
		row.command_count = queued.commands.size ();
		row.command_summaries = command_summaries_from_commands (queued.commands);
		row.due = due_at_or_before (queued.due_at, now);
		summary.push_back (row);
	}

	return summary;
}

std::vector<ReactiveQueuedAction>
ReactiveActionScheduler::pop_due (Temporal::BBT_Time const& now)
{
	std::vector<ReactiveQueuedAction> due;
	std::vector<ReactiveQueuedAction> pending;

	due.reserve (_queued.size ());
	pending.reserve (_queued.size ());

	for (std::vector<ReactiveQueuedAction>::const_iterator queued = _queued.begin (); queued != _queued.end (); ++queued) {
		if (due_at_or_before (queued->due_at, now)) {
			due.push_back (*queued);
		} else {
			pending.push_back (*queued);
		}
	}

	std::sort (due.begin (), due.end (), queued_action_due_less);
	_queued = pending;
	return due;
}

void
ReactiveActionScheduler::clear ()
{
	_queued.clear ();
}

bool
ReactiveActionScheduler::has_pending_for_slot (size_t slot) const
{
	for (std::vector<ReactiveQueuedAction>::const_iterator queued = _queued.begin (); queued != _queued.end (); ++queued) {
		if (queued->slot == slot) {
			return true;
		}
	}

	return false;
}

bool
ReactiveActionScheduler::due_at_or_before (Temporal::BBT_Time const& due_at, Temporal::BBT_Time const& now)
{
	return due_at < now || due_at == now;
}

bool
ReactiveActionScheduler::zero_quantize (Temporal::BBT_Offset const& quantize)
{
	return quantize.bars == 0 && quantize.beats == 0 && quantize.ticks == 0;
}

std::string
ReactiveActionScheduler::format_bbt_offset (Temporal::BBT_Offset const& offset)
{
	std::ostringstream text;
	text << offset.bars << "|" << offset.beats << "|" << offset.ticks;
	return text.str ();
}
