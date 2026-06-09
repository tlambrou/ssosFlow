#include "reactive_action_clock_test.h"

#include "ardour/reactive_action_clock.h"

#include "temporal/bbt_argument.h"
#include "temporal/tempo.h"

using namespace ARDOUR;

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionClockTest);

namespace {

static Temporal::TempoMap
simple_tempo_map ()
{
	return Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4));
}

} // namespace

void
ReactiveActionClockTest::zeroQuantizeIsDueAtRequestTime ()
{
	Temporal::TempoMap map = simple_tempo_map ();

	ReactiveActionClockPosition const position = ReactiveActionClock::quantize_bbt (
		map,
		Temporal::BBT_Time (2, 3, 240),
		Temporal::BBT_Offset (0, 0, 0));

	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Time (2, 3, 240), position.requested_at);
	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Time (2, 3, 240), position.due_at);
}

void
ReactiveActionClockTest::barQuantizeRoundsToNextBarBoundary ()
{
	Temporal::TempoMap map = simple_tempo_map ();

	ReactiveActionClockPosition const position = ReactiveActionClock::quantize_bbt (
		map,
		Temporal::BBT_Time (3, 2, 0),
		Temporal::BBT_Offset (1, 0, 0));

	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Time (3, 2, 0), position.requested_at);
	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Time (4, 1, 0), position.due_at);
}

void
ReactiveActionClockTest::beatQuantizeRoundsToNextBeatBoundary ()
{
	Temporal::TempoMap map = simple_tempo_map ();

	ReactiveActionClockPosition const position = ReactiveActionClock::quantize_bbt (
		map,
		Temporal::BBT_Time (1, 1, 120),
		Temporal::BBT_Offset (0, 1, 0));

	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Time (1, 1, 120), position.requested_at);
	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Time (1, 2, 0), position.due_at);
}

void
ReactiveActionClockTest::samplePositionUsesTempoMapForRequestedBbt ()
{
	Temporal::TempoMap map = simple_tempo_map ();
	Temporal::BBT_Argument const request (Temporal::BBT_Time (3, 2, 0));
	samplepos_t const sample = map.sample_at (request);

	ReactiveActionClockPosition const position = ReactiveActionClock::quantize_sample (
		map,
		sample,
		Temporal::BBT_Offset (1, 0, 0));

	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Time (3, 2, 0), position.requested_at);
	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Time (4, 1, 0), position.due_at);
}
