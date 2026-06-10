#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveMidiMapTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveMidiMapTest);
	CPPUNIT_TEST (mapContainsExpectedControllerBindings);
	CPPUNIT_TEST (mapContainsLiveReactiveMidiTriggerBindings);
	CPPUNIT_TEST (mapContainsReactiveFeedbackBindings);
	CPPUNIT_TEST_SUITE_END ();

public:
	void mapContainsExpectedControllerBindings ();
	void mapContainsLiveReactiveMidiTriggerBindings ();
	void mapContainsReactiveFeedbackBindings ();
};
