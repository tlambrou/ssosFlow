#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveControllerFeedbackMidiCacheTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveControllerFeedbackMidiCacheTest);
	CPPUNIT_TEST (storeSnapshotAndClearMessages);
	CPPUNIT_TEST (tryWriteCachedMessagesSkipsEmptyMessages);
	CPPUNIT_TEST_SUITE_END ();

public:
	void storeSnapshotAndClearMessages ();
	void tryWriteCachedMessagesSkipsEmptyMessages ();
};
