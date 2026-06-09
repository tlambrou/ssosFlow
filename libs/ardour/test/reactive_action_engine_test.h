#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionEngineTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionEngineTest);
	CPPUNIT_TEST (matchMidiNoteTrigger);
	CPPUNIT_TEST (mapMidiNoteBytesToTriggerEvent);
	CPPUNIT_TEST (mapMidiCCBytesToTriggerEvent);
	CPPUNIT_TEST (ignoreUnsupportedMidiBytes);
	CPPUNIT_TEST (ignoreMismatchedMidiNote);
	CPPUNIT_TEST (ignoreZeroVelocityNoteOn);
	CPPUNIT_TEST (matchMidiCCThreshold);
	CPPUNIT_TEST (matchMidiCCWithoutThreshold);
	CPPUNIT_TEST (ignoreMarkerTriggersForMidi);
	CPPUNIT_TEST (keepDocumentOrderForMultipleMatches);
	CPPUNIT_TEST (triggerAllChainReturnsAllCommandsAndMetadata);
	CPPUNIT_TEST (triggerActionWithMidiEventResolvesMacroValue);
	CPPUNIT_TEST (rotateSequentialChainCommands);
	CPPUNIT_TEST (rejectUnknownActionWithoutChangingLastAction);
	CPPUNIT_TEST (trackMacroAndStateValuesFromSelectedCommands);
	CPPUNIT_TEST_SUITE_END ();

public:
	void matchMidiNoteTrigger ();
	void mapMidiNoteBytesToTriggerEvent ();
	void mapMidiCCBytesToTriggerEvent ();
	void ignoreUnsupportedMidiBytes ();
	void ignoreMismatchedMidiNote ();
	void ignoreZeroVelocityNoteOn ();
	void matchMidiCCThreshold ();
	void matchMidiCCWithoutThreshold ();
	void ignoreMarkerTriggersForMidi ();
	void keepDocumentOrderForMultipleMatches ();
	void triggerAllChainReturnsAllCommandsAndMetadata ();
	void triggerActionWithMidiEventResolvesMacroValue ();
	void rotateSequentialChainCommands ();
	void rejectUnknownActionWithoutChangingLastAction ();
	void trackMacroAndStateValuesFromSelectedCommands ();
};
