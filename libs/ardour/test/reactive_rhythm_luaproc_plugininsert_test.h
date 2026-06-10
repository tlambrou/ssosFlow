#pragma once

#include <cppunit/extensions/HelperMacros.h>

#include "test_needing_session.h"

class ReactiveRhythmLuaProcPluginInsertTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmLuaProcPluginInsertTest);
	CPPUNIT_TEST (pluginInsertRuntimePathProcessesMidiEvents);
	CPPUNIT_TEST (pluginInsertRuntimePathAppliesLatchedParameters);
	CPPUNIT_TEST_SUITE_END ();

public:
	void pluginInsertRuntimePathProcessesMidiEvents ();
	void pluginInsertRuntimePathAppliesLatchedParameters ();
};
