#include "reactive_session_target_test.h"

#include "ardour/reactive_session_target.h"

#include <sstream>
#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveSessionTargetTest);

using namespace ARDOUR;

namespace {

class RecordingSessionTarget : public ReactiveSessionTarget {
public:
	RecordingSessionTarget ()
		: ReactiveSessionTarget ()
	{
	}

	bool fail_trigger = false;
	bool fail_trigger_stop = false;
	bool fail_scene_apply = false;
	bool fail_rhythm_insert = false;
	std::vector<std::string> calls;

protected:
	void session_trigger_cue_row (int row)
	{
		calls.push_back (compose_one ("cue", row));
	}

	bool session_bang_trigger_at (int route, int row, float velocity)
	{
		std::ostringstream call;
		call << "trigger:" << route << ":" << row << ":" << velocity;
		calls.push_back (call.str ());
		return !fail_trigger;
	}

	bool session_stop_triggers_at (int route, std::string& error)
	{
		calls.push_back (compose_one ("trigger-stop", route));
		if (fail_trigger_stop) {
			error = "missing triggerbox";
			return false;
		}
		return true;
	}

	void session_trigger_stop_all (bool now)
	{
		calls.push_back (std::string ("stop-all:") + (now ? "now" : "quantized"));
	}

	void session_request_transport_speed (double speed)
	{
		std::ostringstream call;
		call << "speed:" << speed;
		calls.push_back (call.str ());
	}

	void session_request_roll ()
	{
		calls.push_back ("roll");
	}

	void session_request_stop ()
	{
		calls.push_back ("stop");
	}

	bool session_apply_nth_mixer_scene (int index)
	{
		calls.push_back (compose_one ("scene-apply", index));
		return !fail_scene_apply;
	}

	void session_store_nth_mixer_scene (int index)
	{
		calls.push_back (compose_one ("scene-store", index));
	}

	bool session_insert_reactive_rhythm (int route, std::string& error)
	{
		calls.push_back (compose_one ("rhythm-insert", route));
		if (fail_rhythm_insert) {
			error = "missing route";
			return false;
		}
		return true;
	}

private:
	static std::string compose_one (std::string const& prefix, int value)
	{
		std::ostringstream call;
		call << prefix << ":" << value;
		return call.str ();
	}
};

} // namespace

void
ReactiveSessionTargetTest::mapCueTriggerStopsAndTransport ()
{
	RecordingSessionTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, target.cue (2, error));
	CPPUNIT_ASSERT_EQUAL (true, target.trigger (3, 4, error));
	CPPUNIT_ASSERT_EQUAL (true, target.trigger_stop (3, error));
	CPPUNIT_ASSERT_EQUAL (true, target.stop_all (error));
	CPPUNIT_ASSERT_EQUAL (true, target.transport_play (error));
	CPPUNIT_ASSERT_EQUAL (true, target.transport_stop (Temporal::BBT_Offset (), error));

	CPPUNIT_ASSERT_EQUAL (size_t (7), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:2"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("trigger:3:4:1"), target.calls[1]);
	CPPUNIT_ASSERT_EQUAL (std::string ("trigger-stop:3"), target.calls[2]);
	CPPUNIT_ASSERT_EQUAL (std::string ("stop-all:quantized"), target.calls[3]);
	CPPUNIT_ASSERT_EQUAL (std::string ("speed:1"), target.calls[4]);
	CPPUNIT_ASSERT_EQUAL (std::string ("roll"), target.calls[5]);
	CPPUNIT_ASSERT_EQUAL (std::string ("stop"), target.calls[6]);
	CPPUNIT_ASSERT (error.empty ());
}

void
ReactiveSessionTargetTest::mapMixerScenes ()
{
	RecordingSessionTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, target.scene_apply (1, error));
	CPPUNIT_ASSERT_EQUAL (true, target.scene_store (2, error));

	CPPUNIT_ASSERT_EQUAL (size_t (2), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("scene-apply:1"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("scene-store:2"), target.calls[1]);
}

void
ReactiveSessionTargetTest::propagateSessionFailures ()
{
	RecordingSessionTarget target;
	std::string error;

	target.fail_trigger = true;
	CPPUNIT_ASSERT_EQUAL (false, target.trigger (3, 4, error));
	CPPUNIT_ASSERT (error.find ("trigger route 3 row 4 failed") != std::string::npos);

	error.clear ();
	target.fail_trigger_stop = true;
	CPPUNIT_ASSERT_EQUAL (false, target.trigger_stop (5, error));
	CPPUNIT_ASSERT (error.find ("missing triggerbox") != std::string::npos);

	error.clear ();
	target.fail_scene_apply = true;
	CPPUNIT_ASSERT_EQUAL (false, target.scene_apply (8, error));
	CPPUNIT_ASSERT (error.find ("scene apply 8 failed") != std::string::npos);
}

void
ReactiveSessionTargetTest::rejectDelayedTransportStopUntilSchedulerExists ()
{
	RecordingSessionTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (false, target.transport_stop (Temporal::BBT_Offset (1, 0, 0), error));
	CPPUNIT_ASSERT (error.find ("delayed transport stop") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveSessionTargetTest::acceptNonSessionStateCommands ()
{
	RecordingSessionTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, target.macro ("filter", 0.75, Temporal::BBT_Offset (0, 2, 0), error));
	CPPUNIT_ASSERT_EQUAL (true, target.state ("section", "breakdown", error));
	CPPUNIT_ASSERT_EQUAL (false, target.rhythm ("density", 0.50, error));
	CPPUNIT_ASSERT (error.find ("no session") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveSessionTargetTest::mapRhythmInsert ()
{
	RecordingSessionTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, target.rhythm_insert (4, error));
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm-insert:4"), target.calls[0]);
	CPPUNIT_ASSERT (error.empty ());

	target.fail_rhythm_insert = true;
	CPPUNIT_ASSERT_EQUAL (false, target.rhythm_insert (9, error));
	CPPUNIT_ASSERT (error.find ("missing route") != std::string::npos);
}
