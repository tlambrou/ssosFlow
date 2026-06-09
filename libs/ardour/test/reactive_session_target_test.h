#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveSessionTargetTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveSessionTargetTest);
	CPPUNIT_TEST (mapCueTriggerStopsAndTransport);
	CPPUNIT_TEST (mapMixerScenes);
	CPPUNIT_TEST (propagateSessionFailures);
	CPPUNIT_TEST (rejectDelayedTransportStopUntilSchedulerExists);
	CPPUNIT_TEST (acceptNonSessionStateCommands);
	CPPUNIT_TEST_SUITE_END ();

public:
	void mapCueTriggerStopsAndTransport ();
	void mapMixerScenes ();
	void propagateSessionFailures ();
	void rejectDelayedTransportStopUntilSchedulerExists ();
	void acceptNonSessionStateCommands ();
};
