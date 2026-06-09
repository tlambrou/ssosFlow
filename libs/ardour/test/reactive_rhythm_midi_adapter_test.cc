#include "reactive_rhythm_midi_adapter_test.h"

#include "ardour/reactive_rhythm_midi_adapter.h"

#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmMidiAdapterTest);

using namespace ARDOUR;

namespace {

static ReactiveRhythmSettings
settings_with_density (double density)
{
	ReactiveRhythmSettings settings;
	settings.density = density;
	settings.chance = 1.0;
	settings.priority_mode = ReactiveRhythmPriorityMode::Velocity;
	return settings;
}

} // namespace

void
ReactiveRhythmMidiAdapterTest::densityOneForwardsNoteOnAndMatchingNoteOff ()
{
	ReactiveRhythmMidiAdapter adapter (settings_with_density (1.0));

	std::vector<ReactiveRhythmMidiDecision> decisions = adapter.process_events ({
		ReactiveRhythmMidiEvent::note_on (0, 0, 1, 60, 100, 0.25),
		ReactiveRhythmMidiEvent::note_off (120, 1, 60)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (2), decisions.size ());
	CPPUNIT_ASSERT_EQUAL (true, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmMidiEventType::NoteOn, decisions[0].event.type);
	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmMidiEventType::NoteOff, decisions[1].event.type);
}

void
ReactiveRhythmMidiAdapterTest::densityZeroSuppressesNoteOnAndMatchingNoteOff ()
{
	ReactiveRhythmMidiAdapter adapter (settings_with_density (0.0));

	std::vector<ReactiveRhythmMidiDecision> decisions = adapter.process_events ({
		ReactiveRhythmMidiEvent::note_on (0, 0, 1, 60, 100, 0.25),
		ReactiveRhythmMidiEvent::note_off (120, 1, 60)
	});

	CPPUNIT_ASSERT_EQUAL (false, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].forward);
}

void
ReactiveRhythmMidiAdapterTest::chanceZeroSuppressesNoteOnDeterministically ()
{
	ReactiveRhythmSettings settings = settings_with_density (1.0);
	settings.chance = 0.0;
	ReactiveRhythmMidiAdapter adapter (settings);

	std::vector<ReactiveRhythmMidiDecision> decisions = adapter.process_events ({
		ReactiveRhythmMidiEvent::note_on (0, 0, 1, 60, 100, 0.0),
		ReactiveRhythmMidiEvent::note_off (120, 1, 60)
	});

	CPPUNIT_ASSERT_EQUAL (false, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].forward);
}

void
ReactiveRhythmMidiAdapterTest::batchDensityKeepsHigherPriorityNoteOn ()
{
	ReactiveRhythmMidiAdapter adapter (settings_with_density (0.5));

	std::vector<ReactiveRhythmMidiDecision> decisions = adapter.process_events ({
		ReactiveRhythmMidiEvent::note_on (0, 0, 1, 60, 20, 0.25),
		ReactiveRhythmMidiEvent::note_on (0, 0, 1, 62, 100, 0.25),
		ReactiveRhythmMidiEvent::note_off (120, 1, 60),
		ReactiveRhythmMidiEvent::note_off (120, 1, 62)
	});

	CPPUNIT_ASSERT_EQUAL (false, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[2].forward);
	CPPUNIT_ASSERT_EQUAL (true, decisions[3].forward);
}

void
ReactiveRhythmMidiAdapterTest::passedNoteOffStillForwardsAfterLaterDrops ()
{
	ReactiveRhythmMidiAdapter adapter (settings_with_density (1.0));

	std::vector<ReactiveRhythmMidiDecision> first = adapter.process_events ({
		ReactiveRhythmMidiEvent::note_on (0, 0, 1, 60, 100, 0.25)
	});
	CPPUNIT_ASSERT_EQUAL (true, first[0].forward);

	adapter.set_settings (settings_with_density (0.0));
	std::vector<ReactiveRhythmMidiDecision> second = adapter.process_events ({
		ReactiveRhythmMidiEvent::note_on (1, 1, 1, 62, 100, 0.25),
		ReactiveRhythmMidiEvent::note_off (120, 1, 60),
		ReactiveRhythmMidiEvent::note_off (120, 1, 62)
	});

	CPPUNIT_ASSERT_EQUAL (false, second[0].forward);
	CPPUNIT_ASSERT_EQUAL (true, second[1].forward);
	CPPUNIT_ASSERT_EQUAL (false, second[2].forward);
}
