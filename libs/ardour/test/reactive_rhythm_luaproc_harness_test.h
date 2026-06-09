#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRhythmLuaProcHarnessTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmLuaProcHarnessTest);
	CPPUNIT_TEST (defaultPassesNoteOnAndMatchingNoteOff);
	CPPUNIT_TEST (densityZeroSuppressesNoteOnAndMatchingNoteOff);
	CPPUNIT_TEST (passesNonNoteMidi);
	CPPUNIT_TEST (velocityZeroNoteOnClosesForwardedNote);
	CPPUNIT_TEST (velocityPriorityKeepsHighestVelocityNote);
	CPPUNIT_TEST (latchDelaysParameterUpdatesUntilBoundary);
	CPPUNIT_TEST_SUITE_END ();

public:
	void defaultPassesNoteOnAndMatchingNoteOff ();
	void densityZeroSuppressesNoteOnAndMatchingNoteOff ();
	void passesNonNoteMidi ();
	void velocityZeroNoteOnClosesForwardedNote ();
	void velocityPriorityKeepsHighestVelocityNote ();
	void latchDelaysParameterUpdatesUntilBoundary ();
};
