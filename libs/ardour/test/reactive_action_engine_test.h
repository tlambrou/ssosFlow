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
	CPPUNIT_TEST (matchMarkerTriggerByName);
	CPPUNIT_TEST (keepDocumentOrderForMultipleMarkerMatches);
	CPPUNIT_TEST (ignoreSceneTriggersForMidiAndMarker);
	CPPUNIT_TEST (matchSceneTriggerByIndex);
	CPPUNIT_TEST (keepDocumentOrderForMultipleSceneMatches);
	CPPUNIT_TEST (ignoreRegionTriggersForOtherFamilies);
	CPPUNIT_TEST (matchRegionTriggerByName);
	CPPUNIT_TEST (keepDocumentOrderForMultipleRegionMatches);
	CPPUNIT_TEST (keepDocumentOrderForMultipleMatches);
	CPPUNIT_TEST (triggerAllChainReturnsAllCommandsAndMetadata);
	CPPUNIT_TEST (triggerActionWithMidiEventResolvesMacroValue);
	CPPUNIT_TEST (triggerActionWithMidiEventResolvesRhythmRouteValue);
	CPPUNIT_TEST (rotateSequentialChainCommands);
	CPPUNIT_TEST (rejectUnknownActionWithoutChangingLastAction);
	CPPUNIT_TEST (trackMacroAndStateValuesFromSelectedCommands);
	CPPUNIT_TEST (trackHarmonyValuesAndConditions);
	CPPUNIT_TEST (storeAndRecallMacroSnapshotValues);
	CPPUNIT_TEST (rejectMissingMacroSnapshotRecallWithoutChangingLastAction);
	CPPUNIT_TEST (morphBetweenMacroSnapshots);
	CPPUNIT_TEST (morphBetweenMacroSnapshotsWithMidiValue);
	CPPUNIT_TEST (rejectMacroMorphWithoutSharedSnapshotValues);
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
	void matchMarkerTriggerByName ();
	void keepDocumentOrderForMultipleMarkerMatches ();
	void ignoreSceneTriggersForMidiAndMarker ();
	void matchSceneTriggerByIndex ();
	void keepDocumentOrderForMultipleSceneMatches ();
	void ignoreRegionTriggersForOtherFamilies ();
	void matchRegionTriggerByName ();
	void keepDocumentOrderForMultipleRegionMatches ();
	void keepDocumentOrderForMultipleMatches ();
	void triggerAllChainReturnsAllCommandsAndMetadata ();
	void triggerActionWithMidiEventResolvesMacroValue ();
	void triggerActionWithMidiEventResolvesRhythmRouteValue ();
	void rotateSequentialChainCommands ();
	void rejectUnknownActionWithoutChangingLastAction ();
	void trackMacroAndStateValuesFromSelectedCommands ();
	void trackHarmonyValuesAndConditions ();
	void storeAndRecallMacroSnapshotValues ();
	void rejectMissingMacroSnapshotRecallWithoutChangingLastAction ();
	void morphBetweenMacroSnapshots ();
	void morphBetweenMacroSnapshotsWithMidiValue ();
	void rejectMacroMorphWithoutSharedSnapshotValues ();
	void blockUnmetStateConditionWithoutMutatingState ();
	void matchMacroConditionAfterMacroChanges ();
	void blockTransportConditionWithoutAdvancingSequentialChain ();
};
