#include "reactive_rhythm_test.h"

#include "ardour/reactive_rhythm.h"

#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmTest);

using namespace ARDOUR;

namespace {

static ReactiveRhythmEvent
event (size_t step, int pitch, int velocity, double chance_value)
{
	ReactiveRhythmEvent e;
	e.step = step;
	e.pitch = pitch;
	e.velocity = velocity;
	e.chance_value = chance_value;
	return e;
}

} // namespace

void
ReactiveRhythmTest::densityKeepsHighestPriorityEvents ()
{
	ReactiveRhythmSettings settings;
	settings.density = 0.5;
	settings.chance = 1.0;
	settings.priority_mode = ReactiveRhythmPriorityMode::Velocity;

	ReactiveRhythmState state (settings);
	std::vector<ReactiveRhythmDecision> decisions = state.evaluate ({
		event (0, 60, 20, 0.1),
		event (1, 62, 100, 0.1),
		event (2, 64, 70, 0.1),
		event (3, 65, 10, 0.1)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (4), decisions.size ());
	CPPUNIT_ASSERT_EQUAL (false, decisions[0].passes);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].passes);
	CPPUNIT_ASSERT_EQUAL (true, decisions[2].passes);
	CPPUNIT_ASSERT_EQUAL (false, decisions[3].passes);
}

void
ReactiveRhythmTest::chanceAppliesAfterDensity ()
{
	ReactiveRhythmSettings settings;
	settings.density = 0.5;
	settings.chance = 0.5;
	settings.priority_mode = ReactiveRhythmPriorityMode::Velocity;

	ReactiveRhythmState state (settings);
	std::vector<ReactiveRhythmDecision> decisions = state.evaluate ({
		event (0, 60, 100, 0.2),
		event (1, 62, 90, 0.8),
		event (2, 64, 10, 0.1),
		event (3, 65, 5, 0.1)
	});

	CPPUNIT_ASSERT_EQUAL (true, decisions[0].passes);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].passes);
	CPPUNIT_ASSERT_EQUAL (false, decisions[2].passes);
	CPPUNIT_ASSERT_EQUAL (false, decisions[3].passes);
}

void
ReactiveRhythmTest::rotationReportsShiftedPatternSteps ()
{
	ReactiveRhythmSettings settings;
	settings.pattern_steps = 8;
	settings.rotation = 3;

	ReactiveRhythmState state (settings);
	std::vector<ReactiveRhythmDecision> decisions = state.evaluate ({
		event (6, 60, 64, 0.1),
		event (1, 62, 64, 0.1)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (1), decisions[0].rotated_step);
	CPPUNIT_ASSERT_EQUAL (size_t (4), decisions[1].rotated_step);
}

void
ReactiveRhythmTest::downbeatPriorityUsesRotatedSteps ()
{
	ReactiveRhythmSettings settings;
	settings.pattern_steps = 8;
	settings.rotation = 1;
	settings.density = 0.5;
	settings.priority_mode = ReactiveRhythmPriorityMode::Downbeat;

	ReactiveRhythmState state (settings);
	std::vector<ReactiveRhythmDecision> decisions = state.evaluate ({
		event (7, 60, 64, 0.1),
		event (0, 62, 64, 0.1),
		event (3, 64, 64, 0.1),
		event (2, 65, 64, 0.1)
	});

	CPPUNIT_ASSERT_EQUAL (true, decisions[0].passes);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].passes);
	CPPUNIT_ASSERT_EQUAL (true, decisions[2].passes);
	CPPUNIT_ASSERT_EQUAL (false, decisions[3].passes);
}

void
ReactiveRhythmTest::pendingSettingsApplyOnlyAtQuantizedBoundary ()
{
	ReactiveRhythmSettings settings;
	settings.density = 1.0;
	settings.latch_steps = 4;

	ReactiveRhythmState state (settings);
	ReactiveRhythmSettings pending = settings;
	pending.density = 0.0;

	state.queue_settings (pending);
	CPPUNIT_ASSERT_EQUAL (true, state.has_pending_settings ());

	state.advance_to_step (3);
	CPPUNIT_ASSERT_EQUAL (true, state.has_pending_settings ());
	CPPUNIT_ASSERT_EQUAL (1.0, state.settings ().density);

	state.advance_to_step (4);
	CPPUNIT_ASSERT_EQUAL (false, state.has_pending_settings ());
	CPPUNIT_ASSERT_EQUAL (0.0, state.settings ().density);
}
