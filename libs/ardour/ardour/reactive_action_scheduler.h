#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "temporal/bbt_time.h"

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action_engine.h"

namespace ARDOUR {

struct LIBARDOUR_API ReactiveQueuedAction {
	size_t id = 0;
	size_t slot = 0;
	std::string action_name;
	std::string primary_trigger;
	Temporal::BBT_Offset quantize;
	Temporal::BBT_Time requested_at;
	Temporal::BBT_Time due_at;
	std::vector<ReactiveCommand> commands;
};

struct LIBARDOUR_API ReactiveQueuedActionSummary {
	size_t id = 0;
	size_t slot = 0;
	std::string action_name;
	std::string primary_trigger;
	std::string quantize;
	std::string requested_at;
	std::string due_at;
	size_t command_count = 0;
	bool due = false;
};

class LIBARDOUR_API ReactiveActionScheduler
{
public:
	size_t queue_action (
		size_t slot,
		std::string const& primary_trigger,
		ReactiveActionPlan const& plan,
		Temporal::BBT_Time const& requested_at,
		Temporal::BBT_Time const& due_at);

	std::vector<ReactiveQueuedActionSummary> queued_action_summary (size_t max_items, Temporal::BBT_Time const& now) const;
	std::vector<ReactiveQueuedAction> pop_due (Temporal::BBT_Time const& now);
	void clear ();
	size_t queued_count () const { return _queued.size (); }
	bool has_pending_for_slot (size_t slot) const;

private:
	static bool due_at_or_before (Temporal::BBT_Time const& due_at, Temporal::BBT_Time const& now);
	static bool zero_quantize (Temporal::BBT_Offset const&);
	static std::string format_bbt_offset (Temporal::BBT_Offset const&);

	std::vector<ReactiveQueuedAction> _queued;
	size_t _next_id = 1;
};

} // namespace ARDOUR
