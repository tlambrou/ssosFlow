#include <memory>

#include "test_needing_session.h"

class LuaScriptTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (LuaScriptTest);
	CPPUNIT_TEST (session_script_test);
	CPPUNIT_TEST (reactive_performance_session_init_script_test);
	CPPUNIT_TEST (dsp_script_test);
	CPPUNIT_TEST (reactive_rhythm_luaproc_script_test);
	CPPUNIT_TEST_SUITE_END ();

public:
	void session_script_test ();
	void reactive_performance_session_init_script_test ();
	void dsp_script_test ();
	void reactive_rhythm_luaproc_script_test ();
};
