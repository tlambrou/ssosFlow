#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveActionSlotRunnerTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveActionSlotRunnerTest);
	CPPUNIT_TEST (executeSlotByDocumentOrder);
	CPPUNIT_TEST (reportMissingDocumentAndOutOfRangeSlot);
	CPPUNIT_TEST (rejectInvalidSource);
	CPPUNIT_TEST (propagateTargetFailure);
	CPPUNIT_TEST_SUITE_END ();

public:
	void executeSlotByDocumentOrder ();
	void reportMissingDocumentAndOutOfRangeSlot ();
	void rejectInvalidSource ();
	void propagateTargetFailure ();
};
