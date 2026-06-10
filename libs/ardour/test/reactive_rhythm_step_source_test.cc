#include "reactive_rhythm_step_source_test.h"

#include "ardour/reactive_rhythm_step_source.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmStepSourceTest);

using namespace ARDOUR;

void
ReactiveRhythmStepSourceTest::mapsFramesFromOriginToSteps ()
{
	ReactiveRhythmStepSource source (100, 120);

	CPPUNIT_ASSERT_EQUAL (size_t (0), source.step_for (100));
	CPPUNIT_ASSERT_EQUAL (size_t (0), source.step_for (219));
	CPPUNIT_ASSERT_EQUAL (size_t (1), source.step_for (220));
	CPPUNIT_ASSERT_EQUAL (size_t (1), source.step_for (339));
	CPPUNIT_ASSERT_EQUAL (size_t (2), source.step_for (340));
}

void
ReactiveRhythmStepSourceTest::clampsFramesBeforeOriginToStepZero ()
{
	ReactiveRhythmStepSource source (100, 120);

	CPPUNIT_ASSERT_EQUAL (size_t (0), source.step_for (0));
	CPPUNIT_ASSERT_EQUAL (size_t (0), source.step_for (99));
}

void
ReactiveRhythmStepSourceTest::normalizesInvalidFramesPerStep ()
{
	ReactiveRhythmStepSource zero_source (0, 0);
	ReactiveRhythmStepSource negative_source (0, -12);

	CPPUNIT_ASSERT_EQUAL (samplecnt_t (1), zero_source.frames_per_step ());
	CPPUNIT_ASSERT_EQUAL (size_t (7), zero_source.step_for (7));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (1), negative_source.frames_per_step ());
	CPPUNIT_ASSERT_EQUAL (size_t (7), negative_source.step_for (7));
}

void
ReactiveRhythmStepSourceTest::calculatesFramesPerStepFromTempo ()
{
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (6000), ReactiveRhythmStepSource::frames_per_step_for_tempo (48000.0, 120.0, 4));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (48000), ReactiveRhythmStepSource::frames_per_step_for_tempo (48000.0, 60.0, 1));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (5513), ReactiveRhythmStepSource::frames_per_step_for_tempo (44100.0, 120.0, 4));
}

void
ReactiveRhythmStepSourceTest::normalizesInvalidTempoInputs ()
{
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (1), ReactiveRhythmStepSource::frames_per_step_for_tempo (0.0, 120.0, 4));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (720000), ReactiveRhythmStepSource::frames_per_step_for_tempo (48000.0, 0.0, 4));
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (24000), ReactiveRhythmStepSource::frames_per_step_for_tempo (48000.0, 120.0, 0));
}

void
ReactiveRhythmStepSourceTest::configuresStepSourceFromTempo ()
{
	ReactiveRhythmStepSource source;
	source.configure_from_tempo (100, 48000.0, 120.0, 4);

	CPPUNIT_ASSERT_EQUAL (samplepos_t (100), source.origin_frame ());
	CPPUNIT_ASSERT_EQUAL (samplecnt_t (6000), source.frames_per_step ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), source.step_for (6099));
	CPPUNIT_ASSERT_EQUAL (size_t (1), source.step_for (6100));
}
