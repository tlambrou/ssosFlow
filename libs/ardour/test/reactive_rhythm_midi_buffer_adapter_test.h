#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRhythmMidiBufferAdapterTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmMidiBufferAdapterTest);
	CPPUNIT_TEST (keepsPassedNoteOnAndMatchingNoteOff);
	CPPUNIT_TEST (dropsSuppressedNoteOnAndMatchingNoteOff);
	CPPUNIT_TEST (mapsVelocityZeroNoteOnAsNoteOff);
	CPPUNIT_TEST (chanceZeroDropsNoteOnDeterministically);
	CPPUNIT_TEST (chanceSourceDrivesPassAndDropDecisions);
	CPPUNIT_TEST (forwardsNonNoteEventsUnchanged);
	CPPUNIT_TEST_SUITE_END ();

public:
	void keepsPassedNoteOnAndMatchingNoteOff ();
	void dropsSuppressedNoteOnAndMatchingNoteOff ();
	void mapsVelocityZeroNoteOnAsNoteOff ();
	void chanceZeroDropsNoteOnDeterministically ();
	void chanceSourceDrivesPassAndDropDecisions ();
	void forwardsNonNoteEventsUnchanged ();
};
