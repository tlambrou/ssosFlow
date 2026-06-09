#include "reactive_rhythm_luaproc_harness_test.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glib.h>

#include "lua/lua.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmLuaProcHarnessTest);

namespace {

struct MidiEvent {
	int time = 1;
	std::vector<int> data;
};

class LuaRhythmHarness
{
public:
	LuaRhythmHarness ()
		: _controls { 100, 100, 0, 0, 16, 1, 4, 1 }
		, _lua (luaL_newstate ())
	{
		if (!_lua) {
			throw std::runtime_error ("failed to create Lua state");
		}

		luaL_openlibs (_lua);
		install_ardour_stub ();
		install_ctrlports_stub ();

		std::string const path = reactive_script_path ();
		if (luaL_dofile (_lua, path.c_str ()) != LUA_OK) {
			std::string const error = lua_tostring (_lua, -1);
			lua_pop (_lua, 1);
			throw std::runtime_error ("failed to load " + path + ": " + error);
		}
	}

	~LuaRhythmHarness ()
	{
		if (_lua) {
			lua_close (_lua);
		}
	}

	void set_control (size_t index, double value)
	{
		_controls.at (index - 1) = value;
	}

	std::vector<MidiEvent> run (double beat, std::vector<MidiEvent> const& input)
	{
		set_time (beat);
		set_midiin (input);
		lua_newtable (_lua);
		lua_setglobal (_lua, "midiout");

		lua_getglobal (_lua, "dsp_run");
		lua_pushnil (_lua);
		lua_pushnil (_lua);
		lua_pushinteger (_lua, 120);
		if (lua_pcall (_lua, 3, 0, 0) != LUA_OK) {
			std::string const error = lua_tostring (_lua, -1);
			lua_pop (_lua, 1);
			throw std::runtime_error ("dsp_run failed: " + error);
		}

		return read_midiout ();
	}

private:
	static int ardour_stub (lua_State*)
	{
		return 0;
	}

	static int ctrlports_array (lua_State* lua)
	{
		LuaRhythmHarness* harness = static_cast<LuaRhythmHarness*> (lua_touserdata (lua, lua_upvalueindex (1)));
		lua_newtable (lua);
		for (size_t i = 0; i < harness->_controls.size (); ++i) {
			lua_pushnumber (lua, harness->_controls[i]);
			lua_rawseti (lua, -2, i + 1);
		}
		return 1;
	}

	static bool file_exists (std::string const& path)
	{
		std::ifstream file (path.c_str ());
		return file.good ();
	}

	static std::string reactive_script_path ()
	{
		char const* data_path = std::getenv ("ARDOUR_DATA_PATH");
		if (data_path) {
			std::stringstream paths (data_path);
			std::string root;
			while (std::getline (paths, root, G_SEARCHPATH_SEPARATOR)) {
				std::string const candidate = root + "/scripts/reactive_rhythm_state_mvp.lua";
				if (file_exists (candidate)) {
					return candidate;
				}
			}
		}

		std::string const fallback = "share/scripts/reactive_rhythm_state_mvp.lua";
		if (file_exists (fallback)) {
			return fallback;
		}

		throw std::runtime_error ("reactive_rhythm_state_mvp.lua was not found in ARDOUR_DATA_PATH or share/scripts");
	}

	void install_ardour_stub ()
	{
		lua_pushcfunction (_lua, &LuaRhythmHarness::ardour_stub);
		lua_setglobal (_lua, "ardour");
	}

	void install_ctrlports_stub ()
	{
		lua_newtable (_lua);
		lua_pushlightuserdata (_lua, this);
		lua_pushcclosure (_lua, &LuaRhythmHarness::ctrlports_array, 1);
		lua_setfield (_lua, -2, "array");
		lua_setglobal (_lua, "CtrlPorts");
	}

	void set_time (double beat)
	{
		lua_newtable (_lua);
		lua_pushnumber (_lua, beat);
		lua_setfield (_lua, -2, "beat");
		lua_setglobal (_lua, "time");
	}

	void push_data (std::vector<int> const& data)
	{
		lua_newtable (_lua);
		for (size_t i = 0; i < data.size (); ++i) {
			lua_pushinteger (_lua, data[i]);
			lua_rawseti (_lua, -2, i + 1);
		}
	}

	void set_midiin (std::vector<MidiEvent> const& input)
	{
		lua_newtable (_lua);
		for (size_t i = 0; i < input.size (); ++i) {
			lua_newtable (_lua);
			lua_pushinteger (_lua, input[i].time);
			lua_setfield (_lua, -2, "time");
			push_data (input[i].data);
			lua_setfield (_lua, -2, "data");
			lua_rawseti (_lua, -2, i + 1);
		}
		lua_setglobal (_lua, "midiin");
	}

	std::vector<int> read_data ()
	{
		std::vector<int> data;
		lua_getfield (_lua, -1, "data");
		size_t const len = lua_rawlen (_lua, -1);
		for (size_t i = 1; i <= len; ++i) {
			lua_rawgeti (_lua, -1, i);
			data.push_back (static_cast<int> (lua_tointeger (_lua, -1)));
			lua_pop (_lua, 1);
		}
		lua_pop (_lua, 1);
		return data;
	}

	std::vector<MidiEvent> read_midiout ()
	{
		std::vector<MidiEvent> output;
		lua_getglobal (_lua, "midiout");
		size_t const len = lua_rawlen (_lua, -1);
		for (size_t i = 1; i <= len; ++i) {
			lua_rawgeti (_lua, -1, i);

			MidiEvent event;
			lua_getfield (_lua, -1, "time");
			event.time = static_cast<int> (lua_tointeger (_lua, -1));
			lua_pop (_lua, 1);
			event.data = read_data ();
			output.push_back (event);

			lua_pop (_lua, 1);
		}
		lua_pop (_lua, 1);
		return output;
	}

	std::vector<double> _controls;
	lua_State* _lua;
};

static MidiEvent
midi (int time, std::vector<int> data)
{
	MidiEvent event;
	event.time = time;
	event.data = data;
	return event;
}

} // namespace

void
ReactiveRhythmLuaProcHarnessTest::defaultPassesNoteOnAndMatchingNoteOff ()
{
	LuaRhythmHarness harness;
	std::vector<MidiEvent> output = harness.run (0.0, {
		midi (1, { 0x90, 60, 100 }),
		midi (24, { 0x80, 60, 64 })
	});

	CPPUNIT_ASSERT_EQUAL (size_t (2), output.size ());
	CPPUNIT_ASSERT_EQUAL (0x90, output[0].data[0]);
	CPPUNIT_ASSERT_EQUAL (60, output[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (0x80, output[1].data[0]);
	CPPUNIT_ASSERT_EQUAL (60, output[1].data[1]);
}

void
ReactiveRhythmLuaProcHarnessTest::densityZeroSuppressesNoteOnAndMatchingNoteOff ()
{
	LuaRhythmHarness harness;
	harness.set_control (1, 0);

	std::vector<MidiEvent> output = harness.run (0.0, {
		midi (1, { 0x90, 60, 100 }),
		midi (24, { 0x80, 60, 64 })
	});

	CPPUNIT_ASSERT_EQUAL (size_t (0), output.size ());
}

void
ReactiveRhythmLuaProcHarnessTest::passesNonNoteMidi ()
{
	LuaRhythmHarness harness;
	harness.set_control (1, 0);

	std::vector<MidiEvent> output = harness.run (0.0, {
		midi (1, { 0xb0, 64, 127 })
	});

	CPPUNIT_ASSERT_EQUAL (size_t (1), output.size ());
	CPPUNIT_ASSERT_EQUAL (0xb0, output[0].data[0]);
	CPPUNIT_ASSERT_EQUAL (64, output[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (127, output[0].data[2]);
}

void
ReactiveRhythmLuaProcHarnessTest::velocityZeroNoteOnClosesForwardedNote ()
{
	LuaRhythmHarness harness;

	std::vector<MidiEvent> first = harness.run (0.0, {
		midi (1, { 0x90, 60, 100 })
	});
	std::vector<MidiEvent> second = harness.run (0.25, {
		midi (1, { 0x90, 60, 0 })
	});

	CPPUNIT_ASSERT_EQUAL (size_t (1), first.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), second.size ());
	CPPUNIT_ASSERT_EQUAL (0x90, second[0].data[0]);
	CPPUNIT_ASSERT_EQUAL (60, second[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (0, second[0].data[2]);
}

void
ReactiveRhythmLuaProcHarnessTest::velocityPriorityKeepsHighestVelocityNote ()
{
	LuaRhythmHarness harness;
	harness.set_control (1, 50);
	harness.set_control (3, 3);

	std::vector<MidiEvent> output = harness.run (0.0, {
		midi (1, { 0x90, 60, 20 }),
		midi (1, { 0x90, 62, 100 })
	});

	CPPUNIT_ASSERT_EQUAL (size_t (1), output.size ());
	CPPUNIT_ASSERT_EQUAL (62, output[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (100, output[0].data[2]);
}

void
ReactiveRhythmLuaProcHarnessTest::latchDelaysParameterUpdatesUntilBoundary ()
{
	LuaRhythmHarness harness;
	harness.set_control (6, 4);

	std::vector<MidiEvent> first = harness.run (0.0, {
		midi (1, { 0x90, 60, 100 })
	});

	harness.set_control (1, 0);

	std::vector<MidiEvent> before_boundary = harness.run (0.25, {
		midi (1, { 0x90, 61, 100 })
	});
	std::vector<MidiEvent> at_boundary = harness.run (1.0, {
		midi (1, { 0x90, 62, 100 })
	});

	CPPUNIT_ASSERT_EQUAL (size_t (1), first.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), before_boundary.size ());
	CPPUNIT_ASSERT_EQUAL (61, before_boundary[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (size_t (0), at_boundary.size ());
}
