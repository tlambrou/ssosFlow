#include "reactive_action_scheduler_test.h"

#include "ardour/reactive_action_scheduler.h"

using namespace ARDOUR;

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionSchedulerTest);

namespace {

static ReactiveCommand
cue_command (int row)
{
	ReactiveCommand command;
	command.type = ReactiveCommand::Cue;
	command.first = row;
	return command;
}

static ReactiveActionPlan
plan (std::string const& name, Temporal::BBT_Offset const& quantize, int cue_row)
{
	ReactiveActionPlan p;
	p.ok = true;
	p.action_name = name;
	p.quantize = quantize;
	p.commands.push_back (cue_command (cue_row));
	return p;
}

} // namespace

void
ReactiveActionSchedulerTest::queueActionReportsSummary ()
{
	ReactiveActionScheduler scheduler;
	ReactiveActionPlan const p = plan ("intro.drop", Temporal::BBT_Offset (1, 0, 0), 0);

	size_t const id = scheduler.queue_action (
		2,
		"MIDI note ch=10 note=36",
		p,
		Temporal::BBT_Time (3, 2, 0),
		Temporal::BBT_Time (4, 1, 0));

	CPPUNIT_ASSERT_EQUAL (size_t (1), id);
	CPPUNIT_ASSERT_EQUAL (size_t (1), scheduler.queued_count ());

	std::vector<ReactiveQueuedActionSummary> const summary = scheduler.queued_action_summary (8, Temporal::BBT_Time (3, 3, 0));

	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (id, summary[0].id);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), summary[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI note ch=10 note=36"), summary[0].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|0|0"), summary[0].quantize);
	CPPUNIT_ASSERT_EQUAL (std::string ("3|2|0"), summary[0].requested_at);
	CPPUNIT_ASSERT_EQUAL (std::string ("4|1|0"), summary[0].due_at);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[0].command_count);
	CPPUNIT_ASSERT_EQUAL (false, summary[0].due);
}

void
ReactiveActionSchedulerTest::popDueActionsInDeterministicOrder ()
{
	ReactiveActionScheduler scheduler;

	size_t const later = scheduler.queue_action (
		0,
		"manual",
		plan ("later", Temporal::BBT_Offset (1, 0, 0), 1),
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (3, 1, 0));
	size_t const earlier = scheduler.queue_action (
		1,
		"manual",
		plan ("earlier", Temporal::BBT_Offset (0, 1, 0), 2),
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (2, 1, 0));
	size_t const same_due = scheduler.queue_action (
		2,
		"manual",
		plan ("same.due", Temporal::BBT_Offset (0, 1, 0), 3),
		Temporal::BBT_Time (1, 2, 0),
		Temporal::BBT_Time (2, 1, 0));

	std::vector<ReactiveQueuedAction> due = scheduler.pop_due (Temporal::BBT_Time (2, 1, 0));

	CPPUNIT_ASSERT_EQUAL (size_t (2), due.size ());
	CPPUNIT_ASSERT_EQUAL (earlier, due[0].id);
	CPPUNIT_ASSERT_EQUAL (same_due, due[1].id);
	CPPUNIT_ASSERT_EQUAL (std::string ("earlier"), due[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("same.due"), due[1].action_name);
	CPPUNIT_ASSERT_EQUAL (size_t (1), scheduler.queued_count ());

	due = scheduler.pop_due (Temporal::BBT_Time (3, 1, 0));

	CPPUNIT_ASSERT_EQUAL (size_t (1), due.size ());
	CPPUNIT_ASSERT_EQUAL (later, due[0].id);
	CPPUNIT_ASSERT_EQUAL (size_t (0), scheduler.queued_count ());
}

void
ReactiveActionSchedulerTest::zeroQuantizeActionsAreImmediatelyDue ()
{
	ReactiveActionScheduler scheduler;

	size_t const id = scheduler.queue_action (
		4,
		"manual",
		plan ("instant", Temporal::BBT_Offset (), 4),
		Temporal::BBT_Time (5, 2, 120),
		Temporal::BBT_Time (9, 1, 0));

	std::vector<ReactiveQueuedActionSummary> const summary = scheduler.queued_action_summary (8, Temporal::BBT_Time (5, 2, 120));

	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (id, summary[0].id);
	CPPUNIT_ASSERT_EQUAL (std::string ("5|2|120"), summary[0].due_at);
	CPPUNIT_ASSERT_EQUAL (true, summary[0].due);

	std::vector<ReactiveQueuedAction> const due = scheduler.pop_due (Temporal::BBT_Time (5, 2, 120));

	CPPUNIT_ASSERT_EQUAL (size_t (1), due.size ());
	CPPUNIT_ASSERT_EQUAL (id, due[0].id);
	CPPUNIT_ASSERT_EQUAL (std::string ("instant"), due[0].action_name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), scheduler.queued_count ());
}

void
ReactiveActionSchedulerTest::clearRemovesQueuedActions ()
{
	ReactiveActionScheduler scheduler;

	scheduler.queue_action (
		0,
		"manual",
		plan ("queued", Temporal::BBT_Offset (1, 0, 0), 0),
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (2, 1, 0));

	CPPUNIT_ASSERT_EQUAL (size_t (1), scheduler.queued_count ());

	scheduler.clear ();

	CPPUNIT_ASSERT_EQUAL (size_t (0), scheduler.queued_count ());
	CPPUNIT_ASSERT (scheduler.queued_action_summary (8, Temporal::BBT_Time (2, 1, 0)).empty ());
	CPPUNIT_ASSERT (scheduler.pop_due (Temporal::BBT_Time (2, 1, 0)).empty ());
}
