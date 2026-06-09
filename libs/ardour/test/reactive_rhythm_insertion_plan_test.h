#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ReactiveRhythmInsertionPlanTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ReactiveRhythmInsertionPlanTest);
	CPPUNIT_TEST (choosesLuaProcWhenMidiIoAndTimeInfoAreAvailable);
	CPPUNIT_TEST (keepsLiveRouteMutationDisabledForMvp);
	CPPUNIT_TEST (rejectsIncompleteLuaProcCapabilities);
	CPPUNIT_TEST (defersNativeAndExternalProcessorOptions);
	CPPUNIT_TEST (rejectsRouteHooksAndControlSurfacesAsStreamTargets);
	CPPUNIT_TEST_SUITE_END ();

public:
	void choosesLuaProcWhenMidiIoAndTimeInfoAreAvailable ();
	void keepsLiveRouteMutationDisabledForMvp ();
	void rejectsIncompleteLuaProcCapabilities ();
	void defersNativeAndExternalProcessorOptions ();
	void rejectsRouteHooksAndControlSurfacesAsStreamTargets ();
};
