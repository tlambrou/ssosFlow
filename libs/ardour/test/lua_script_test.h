#include <memory>

#include "test_needing_session.h"

class LuaScriptTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (LuaScriptTest);
	CPPUNIT_TEST (session_script_test);
	CPPUNIT_TEST (reactive_performance_session_init_script_test);
	CPPUNIT_TEST (reactive_performance_session_init_installs_demo_action_document_test);
	CPPUNIT_TEST (reactive_performance_session_init_keeps_existing_action_document_test);
	CPPUNIT_TEST (reactive_performance_session_init_tolerates_unavailable_file_io_test);
	CPPUNIT_TEST (reactive_performance_lua_api_ensures_session_marker_test);
	CPPUNIT_TEST (reactive_performance_lua_api_ensures_midi_region_test);
	CPPUNIT_TEST (reactive_performance_lua_api_ensures_midi_trigger_region_test);
	CPPUNIT_TEST (dsp_script_test);
	CPPUNIT_TEST (reactive_rhythm_luaproc_script_test);
	CPPUNIT_TEST_SUITE_END ();

public:
	void session_script_test ();
	void reactive_performance_session_init_script_test ();
	void reactive_performance_session_init_installs_demo_action_document_test ();
	void reactive_performance_session_init_keeps_existing_action_document_test ();
	void reactive_performance_session_init_tolerates_unavailable_file_io_test ();
	void reactive_performance_lua_api_ensures_session_marker_test ();
	void reactive_performance_lua_api_ensures_midi_region_test ();
	void reactive_performance_lua_api_ensures_midi_trigger_region_test ();
	void dsp_script_test ();
	void reactive_rhythm_luaproc_script_test ();
};
