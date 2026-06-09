#include "reactive_rhythm_luaproc_plugininsert_test.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "ardour/automation_control.h"
#include "ardour/audioengine.h"
#include "ardour/buffer_set.h"
#include "ardour/midi_buffer.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/process_thread.h"
#include "ardour/session.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveRhythmLuaProcPluginInsertTest);

using namespace ARDOUR;

namespace {

struct MidiEvent {
	samplepos_t time = 0;
	std::vector<unsigned char> data;
};

class ScopedProcessThreadBuffers
{
public:
	ScopedProcessThreadBuffers ()
		: _owns_buffers (!ProcessThread::have_thread_buffers ())
	{
		if (_owns_buffers) {
			_process_thread.get_buffers ();
		}
	}

	~ScopedProcessThreadBuffers ()
	{
		if (_owns_buffers) {
			_process_thread.drop_buffers ();
		}
	}

private:
	ProcessThread _process_thread;
	bool _owns_buffers;
};

static PluginInfoPtr
reactive_rhythm_info ()
{
	PluginManager& pm = PluginManager::instance ();
	const PluginInfoList& plugins = pm.lua_plugin_info ();
	for (PluginInfoList::const_iterator i = plugins.begin (); i != plugins.end (); ++i) {
		if ((*i)->name == "Reactive Rhythm State MVP") {
			return *i;
		}
	}
	return PluginInfoPtr ();
}

static void
push (MidiBuffer& buffer, samplepos_t frame, std::vector<unsigned char> const& data)
{
	CPPUNIT_ASSERT (!data.empty ());
	CPPUNIT_ASSERT_EQUAL (true, buffer.push_back (frame, Evoral::MIDI_EVENT, data.size (), &data[0]));
}

static std::vector<MidiEvent>
buffer_events (MidiBuffer const& buffer)
{
	std::vector<MidiEvent> events;

	for (MidiBuffer::const_iterator i = buffer.begin (); i != buffer.end (); ++i) {
		Evoral::Event<samplepos_t> event (*i, false);
		MidiEvent midi;
		midi.time = event.time ();
		midi.data.assign (event.buffer (), event.buffer () + event.size ());
		events.push_back (midi);
	}

	return events;
}

class PluginInsertHarness
{
public:
	explicit PluginInsertHarness (Session& session)
		: _session (session)
	{
		PluginInfoPtr info = reactive_rhythm_info ();
		CPPUNIT_ASSERT_MESSAGE ("Reactive Rhythm State MVP LuaProc script was not discoverable", info);

		PluginPtr plugin = info->load (_session);
		CPPUNIT_ASSERT_MESSAGE (info->name, plugin);

		_insert.reset (new PluginInsert (_session, Temporal::TimeDomainProvider (Temporal::AudioTime), plugin));

		ChanCount midi_io (DataType::MIDI, 1);
		{
			PBD::Mutex::Lock lm (AudioEngine::instance ()->process_lock ());
			CPPUNIT_ASSERT_EQUAL (true, _insert->configure_io (midi_io, midi_io));
		}
		_session.ensure_buffers_unlocked (_insert->required_buffers ());
		_insert->enable (true);
	}

	void set_parameter (uint32_t index, float value)
	{
		CPPUNIT_ASSERT (index < _insert->plugin ()->parameter_count ());
		std::shared_ptr<AutomationControl> control = _insert->automation_control (Evoral::Parameter (PluginAutomation, 0, index));
		CPPUNIT_ASSERT (control);
		control->set_value (value, PBD::Controllable::NoGroup);
	}

	std::vector<MidiEvent> run (samplepos_t start, pframes_t nframes, std::vector<MidiEvent> const& input)
	{
		ChanCount configured_in;
		ChanCount configured_out;
		_insert->configured_io (configured_in, configured_out);

		ChanCount buffer_count = ChanCount::max (configured_in, configured_out);
		buffer_count = ChanCount::max (buffer_count, _insert->required_buffers ());

		BufferSet buffers;
		buffers.ensure_buffers (buffer_count, std::max<size_t> (1024, nframes));
		buffers.set_count (buffer_count);
		buffers.silence (nframes, 0);

		MidiBuffer& midi = buffers.get_midi (0);
		for (std::vector<MidiEvent>::const_iterator i = input.begin (); i != input.end (); ++i) {
			push (midi, i->time, i->data);
		}

		ScopedProcessThreadBuffers process_thread_buffers;
		_insert->run (buffers, start, start + nframes, 1.0, nframes, true);
		return buffer_events (buffers.get_midi (0));
	}

	samplepos_t sample_for_beat (double beat) const
	{
		return static_cast<samplepos_t> (std::llround (beat * (_session.sample_rate () * 0.5)));
	}

private:
	Session& _session;
	std::shared_ptr<PluginInsert> _insert;
};

static MidiEvent
midi (samplepos_t time, unsigned char status, unsigned char data1, unsigned char data2)
{
	MidiEvent event;
	event.time = time;
	event.data.push_back (status);
	event.data.push_back (data1);
	event.data.push_back (data2);
	return event;
}

} // namespace

void
ReactiveRhythmLuaProcPluginInsertTest::pluginInsertRuntimePathProcessesMidiEvents ()
{
	PluginInsertHarness pass_harness (*_session);
	std::vector<MidiEvent> passed = pass_harness.run (0, 256, {
		midi (10, 0x90, 60, 100),
		midi (20, 0xb0, 64, 127),
		midi (90, 0x80, 60, 64)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (3), passed.size ());
	CPPUNIT_ASSERT_EQUAL (samplepos_t (10), passed[0].time);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0x90), passed[0].data[0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (60), passed[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0xb0), passed[1].data[0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (64), passed[1].data[1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (127), passed[1].data[2]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0x80), passed[2].data[0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (60), passed[2].data[1]);

	PluginInsertHarness density_harness (*_session);
	density_harness.set_parameter (0, 0.0f);
	std::vector<MidiEvent> density_zero = density_harness.run (0, 256, {
		midi (10, 0x90, 60, 100),
		midi (20, 0xb0, 64, 127),
		midi (90, 0x80, 60, 64)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (1), density_zero.size ());
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0xb0), density_zero[0].data[0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (64), density_zero[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (127), density_zero[0].data[2]);

	PluginInsertHarness velocity_zero_harness (*_session);
	std::vector<MidiEvent> first = velocity_zero_harness.run (0, 256, {
		midi (10, 0x90, 60, 100)
	});
	std::vector<MidiEvent> release = velocity_zero_harness.run (256, 256, {
		midi (10, 0x90, 60, 0)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (1), first.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), release.size ());
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0x90), release[0].data[0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (60), release[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0), release[0].data[2]);
}

void
ReactiveRhythmLuaProcPluginInsertTest::pluginInsertRuntimePathAppliesLatchedParameters ()
{
	PluginInsertHarness harness (*_session);
	harness.set_parameter (5, 4.0f);

	std::vector<MidiEvent> first = harness.run (0, 256, {
		midi (10, 0x90, 60, 100)
	});

	harness.set_parameter (0, 0.0f);

	std::vector<MidiEvent> before_boundary = harness.run (harness.sample_for_beat (0.25), 256, {
		midi (10, 0x90, 61, 100)
	});
	std::vector<MidiEvent> at_boundary = harness.run (harness.sample_for_beat (1.0), 256, {
		midi (10, 0x90, 62, 100)
	});

	CPPUNIT_ASSERT_EQUAL (size_t (1), first.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), before_boundary.size ());
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (61), before_boundary[0].data[1]);
	CPPUNIT_ASSERT_EQUAL (size_t (0), at_boundary.size ());
}
