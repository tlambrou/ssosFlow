#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRhythmMidiAdapterTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmMidiAdapterTest);
	CPPUNIT_TEST (densityOneForwardsNoteOnAndMatchingNoteOff);
	CPPUNIT_TEST (densityZeroSuppressesNoteOnAndMatchingNoteOff);
	CPPUNIT_TEST (chanceZeroSuppressesNoteOnDeterministically);
	CPPUNIT_TEST (batchDensityKeepsHigherPriorityNoteOn);
	CPPUNIT_TEST (passedNoteOffStillForwardsAfterLaterDrops);
	CPPUNIT_TEST_SUITE_END ();

public:
	void densityOneForwardsNoteOnAndMatchingNoteOff ();
	void densityZeroSuppressesNoteOnAndMatchingNoteOff ();
	void chanceZeroSuppressesNoteOnDeterministically ();
	void batchDensityKeepsHigherPriorityNoteOn ();
	void passedNoteOffStillForwardsAfterLaterDrops ();
};
