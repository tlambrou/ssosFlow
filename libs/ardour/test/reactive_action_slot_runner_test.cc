#include "reactive_action_slot_runner_test.h"

#include "ardour/reactive_action_slot_runner.h"

#include <sstream>
#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionSlotRunnerTest);

using namespace ARDOUR;

namespace {

class RecordingTarget : public ReactiveActionTarget {
public:
	explicit RecordingTarget (std::string const& failing_call = std::string ())
		: _failing_call (failing_call)
	{
	}

	bool cue (int row, std::string& error)
	{
		return record (compose_one ("cue", row), error);
	}

	bool trigger (int route, int row, std::string& error)
	{
		std::ostringstream call;
		call << "trigger:" << route << ":" << row;
		return record (call.str (), error);
	}

	bool trigger_stop (int route, std::string& error)
	{
		return record (compose_one ("trigger-stop", route), error);
	}

	bool stop_all (std::string& error)
	{
		return record ("stop-all", error);
	}

	bool transport_play (std::string& error)
	{
		return record ("transport-play", error);
	}

	bool transport_stop (Temporal::BBT_Offset const& after, std::string& error)
	{
		std::ostringstream call;
		call << "transport-stop:" << after.bars << "|" << after.beats << "|" << after.ticks;
		return record (call.str (), error);
	}

	bool scene_apply (int index, std::string& error)
	{
		return record (compose_one ("scene-apply", index), error);
	}

	bool scene_store (int index, std::string& error)
	{
		return record (compose_one ("scene-store", index), error);
	}

	bool macro (std::string const& name, double value, Temporal::BBT_Offset const& ramp, std::string& error)
	{
		std::ostringstream call;
		call << "macro:" << name << ":" << value << ":" << ramp.bars << "|" << ramp.beats << "|" << ramp.ticks;
		return record (call.str (), error);
	}

	bool state (std::string const& name, std::string const& value, std::string& error)
	{
		return record ("state:" + name + ":" + value, error);
	}

	bool rhythm (std::string const& name, double value, std::string& error)
	{
		std::ostringstream call;
		call << "rhythm:" << name << ":" << value;
		return record (call.str (), error);
	}

	bool rhythm_insert (int route, std::string& error)
	{
		return record (compose_one ("rhythm-insert", route), error);
	}

	std::vector<std::string> calls;

private:
	static std::string compose_one (std::string const& prefix, int value)
	{
		std::ostringstream call;
		call << prefix << ":" << value;
		return call.str ();
	}

	bool record (std::string const& call, std::string& error)
	{
		if (call == _failing_call) {
			error = "target failed " + call;
			return false;
		}

		calls.push_back (call);
		error.clear ();
		return true;
	}

	std::string _failing_call;
};

static void
load_two_action_document (ReactiveActionSlotRunner& runner)
{
	std::string error;
	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION intro\n"
		"DO cue 2\n"
		"END\n"
		"ACTION breakdown\n"
		"DO trigger 1 3\n"
		"DO state section breakdown\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());
}

} // namespace

void
ReactiveActionSlotRunnerTest::executeSlotByDocumentOrder ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	load_two_action_document (runner);
	ReactiveExecutionResult result = runner.execute_slot (1, target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (2), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (2), runner.action_count ());
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), runner.action_name (1));
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), runner.last_action ());
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), runner.state_value ("section"));
	CPPUNIT_ASSERT_EQUAL (size_t (2), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("trigger:1:3"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("state:section:breakdown"), target.calls[1]);
}

void
ReactiveActionSlotRunnerTest::reportMissingDocumentAndOutOfRangeSlot ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	ReactiveExecutionResult missing = runner.execute_slot (0, target);

	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("no reactive action document loaded") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());

	load_two_action_document (runner);
	ReactiveExecutionResult out_of_range = runner.execute_slot (2, target);

	CPPUNIT_ASSERT_EQUAL (false, out_of_range.ok);
	CPPUNIT_ASSERT (out_of_range.error.find ("reactive action slot 2 is out of range") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionSlotRunnerTest::rejectInvalidSource ()
{
	ReactiveActionSlotRunner runner;
	std::string error;

	load_two_action_document (runner);
	CPPUNIT_ASSERT_EQUAL (true, runner.loaded ());
	CPPUNIT_ASSERT_EQUAL (size_t (2), runner.action_count ());

	CPPUNIT_ASSERT_EQUAL (false, runner.load_source (
		"ACTION broken\n"
		"DO warp now\n"
		"END\n",
		error));

	CPPUNIT_ASSERT (error.find ("unknown command") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (false, runner.loaded ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.action_count ());
}

void
ReactiveActionSlotRunnerTest::propagateTargetFailure ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target ("cue:2");

	load_two_action_document (runner);
	ReactiveExecutionResult result = runner.execute_slot (0, target);

	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("target failed cue:2") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (size_t (0), result.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
}
