#include "reactive_rhythm_insertion_plan_test.h"

#include "ardour/reactive_rhythm_insertion_plan.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmInsertionPlanTest);

using namespace ARDOUR;

namespace {

static ReactiveRhythmInsertionCapabilities
complete_luaproc_capabilities ()
{
	ReactiveRhythmInsertionCapabilities capabilities;
	capabilities.lua_proc_available = true;
	capabilities.lua_proc_midi_io = true;
	capabilities.lua_proc_time_info = true;
	return capabilities;
}

} // namespace

void
ReactiveRhythmInsertionPlanTest::choosesLuaProcWhenMidiIoAndTimeInfoAreAvailable ()
{
	ReactiveRhythmInsertionPlan const plan = ReactiveRhythmInsertionPlanner::choose (complete_luaproc_capabilities ());

	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmInsertionTarget::LuaProc, plan.target);
	CPPUNIT_ASSERT (plan.can_process_midi_stream);
	CPPUNIT_ASSERT (!plan.rejects (ReactiveRhythmInsertionTarget::LuaProc));
}

void
ReactiveRhythmInsertionPlanTest::keepsLiveRouteMutationDisabledForMvp ()
{
	ReactiveRhythmInsertionPlan const plan = ReactiveRhythmInsertionPlanner::choose (complete_luaproc_capabilities ());

	CPPUNIT_ASSERT (!plan.allow_live_route_mutation);
}

void
ReactiveRhythmInsertionPlanTest::rejectsIncompleteLuaProcCapabilities ()
{
	ReactiveRhythmInsertionCapabilities without_midi_io;
	without_midi_io.lua_proc_available = true;
	without_midi_io.lua_proc_time_info = true;

	ReactiveRhythmInsertionCapabilities without_time_info;
	without_time_info.lua_proc_available = true;
	without_time_info.lua_proc_midi_io = true;

	ReactiveRhythmInsertionPlan const missing_midi = ReactiveRhythmInsertionPlanner::choose (without_midi_io);
	ReactiveRhythmInsertionPlan const missing_time = ReactiveRhythmInsertionPlanner::choose (without_time_info);

	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmInsertionTarget::None, missing_midi.target);
	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmInsertionTarget::None, missing_time.target);
	CPPUNIT_ASSERT (!missing_midi.can_process_midi_stream);
	CPPUNIT_ASSERT (!missing_time.can_process_midi_stream);
	CPPUNIT_ASSERT (missing_midi.rejects (ReactiveRhythmInsertionTarget::LuaProc));
	CPPUNIT_ASSERT (missing_time.rejects (ReactiveRhythmInsertionTarget::LuaProc));
}

void
ReactiveRhythmInsertionPlanTest::defersNativeAndExternalProcessorOptions ()
{
	ReactiveRhythmInsertionCapabilities capabilities = complete_luaproc_capabilities ();
	capabilities.native_processor_available = true;
	capabilities.external_plugin_available = true;

	ReactiveRhythmInsertionPlan const plan = ReactiveRhythmInsertionPlanner::choose (capabilities);

	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmInsertionTarget::LuaProc, plan.target);
	CPPUNIT_ASSERT (plan.defers (ReactiveRhythmInsertionTarget::NativeProcessor));
	CPPUNIT_ASSERT (plan.defers (ReactiveRhythmInsertionTarget::ExternalPlugin));
}

void
ReactiveRhythmInsertionPlanTest::rejectsRouteHooksAndControlSurfacesAsStreamTargets ()
{
	ReactiveRhythmInsertionCapabilities capabilities;
	capabilities.midi_route_hook_available = true;
	capabilities.control_surface_available = true;

	ReactiveRhythmInsertionPlan const plan = ReactiveRhythmInsertionPlanner::choose (capabilities);

	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmInsertionTarget::None, plan.target);
	CPPUNIT_ASSERT (!plan.can_process_midi_stream);
	CPPUNIT_ASSERT (plan.rejects (ReactiveRhythmInsertionTarget::MidiRouteHook));
	CPPUNIT_ASSERT (plan.rejects (ReactiveRhythmInsertionTarget::ControlSurface));
}
