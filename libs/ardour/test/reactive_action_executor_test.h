#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionExecutorTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionExecutorTest);
	CPPUNIT_TEST (executeAllCommandTypesInOrder);
	CPPUNIT_TEST (refuseFailedPlan);
	CPPUNIT_TEST (stopAfterFirstTargetFailure);
	CPPUNIT_TEST_SUITE_END ();

public:
	void executeAllCommandTypesInOrder ();
	void refuseFailedPlan ();
	void stopAfterFirstTargetFailure ();
};
