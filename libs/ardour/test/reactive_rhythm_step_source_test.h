#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRhythmStepSourceTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmStepSourceTest);
	CPPUNIT_TEST (mapsFramesFromOriginToSteps);
	CPPUNIT_TEST (clampsFramesBeforeOriginToStepZero);
	CPPUNIT_TEST (normalizesInvalidFramesPerStep);
	CPPUNIT_TEST (calculatesFramesPerStepFromTempo);
	CPPUNIT_TEST (normalizesInvalidTempoInputs);
	CPPUNIT_TEST (configuresStepSourceFromTempo);
	CPPUNIT_TEST_SUITE_END ();

public:
	void mapsFramesFromOriginToSteps ();
	void clampsFramesBeforeOriginToStepZero ();
	void normalizesInvalidFramesPerStep ();
	void calculatesFramesPerStepFromTempo ();
	void normalizesInvalidTempoInputs ();
	void configuresStepSourceFromTempo ();
};
