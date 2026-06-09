#include "reactive_action_executor_test.h"

#include "ardour/reactive_action.h"
#include "ardour/reactive_action_engine.h"
#include "ardour/reactive_action_executor.h"

#include <sstream>
#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionExecutorTest);

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

	bool rhythm_route (int route, std::string const& name, double value, std::string& error)
	{
		std::ostringstream call;
		call << "rhythm-route:" << route << ":" << name << ":" << value;
		return record (call.str (), error);
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
		return true;
	}

	std::string _failing_call;
};

static ReactiveActionEngine
engine_from_source (char const* src)
{
	ReactiveActionParseResult parsed = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, parsed.ok);

	ReactiveActionEngine engine;
	std::string error;
	CPPUNIT_ASSERT_EQUAL (true, engine.load_document (parsed.document, error));
	return engine;
}

} // namespace

void
ReactiveActionExecutorTest::executeAllCommandTypesInOrder ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION full\n"
		"DO cue 2\n"
		"DO trigger 3 4\n"
		"DO trigger-stop 3\n"
		"DO stop-all\n"
		"DO transport play\n"
		"DO transport stop after 4|0|0\n"
		"DO scene apply 1\n"
		"DO scene store 2\n"
		"DO macro filter 0.75 ramp 0|2|0\n"
		"DO state section breakdown\n"
		"DO rhythm density 0.50\n"
		"DO rhythm insert 5\n"
		"END\n");
	ReactiveActionPlan plan = engine.trigger_action ("full");
	RecordingTarget target;

	ReactiveExecutionResult result = ReactiveActionExecutor::execute (plan, target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (12), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (12), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:2"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("trigger:3:4"), target.calls[1]);
	CPPUNIT_ASSERT_EQUAL (std::string ("trigger-stop:3"), target.calls[2]);
	CPPUNIT_ASSERT_EQUAL (std::string ("stop-all"), target.calls[3]);
	CPPUNIT_ASSERT_EQUAL (std::string ("transport-play"), target.calls[4]);
	CPPUNIT_ASSERT_EQUAL (std::string ("transport-stop:4|0|0"), target.calls[5]);
	CPPUNIT_ASSERT_EQUAL (std::string ("scene-apply:1"), target.calls[6]);
	CPPUNIT_ASSERT_EQUAL (std::string ("scene-store:2"), target.calls[7]);
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.75:0|2|0"), target.calls[8]);
	CPPUNIT_ASSERT_EQUAL (std::string ("state:section:breakdown"), target.calls[9]);
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm:density:0.5"), target.calls[10]);
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm-insert:5"), target.calls[11]);
}

void
ReactiveActionExecutorTest::executeRhythmInsertCommand ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION add.rhythm\n"
		"DO rhythm insert 2\n"
		"END\n");
	ReactiveActionPlan plan = engine.trigger_action ("add.rhythm");
	RecordingTarget target;

	ReactiveExecutionResult result = ReactiveActionExecutor::execute (plan, target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm-insert:2"), target.calls[0]);
}

void
ReactiveActionExecutorTest::executeRouteScopedRhythmCommand ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION route.rhythm\n"
		"DO rhythm route 2 density 0.35\n"
		"END\n");
	ReactiveActionPlan plan = engine.trigger_action ("route.rhythm");
	RecordingTarget target;

	ReactiveExecutionResult result = ReactiveActionExecutor::execute (plan, target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm-route:2:density:0.35"), target.calls[0]);
}

void
ReactiveActionExecutorTest::refuseFailedPlan ()
{
	ReactiveActionPlan plan;
	plan.error = "unknown action 'missing'";
	RecordingTarget target;

	ReactiveExecutionResult result = ReactiveActionExecutor::execute (plan, target);

	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("unknown action") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (size_t (0), result.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionExecutorTest::stopAfterFirstTargetFailure ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION partial\n"
		"DO cue 2\n"
		"DO scene apply 1\n"
		"DO cue 3\n"
		"END\n");
	ReactiveActionPlan plan = engine.trigger_action ("partial");
	RecordingTarget target ("scene-apply:1");

	ReactiveExecutionResult result = ReactiveActionExecutor::execute (plan, target);

	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("target failed scene-apply:1") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:2"), target.calls[0]);
}
