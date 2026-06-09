#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionSlotRunnerTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionSlotRunnerTest);
	CPPUNIT_TEST (executeSlotByDocumentOrder);
	CPPUNIT_TEST (executeMidiNoteTriggerByDocumentOrder);
	CPPUNIT_TEST (executeMidiCCTriggerMacro);
	CPPUNIT_TEST (executeMidiBytesThroughRunner);
	CPPUNIT_TEST (reportMidiEventWithoutLoadedDocumentOrMatch);
	CPPUNIT_TEST (reportUnsupportedMidiBytes);
	CPPUNIT_TEST (executeBuiltInMvpFallbackRhythmDemo);
	CPPUNIT_TEST (reportMissingDocumentAndOutOfRangeSlot);
	CPPUNIT_TEST (rejectInvalidSource);
	CPPUNIT_TEST (propagateTargetFailure);
	CPPUNIT_TEST (resetExecutionStatusOnClearAndLoad);
	CPPUNIT_TEST (summarizeActionBankForPerformancePanel);
	CPPUNIT_TEST_SUITE_END ();

public:
	void executeSlotByDocumentOrder ();
	void executeMidiNoteTriggerByDocumentOrder ();
	void executeMidiCCTriggerMacro ();
	void executeMidiBytesThroughRunner ();
	void reportMidiEventWithoutLoadedDocumentOrMatch ();
	void reportUnsupportedMidiBytes ();
	void executeBuiltInMvpFallbackRhythmDemo ();
	void reportMissingDocumentAndOutOfRangeSlot ();
	void rejectInvalidSource ();
	void propagateTargetFailure ();
	void resetExecutionStatusOnClearAndLoad ();
	void summarizeActionBankForPerformancePanel ();
};
