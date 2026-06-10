#include "reactive_rhythm_midi_buffer_adapter_test.h"

#include "ardour/midi_buffer.h"
#include "ardour/reactive_rhythm_chance_source.h"
#include "ardour/reactive_rhythm_midi_buffer_adapter.h"
#include "ardour/reactive_rhythm_step_source.h"

#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmMidiBufferAdapterTest);

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

static void
push (MidiBuffer& buffer, samplepos_t frame, unsigned char status, unsigned char data1, unsigned char data2)
{
	unsigned char data[3] = { status, data1, data2 };
	CPPUNIT_ASSERT_EQUAL (true, buffer.push_back (frame, Evoral::MIDI_EVENT, 3, data));
}

static std::vector<std::vector<unsigned char> >
buffer_bytes (MidiBuffer const& buffer)
{
	std::vector<std::vector<unsigned char> > events;

	for (MidiBuffer::const_iterator i = buffer.begin (); i != buffer.end (); ++i) {
		Evoral::Event<samplepos_t> event (*i, false);
		events.push_back (std::vector<unsigned char> (event.buffer (), event.buffer () + event.size ()));
	}

	return events;
}

static std::vector<samplepos_t>
buffer_times (MidiBuffer const& buffer)
{
	std::vector<samplepos_t> times;

	for (MidiBuffer::const_iterator i = buffer.begin (); i != buffer.end (); ++i) {
		Evoral::Event<samplepos_t> event (*i, false);
		times.push_back (event.time ());
	}

	return times;
}

} // namespace

void
ReactiveRhythmMidiBufferAdapterTest::keepsPassedNoteOnAndMatchingNoteOff ()
{
	MidiBuffer buffer (512);
	push (buffer, 10, 0x91, 60, 100);
	push (buffer, 90, 0x81, 60, 0);

	ReactiveRhythmMidiBufferAdapter adapter (settings_with_density (1.0));
	std::vector<ReactiveRhythmMidiBufferDecision> decisions = adapter.process_buffer (buffer, { 0.25, 0.25 });
	std::vector<std::vector<unsigned char> > events = buffer_bytes (buffer);
	std::vector<samplepos_t> times = buffer_times (buffer);

	CPPUNIT_ASSERT_EQUAL (size_t (2), decisions.size ());
	CPPUNIT_ASSERT_EQUAL (true, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (true, decisions[0].byte_decision.mapped);
	CPPUNIT_ASSERT_EQUAL (Evoral::MIDI_EVENT, decisions[0].event_type);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (10), decisions[0].time);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (size_t (2), events.size ());
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0x91), events[0][0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (60), events[0][1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (100), events[0][2]);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (10), times[0]);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (90), times[1]);
}

void
ReactiveRhythmMidiBufferAdapterTest::dropsSuppressedNoteOnAndMatchingNoteOff ()
{
	MidiBuffer buffer (512);
	push (buffer, 10, 0x90, 60, 100);
	push (buffer, 90, 0x80, 60, 0);

	ReactiveRhythmMidiBufferAdapter adapter (settings_with_density (0.0));
	std::vector<ReactiveRhythmMidiBufferDecision> decisions = adapter.process_buffer (buffer, { 0.25, 0.25 });

	CPPUNIT_ASSERT_EQUAL (size_t (2), decisions.size ());
	CPPUNIT_ASSERT_EQUAL (false, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (true, buffer.empty ());
}

void
ReactiveRhythmMidiBufferAdapterTest::mapsVelocityZeroNoteOnAsNoteOff ()
{
	MidiBuffer buffer (512);
	push (buffer, 10, 0x90, 60, 100);
	push (buffer, 90, 0x90, 60, 0);

	ReactiveRhythmMidiBufferAdapter adapter (settings_with_density (1.0));
	std::vector<ReactiveRhythmMidiBufferDecision> decisions = adapter.process_buffer (buffer, { 0.25, 0.25 });
	std::vector<std::vector<unsigned char> > events = buffer_bytes (buffer);

	CPPUNIT_ASSERT_EQUAL (true, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (ReactiveRhythmMidiEventType::NoteOff, decisions[1].byte_decision.mapped_event.type);
	CPPUNIT_ASSERT_EQUAL (size_t (2), events.size ());
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0x90), events[1][0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0), events[1][2]);
}

void
ReactiveRhythmMidiBufferAdapterTest::chanceZeroDropsNoteOnDeterministically ()
{
	MidiBuffer buffer (512);
	push (buffer, 10, 0x90, 60, 100);
	push (buffer, 90, 0x80, 60, 0);

	ReactiveRhythmSettings settings = settings_with_density (1.0);
	settings.chance = 0.0;
	ReactiveRhythmMidiBufferAdapter adapter (settings);
	std::vector<ReactiveRhythmMidiBufferDecision> decisions = adapter.process_buffer (buffer, { 0.0, 0.0 });

	CPPUNIT_ASSERT_EQUAL (false, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (true, buffer.empty ());
}

void
ReactiveRhythmMidiBufferAdapterTest::chanceSourceDrivesPassAndDropDecisions ()
{
	MidiBuffer pass_buffer (512);
	push (pass_buffer, 10, 0x90, 60, 100);
	push (pass_buffer, 90, 0x80, 60, 0);

	ReactiveRhythmSettings settings = settings_with_density (1.0);
	settings.chance = 0.5;
	ReactiveRhythmMidiBufferAdapter pass_adapter (settings);
	ReactiveRhythmChanceSource pass_source (1);
	std::vector<ReactiveRhythmMidiBufferDecision> pass_decisions = pass_adapter.process_buffer (pass_buffer, pass_source);

	CPPUNIT_ASSERT_EQUAL (true, pass_decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (true, pass_decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (false, pass_buffer.empty ());

	MidiBuffer drop_buffer (512);
	push (drop_buffer, 10, 0x90, 60, 100);
	push (drop_buffer, 90, 0x80, 60, 0);

	ReactiveRhythmMidiBufferAdapter drop_adapter (settings);
	ReactiveRhythmChanceSource drop_source (12345);
	std::vector<ReactiveRhythmMidiBufferDecision> drop_decisions = drop_adapter.process_buffer (drop_buffer, drop_source);

	CPPUNIT_ASSERT_EQUAL (false, drop_decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, drop_decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (true, drop_buffer.empty ());
}

void
ReactiveRhythmMidiBufferAdapterTest::frameDerivedStepsDriveDownbeatPriority ()
{
	MidiBuffer buffer (512);
	push (buffer, 120, 0x90, 60, 100);
	push (buffer, 0, 0x90, 62, 100);
	push (buffer, 180, 0x80, 60, 0);
	push (buffer, 60, 0x80, 62, 0);

	ReactiveRhythmSettings settings = settings_with_density (0.5);
	settings.pattern_steps = 4;
	settings.priority_mode = ReactiveRhythmPriorityMode::Downbeat;

	ReactiveRhythmMidiBufferAdapter adapter (settings);
	ReactiveRhythmChanceSource chance_source (1);
	ReactiveRhythmStepSource step_source (0, 120);
	std::vector<ReactiveRhythmMidiBufferDecision> decisions = adapter.process_buffer (buffer, chance_source, step_source);
	std::vector<std::vector<unsigned char> > events = buffer_bytes (buffer);

	CPPUNIT_ASSERT_EQUAL (size_t (4), decisions.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), decisions[0].byte_decision.mapped_event.step);
	CPPUNIT_ASSERT_EQUAL (size_t (0), decisions[1].byte_decision.mapped_event.step);
	CPPUNIT_ASSERT_EQUAL (false, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[2].forward);
	CPPUNIT_ASSERT_EQUAL (true, decisions[3].forward);
	CPPUNIT_ASSERT_EQUAL (size_t (2), events.size ());
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (62), events[0][1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (62), events[1][1]);
}

void
ReactiveRhythmMidiBufferAdapterTest::forwardsNonNoteEventsUnchanged ()
{
	MidiBuffer buffer (512);
	push (buffer, 5, 0xb0, 1, 64);
	push (buffer, 12, 0xe0, 0, 64);

	ReactiveRhythmMidiBufferAdapter adapter (settings_with_density (0.0));
	std::vector<ReactiveRhythmMidiBufferDecision> decisions = adapter.process_buffer (buffer, { 0.25, 0.25 });
	std::vector<std::vector<unsigned char> > events = buffer_bytes (buffer);
	std::vector<samplepos_t> times = buffer_times (buffer);

	CPPUNIT_ASSERT_EQUAL (size_t (2), decisions.size ());
	CPPUNIT_ASSERT_EQUAL (false, decisions[0].byte_decision.mapped);
	CPPUNIT_ASSERT_EQUAL (true, decisions[0].forward);
	CPPUNIT_ASSERT_EQUAL (false, decisions[1].byte_decision.mapped);
	CPPUNIT_ASSERT_EQUAL (true, decisions[1].forward);
	CPPUNIT_ASSERT_EQUAL (size_t (2), events.size ());
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0xb0), events[0][0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0xe0), events[1][0]);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (5), times[0]);
	CPPUNIT_ASSERT_EQUAL (samplepos_t (12), times[1]);
}
