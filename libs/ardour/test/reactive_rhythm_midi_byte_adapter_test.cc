#include "reactive_rhythm_midi_byte_adapter_test.h"

#include "ardour/reactive_rhythm_midi_byte_adapter.h"

#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmMidiByteAdapterTest);

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

static ReactiveRhythmMidiBytes
bytes (size_t step, size_t frame, unsigned char status, unsigned char data1, unsigned char data2, double chance_value = 0.25)
{
	return ReactiveRhythmMidiBytes::three_byte (step, frame, status, data1, data2, chance_value);
}

} // namespace

void
ReactiveRhythmMidiByteAdapterTest::mapNoteOnAndNoteOffThroughAdapter ()
{
	ReactiveRhythmMidiByteAdapter adapter (settings_with_density (1.0));

	std::vector<ReactiveRhythmMidiByteDecision> decisions = adapter.process_events ({
		bytes (3, 10, 0x91, 60, 100),
		bytes (3, 90, 0x81, 60, 0)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (2), decisions.size ());
	CPPUNIT_ASSERT_EQUAL (true, decisions[0].mapped);
	CPPUNIT_ASSERT_EQUAL (true, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmMidiEventType::NoteOn, decisions[0].mapped_event.type);
	CPPUNIT_ASSERT_EQUAL (size_t (3), decisions[0].mapped_event.step);
	CPPUNIT_ASSERT_EQUAL (size_t (10), decisions[0].mapped_event.frame);
	CPPUNIT_ASSERT_EQUAL (1, decisions[0].mapped_event.channel);
	CPPUNIT_ASSERT_EQUAL (60, decisions[0].mapped_event.note);
	CPPUNIT_ASSERT_EQUAL (100, decisions[0].mapped_event.velocity);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].mapped);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmMidiEventType::NoteOff, decisions[1].mapped_event.type);
}

void
ReactiveRhythmMidiByteAdapterTest::densityZeroDropsNoteOnAndMatchingNoteOff ()
{
	ReactiveRhythmMidiByteAdapter adapter (settings_with_density (0.0));

	std::vector<ReactiveRhythmMidiByteDecision> decisions = adapter.process_events ({
		bytes (0, 0, 0x90, 60, 100),
		bytes (0, 100, 0x80, 60, 0)
	});

	CPPUNIT_ASSERT_EQUAL (false, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].forward);
}

void
ReactiveRhythmMidiByteAdapterTest::velocityZeroNoteOnMapsToNoteOff ()
{
	ReactiveRhythmMidiByteAdapter adapter (settings_with_density (1.0));

	std::vector<ReactiveRhythmMidiByteDecision> decisions = adapter.process_events ({
		bytes (0, 0, 0x90, 60, 100),
		bytes (0, 100, 0x90, 60, 0)
	});

	CPPUNIT_ASSERT_EQUAL (true, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].mapped);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmMidiEventType::NoteOff, decisions[1].mapped_event.type);
}

void
ReactiveRhythmMidiByteAdapterTest::chanceZeroDropsNoteOnDeterministically ()
{
	ReactiveRhythmSettings settings = settings_with_density (1.0);
	settings.chance = 0.0;
	ReactiveRhythmMidiByteAdapter adapter (settings);

	std::vector<ReactiveRhythmMidiByteDecision> decisions = adapter.process_events ({
		bytes (0, 0, 0x90, 60, 100, 0.0),
		bytes (0, 100, 0x80, 60, 0)
	});

	CPPUNIT_ASSERT_EQUAL (false, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].forward);
}

void
ReactiveRhythmMidiByteAdapterTest::nonNoteMessagesPassThroughUnmapped ()
{
	ReactiveRhythmMidiByteAdapter adapter (settings_with_density (0.0));

	std::vector<ReactiveRhythmMidiByteDecision> decisions = adapter.process_events ({
		bytes (0, 0, 0xb0, 1, 64),
		bytes (0, 10, 0xe0, 0, 64)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (2), decisions.size ());
	CPPUNIT_ASSERT_EQUAL (false, decisions[0].mapped);
	CPPUNIT_ASSERT_EQUAL (true, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].mapped);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
}
