#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionSlotRunnerTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionSlotRunnerTest);
	CPPUNIT_TEST (executeSlotByDocumentOrder);
	CPPUNIT_TEST (executeMidiNoteTriggerByDocumentOrder);
	CPPUNIT_TEST (executeMidiCCTriggerMacro);
	CPPUNIT_TEST (executeMidiCCTriggerMacroValueFromController);
	CPPUNIT_TEST (executeMidiCCTriggerRhythmRouteValueFromController);
	CPPUNIT_TEST (executeMidiBytesThroughRunner);
	CPPUNIT_TEST (reportMidiEventWithoutLoadedDocumentOrMatch);
	CPPUNIT_TEST (reportUnsupportedMidiBytes);
	CPPUNIT_TEST (executeBuiltInMvpFallbackRhythmDemo);
	CPPUNIT_TEST (reportMissingDocumentAndOutOfRangeSlot);
	CPPUNIT_TEST (rejectInvalidSource);
	CPPUNIT_TEST (propagateTargetFailure);
	CPPUNIT_TEST (resetExecutionStatusOnClearAndLoad);
	CPPUNIT_TEST (disabledPerformanceModeBlocksExecutionAndReportsStatus);
	CPPUNIT_TEST (summarizeActionBankForPerformancePanel);
	CPPUNIT_TEST (summarizePerformanceControlsForStatusPanel);
	CPPUNIT_TEST (performanceControlsMarkLatestAttemptForPanelRefresh);
	CPPUNIT_TEST (controllerFeedbackSummarizesLatestAttempt);
	CPPUNIT_TEST (controllerFeedbackSummarizesQueuedActions);
	CPPUNIT_TEST (controllerFeedbackMidiMessagesFollowSlotFeedbackState);
	CPPUNIT_TEST (controllerFeedbackMidiMessagesShowQueuedActions);
	CPPUNIT_TEST (controllerFeedbackMidiMessagesIgnoreInvalidBindings);
	CPPUNIT_TEST (summarizeMacroBankForPerformancePanel);
	CPPUNIT_TEST (summarizeStateBankForPerformancePanel);
	CPPUNIT_TEST (previewSlotForPerformancePanelWithoutMutatingState);
	CPPUNIT_TEST (previewMidiEventForPerformancePanel);
	CPPUNIT_TEST (formatCompactPanelSummaryForCuePage);
	CPPUNIT_TEST (transportProviderControlsSlotConditions);
	CPPUNIT_TEST (transportProviderControlsMidiConditions);
	CPPUNIT_TEST (transportProviderControlsPreviewConditions);
	CPPUNIT_TEST (queueQuantizedSlotActionUntilDue);
	CPPUNIT_TEST (queueQuantizedSlotActionUsingTempoMapClock);
	CPPUNIT_TEST (zeroQuantizeSlotActionExecutesImmediatelyThroughQueuePath);
	CPPUNIT_TEST (queueQuantizedMidiActionPreservesControllerValue);
	CPPUNIT_TEST (queueQuantizedMidiBytesUsingTempoMapClock);
	CPPUNIT_TEST (clearAndLoadDocumentClearQueuedActions);
	CPPUNIT_TEST (disabledPerformanceModeDoesNotQueueActions);
	CPPUNIT_TEST_SUITE_END ();

public:
	void executeSlotByDocumentOrder ();
	void executeMidiNoteTriggerByDocumentOrder ();
	void executeMidiCCTriggerMacro ();
	void executeMidiCCTriggerMacroValueFromController ();
	void executeMidiCCTriggerRhythmRouteValueFromController ();
	void executeMidiBytesThroughRunner ();
	void reportMidiEventWithoutLoadedDocumentOrMatch ();
	void reportUnsupportedMidiBytes ();
	void executeBuiltInMvpFallbackRhythmDemo ();
	void reportMissingDocumentAndOutOfRangeSlot ();
	void rejectInvalidSource ();
	void propagateTargetFailure ();
	void resetExecutionStatusOnClearAndLoad ();
	void disabledPerformanceModeBlocksExecutionAndReportsStatus ();
	void summarizeActionBankForPerformancePanel ();
	void summarizePerformanceControlsForStatusPanel ();
	void performanceControlsMarkLatestAttemptForPanelRefresh ();
	void controllerFeedbackSummarizesLatestAttempt ();
	void controllerFeedbackSummarizesQueuedActions ();
	void controllerFeedbackMidiMessagesFollowSlotFeedbackState ();
	void controllerFeedbackMidiMessagesShowQueuedActions ();
	void controllerFeedbackMidiMessagesIgnoreInvalidBindings ();
	void summarizeMacroBankForPerformancePanel ();
	void summarizeStateBankForPerformancePanel ();
	void previewSlotForPerformancePanelWithoutMutatingState ();
	void previewMidiEventForPerformancePanel ();
	void formatCompactPanelSummaryForCuePage ();
	void transportProviderControlsSlotConditions ();
	void transportProviderControlsMidiConditions ();
	void transportProviderControlsPreviewConditions ();
	void queueQuantizedSlotActionUntilDue ();
	void queueQuantizedSlotActionUsingTempoMapClock ();
	void zeroQuantizeSlotActionExecutesImmediatelyThroughQueuePath ();
	void queueQuantizedMidiActionPreservesControllerValue ();
	void queueQuantizedMidiBytesUsingTempoMapClock ();
	void clearAndLoadDocumentClearQueuedActions ();
	void disabledPerformanceModeDoesNotQueueActions ();
};
