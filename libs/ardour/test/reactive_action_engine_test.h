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
	CPPUNIT_TEST (triggerActionWithMidiEventResolvesRhythmRouteValue);
	CPPUNIT_TEST (rotateSequentialChainCommands);
	CPPUNIT_TEST (rejectUnknownActionWithoutChangingLastAction);
	CPPUNIT_TEST (trackMacroAndStateValuesFromSelectedCommands);
	CPPUNIT_TEST (storeAndRecallMacroSnapshotValues);
	CPPUNIT_TEST (rejectMissingMacroSnapshotRecallWithoutChangingLastAction);
	CPPUNIT_TEST (blockUnmetStateConditionWithoutMutatingState);
	CPPUNIT_TEST (matchMacroConditionAfterMacroChanges);
	CPPUNIT_TEST (blockTransportConditionWithoutAdvancingSequentialChain);
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
	void triggerActionWithMidiEventResolvesRhythmRouteValue ();
	void rotateSequentialChainCommands ();
	void rejectUnknownActionWithoutChangingLastAction ();
	void trackMacroAndStateValuesFromSelectedCommands ();
	void storeAndRecallMacroSnapshotValues ();
	void rejectMissingMacroSnapshotRecallWithoutChangingLastAction ();
	void blockUnmetStateConditionWithoutMutatingState ();
	void matchMacroConditionAfterMacroChanges ();
	void blockTransportConditionWithoutAdvancingSequentialChain ();
};
