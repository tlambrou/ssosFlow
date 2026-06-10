#include "reactive_rhythm_chance_source_test.h"

#include "ardour/reactive_rhythm_chance_source.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmChanceSourceTest);

using namespace ARDOUR;

void
ReactiveRhythmChanceSourceTest::sameSeedProducesSameSequence ()
{
	ReactiveRhythmChanceSource a (12345);
	ReactiveRhythmChanceSource b (12345);

	for (int i = 0; i < 8; ++i) {
		CPPUNIT_ASSERT_EQUAL (a.next (), b.next ());
	}
}

void
ReactiveRhythmChanceSourceTest::differentCallsAdvanceState ()
{
	ReactiveRhythmChanceSource source (12345);

	double const first = source.next ();
	double const second = source.next ();

	CPPUNIT_ASSERT (first != second);
}

void
ReactiveRhythmChanceSourceTest::resetReproducesSequence ()
{
	ReactiveRhythmChanceSource source (12345);

	double const first = source.next ();
	source.next ();
	source.reset (12345);

	CPPUNIT_ASSERT_EQUAL (first, source.next ());
}

void
ReactiveRhythmChanceSourceTest::valuesStayInUnitRange ()
{
	ReactiveRhythmChanceSource source (98765);

	for (int i = 0; i < 64; ++i) {
		double const value = source.next ();
		CPPUNIT_ASSERT (value >= 0.0);
		CPPUNIT_ASSERT (value < 1.0);
	}
}
