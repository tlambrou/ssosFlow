#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionSchedulerTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionSchedulerTest);
	CPPUNIT_TEST (queueActionReportsSummary);
	CPPUNIT_TEST (popDueActionsInDeterministicOrder);
	CPPUNIT_TEST (zeroQuantizeActionsAreImmediatelyDue);
	CPPUNIT_TEST (clearRemovesQueuedActions);
	CPPUNIT_TEST_SUITE_END ();

public:
	void queueActionReportsSummary ();
	void popDueActionsInDeterministicOrder ();
	void zeroQuantizeActionsAreImmediatelyDue ();
	void clearRemovesQueuedActions ();
};
