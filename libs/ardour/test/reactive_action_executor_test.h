#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionExecutorTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionExecutorTest);
	CPPUNIT_TEST (executeAllCommandTypesInOrder);
	CPPUNIT_TEST (executeRhythmInsertCommand);
	CPPUNIT_TEST (executeRouteScopedRhythmCommand);
	CPPUNIT_TEST (executeTriggerProbabilityCommand);
	CPPUNIT_TEST (refuseFailedPlan);
	CPPUNIT_TEST (rejectUnexpandedMacroSnapshotCommand);
	CPPUNIT_TEST (rejectUnexpandedMacroMorphCommand);
	CPPUNIT_TEST (stopAfterFirstTargetFailure);
	CPPUNIT_TEST_SUITE_END ();

public:
	void executeAllCommandTypesInOrder ();
	void executeRhythmInsertCommand ();
	void executeRouteScopedRhythmCommand ();
	void executeTriggerProbabilityCommand ();
	void refuseFailedPlan ();
	void rejectUnexpandedMacroSnapshotCommand ();
	void rejectUnexpandedMacroMorphCommand ();
	void stopAfterFirstTargetFailure ();
};
