#pragma once

#include <cppunit/extensions/HelperMacros.h>

#include "test_needing_session.h"

class ReactiveSessionTargetRhythmInsertTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (ReactiveSessionTargetRhythmInsertTest);
	CPPUNIT_TEST (rhythmInsertAddsLuaProcToRemoteRoute);
	CPPUNIT_TEST (rhythmParameterActionsUpdateInsertedLuaProcControls);
	CPPUNIT_TEST (routeScopedRhythmParameterActionsUpdateOnlyTargetRoute);
	CPPUNIT_TEST (routeScopedRhythmParameterActionsReportMissingTargets);
	CPPUNIT_TEST (rhythmParameterActionsReportMissingInsertAndUnknownName);
	CPPUNIT_TEST (rhythmInsertReportsMissingRoute);
	CPPUNIT_TEST (summarizeReactiveRhythmRoutingStatus);
	CPPUNIT_TEST (formatEmptyReactiveRhythmRoutingStatus);
	CPPUNIT_TEST (summarizeReactiveTriggerSlotStatus);
	CPPUNIT_TEST (formatEmptyReactiveTriggerSlotStatus);
	CPPUNIT_TEST (summarizeReactiveSessionClockState);
	CPPUNIT_TEST (sessionClockStateCountsTriggerVisibleRoutes);
	CPPUNIT_TEST (formatEmptyReactiveMixerSceneState);
	CPPUNIT_TEST (summarizeReactiveMixerSceneState);
	CPPUNIT_TEST (formatEmptyReactiveTrackState);
	CPPUNIT_TEST (summarizeReactiveTrackStateForRoutes);
	CPPUNIT_TEST_SUITE_END ();

public:
	void rhythmInsertAddsLuaProcToRemoteRoute ();
	void rhythmParameterActionsUpdateInsertedLuaProcControls ();
	void routeScopedRhythmParameterActionsUpdateOnlyTargetRoute ();
	void routeScopedRhythmParameterActionsReportMissingTargets ();
	void rhythmParameterActionsReportMissingInsertAndUnknownName ();
	void rhythmInsertReportsMissingRoute ();
	void summarizeReactiveRhythmRoutingStatus ();
	void formatEmptyReactiveRhythmRoutingStatus ();
	void summarizeReactiveTriggerSlotStatus ();
	void formatEmptyReactiveTriggerSlotStatus ();
	void summarizeReactiveSessionClockState ();
	void sessionClockStateCountsTriggerVisibleRoutes ();
	void formatEmptyReactiveMixerSceneState ();
	void summarizeReactiveMixerSceneState ();
	void formatEmptyReactiveTrackState ();
	void summarizeReactiveTrackStateForRoutes ();
};
