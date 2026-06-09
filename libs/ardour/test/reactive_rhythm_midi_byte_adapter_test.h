#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRhythmMidiByteAdapterTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmMidiByteAdapterTest);
	CPPUNIT_TEST (mapNoteOnAndNoteOffThroughAdapter);
	CPPUNIT_TEST (densityZeroDropsNoteOnAndMatchingNoteOff);
	CPPUNIT_TEST (velocityZeroNoteOnMapsToNoteOff);
	CPPUNIT_TEST (chanceZeroDropsNoteOnDeterministically);
	CPPUNIT_TEST (nonNoteMessagesPassThroughUnmapped);
	CPPUNIT_TEST_SUITE_END ();

public:
	void mapNoteOnAndNoteOffThroughAdapter ();
	void densityZeroDropsNoteOnAndMatchingNoteOff ();
	void velocityZeroNoteOnMapsToNoteOff ();
	void chanceZeroDropsNoteOnDeterministically ();
	void nonNoteMessagesPassThroughUnmapped ();
};
