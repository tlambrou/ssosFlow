#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRhythmChanceSourceTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmChanceSourceTest);
	CPPUNIT_TEST (sameSeedProducesSameSequence);
	CPPUNIT_TEST (differentCallsAdvanceState);
	CPPUNIT_TEST (resetReproducesSequence);
	CPPUNIT_TEST (valuesStayInUnitRange);
	CPPUNIT_TEST_SUITE_END ();

public:
	void sameSeedProducesSameSequence ();
	void differentCallsAdvanceState ();
	void resetReproducesSequence ();
	void valuesStayInUnitRange ();
};
