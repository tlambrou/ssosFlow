#include "reactive_action_slot_runner_test.h"

#include "ardour/reactive_action_document_loader.h"
#include "ardour/reactive_action_slot_runner.h"

#include "temporal/tempo.h"

#include <sstream>
#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionSlotRunnerTest);

using namespace ARDOUR;

namespace {

class RecordingTarget : public ReactiveActionTarget {
public:
	explicit RecordingTarget (std::string const& failing_call = std::string ())
		: _failing_call (failing_call)
	{
	}

	bool cue (int row, std::string& error)
	{
		return record (compose_one ("cue", row), error);
	}

	bool trigger (int route, int row, std::string& error)
	{
		std::ostringstream call;
		call << "trigger:" << route << ":" << row;
		return record (call.str (), error);
	}

	bool trigger_probability (int route, int row, double value, std::string& error)
	{
		std::ostringstream call;
		call << "trigger-probability:" << route << ":" << row << ":" << value;
		return record (call.str (), error);
	}

	bool trigger_stop (int route, std::string& error)
	{
		return record (compose_one ("trigger-stop", route), error);
	}

	bool stop_all (std::string& error)
	{
		return record ("stop-all", error);
	}

	bool transport_play (std::string& error)
	{
		return record ("transport-play", error);
	}

	bool transport_stop (Temporal::BBT_Offset const& after, std::string& error)
	{
		std::ostringstream call;
		call << "transport-stop:" << after.bars << "|" << after.beats << "|" << after.ticks;
		return record (call.str (), error);
	}

	bool scene_apply (int index, std::string& error)
	{
		return record (compose_one ("scene-apply", index), error);
	}

	bool scene_store (int index, std::string& error)
	{
		return record (compose_one ("scene-store", index), error);
	}

	bool macro (std::string const& name, double value, Temporal::BBT_Offset const& ramp, std::string& error)
	{
		std::ostringstream call;
		call << "macro:" << name << ":" << value << ":" << ramp.bars << "|" << ramp.beats << "|" << ramp.ticks;
		return record (call.str (), error);
	}

	bool state (std::string const& name, std::string const& value, std::string& error)
	{
		return record ("state:" + name + ":" + value, error);
	}

	bool harmony (std::string const& name, std::string const& value, std::string& error)
	{
		return record ("harmony:" + name + ":" + value, error);
	}

	bool rhythm (std::string const& name, double value, std::string& error)
	{
		std::ostringstream call;
		call << "rhythm:" << name << ":" << value;
		return record (call.str (), error);
	}

	bool rhythm_insert (int route, std::string& error)
	{
		return record (compose_one ("rhythm-insert", route), error);
	}

	bool rhythm_route (int route, std::string const& name, double value, std::string& error)
	{
		std::ostringstream call;
		call << "rhythm-route:" << route << ":" << name << ":" << value;
		return record (call.str (), error);
	}

	std::vector<std::string> calls;

private:
	static std::string compose_one (std::string const& prefix, int value)
	{
		std::ostringstream call;
		call << prefix << ":" << value;
		return call.str ();
	}

	bool record (std::string const& call, std::string& error)
	{
		if (call == _failing_call) {
			error = "target failed " + call;
			return false;
		}

		calls.push_back (call);
		error.clear ();
		return true;
	}

	std::string _failing_call;
};

static void
load_two_action_document (ReactiveActionSlotRunner& runner)
{
	std::string error;
	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION intro\n"
		"DO cue 2\n"
		"END\n"
		"ACTION breakdown\n"
		"DO trigger 1 3\n"
		"DO state section breakdown\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());
}

static ReactiveControllerFeedbackBinding
feedback_note_binding (size_t slot, int channel, int number)
{
	ReactiveControllerFeedbackBinding binding;
	binding.slot = slot;
	binding.type = ReactiveControllerFeedbackBinding::Note;
	binding.channel = channel;
	binding.number = number;
	return binding;
}

static ReactiveControllerFeedbackBinding
feedback_cc_binding (size_t slot, int channel, int number)
{
	ReactiveControllerFeedbackBinding binding;
	binding.slot = slot;
	binding.type = ReactiveControllerFeedbackBinding::ControlChange;
	binding.channel = channel;
	binding.number = number;
	return binding;
}

static void
assert_feedback_message (ReactiveControllerFeedbackMidiMessage const& message, size_t slot, unsigned char status, unsigned char number, unsigned char value)
{
	CPPUNIT_ASSERT_EQUAL (slot, message.slot);
	CPPUNIT_ASSERT_EQUAL (size_t (3), message.bytes.size ());
	CPPUNIT_ASSERT_EQUAL (status, message.bytes[0]);
	CPPUNIT_ASSERT_EQUAL (number, message.bytes[1]);
	CPPUNIT_ASSERT_EQUAL (value, message.bytes[2]);
}

static Temporal::TempoMap
simple_tempo_map ()
{
	return Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4));
}

} // namespace

void
ReactiveActionSlotRunnerTest::executeSlotByDocumentOrder ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	load_two_action_document (runner);
	ReactiveExecutionResult result = runner.execute_slot (1, target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (2), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (2), runner.action_count ());
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), runner.action_name (1));
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), runner.last_action ());
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), runner.state_value ("section"));
	ReactiveActionSlotExecutionStatus const status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (1), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), status.action_name);
	CPPUNIT_ASSERT_EQUAL (true, status.result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (2), status.result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (2), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("trigger:1:3"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("state:section:breakdown"), target.calls[1]);
}

void
ReactiveActionSlotRunnerTest::executeMidiNoteTriggerByDocumentOrder ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION first\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 1\n"
		"END\n",
		error));

	ReactiveExecutionResult result = runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:0"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), runner.last_action ());
	ReactiveActionSlotExecutionStatus const status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (0), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), status.action_name);
	CPPUNIT_ASSERT_EQUAL (true, status.result.ok);
}

void
ReactiveActionSlotRunnerTest::executeMidiCCTriggerMacro ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION knob.high\n"
		"TRIGGER midi cc ch=1 cc=22 value>63\n"
		"DO macro filter 0.80 ramp 0|1|0\n"
		"END\n",
		error));

	ReactiveExecutionResult result = runner.execute_midi_event (ReactiveMidiEvent::control_change (1, 22, 64), target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.8:0|1|0"), target.calls[0]);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, runner.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.high"), runner.last_execution_status ().action_name);
}

void
ReactiveActionSlotRunnerTest::executeMidiCCTriggerMacroValueFromController ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION knob.live\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO macro filter midi-value ramp 0|1|0\n"
		"END\n",
		error));

	ReactiveExecutionResult mid = runner.execute_midi_event (ReactiveMidiEvent::control_change (1, 22, 64), target);

	CPPUNIT_ASSERT_EQUAL (true, mid.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), mid.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.503937:0|1|0"), target.calls[0]);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, runner.macro_value ("filter"), 0.0001);

	std::vector<ReactiveMacroSlotSummary> summary = runner.macro_bank_summary (8);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), summary[0].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, summary[0].value, 0.0001);

	ReactiveExecutionResult full = runner.execute_midi_event (ReactiveMidiEvent::control_change (1, 22, 127), target);

	CPPUNIT_ASSERT_EQUAL (true, full.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (2), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:1:0|1|0"), target.calls[1]);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.0, runner.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.live"), runner.last_execution_status ().action_name);
}

void
ReactiveActionSlotRunnerTest::executeMidiCCTriggerRhythmRouteValueFromController ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION knob.live.rhythm\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO rhythm route 0 density midi-value\n"
		"END\n",
		error));

	ReactiveExecutionResult result = runner.execute_midi_event (ReactiveMidiEvent::control_change (1, 22, 64), target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm-route:0:density:0.503937"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.live.rhythm"), runner.last_execution_status ().action_name);
}

void
ReactiveActionSlotRunnerTest::executeMidiCCTriggerProbabilityValueFromController ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION knob.live.clip\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO trigger probability 0 1 midi-value\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult result = runner.execute_midi_event (ReactiveMidiEvent::control_change (1, 22, 64), target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("trigger-probability:0:1:0.503937"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.live.clip"), runner.last_execution_status ().action_name);
}

void
ReactiveActionSlotRunnerTest::executeMidiBytesThroughRunner ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;
	unsigned char const note_bytes[] = { 0x99, 36, 100 };
	unsigned char const cc_bytes[] = { 0xb0, 22, 64 };

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION knob.high\n"
		"TRIGGER midi cc ch=1 cc=22 value>63\n"
		"DO macro filter midi-value\n"
		"END\n",
		error));

	ReactiveExecutionResult note = runner.execute_midi_bytes (note_bytes, 3, target);
	CPPUNIT_ASSERT_EQUAL (true, note.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), note.commands_executed);
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), runner.last_execution_status ().action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), runner.last_action ());

	ReactiveExecutionResult cc = runner.execute_midi_bytes (cc_bytes, 3, target);
	CPPUNIT_ASSERT_EQUAL (true, cc.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), cc.commands_executed);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.high"), runner.last_execution_status ().action_name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, runner.macro_value ("filter"), 0.0001);

	CPPUNIT_ASSERT_EQUAL (size_t (2), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:0"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.503937:0|0|0"), target.calls[1]);
}

void
ReactiveActionSlotRunnerTest::summarizeLastMidiInputForPanelAndStatus ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;
	unsigned char const cc_bytes[] = { 0xb0, 22, 64 };
	unsigned char const note_off[] = { 0x89, 36, 64 };

	CPPUNIT_ASSERT_EQUAL (false, runner.last_midi_input_summary ().available);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI Input: none"), runner.format_midi_input_summary ());

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION knob.live\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO macro filter midi-value\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult note = runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target);
	CPPUNIT_ASSERT_EQUAL (true, note.ok);

	ReactiveMidiInputSummary summary = runner.last_midi_input_summary ();
	CPPUNIT_ASSERT_EQUAL (true, summary.available);
	CPPUNIT_ASSERT_EQUAL (std::string ("note"), summary.event_type);
	CPPUNIT_ASSERT_EQUAL (int (10), summary.channel);
	CPPUNIT_ASSERT_EQUAL (int (36), summary.number);
	CPPUNIT_ASSERT_EQUAL (int (100), summary.value);
	CPPUNIT_ASSERT_EQUAL (false, summary.from_bytes);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.matched_action_count);
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary.matched_slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), summary.matched_action_name);
	CPPUNIT_ASSERT (runner.format_midi_input_summary ().find ("MIDI Input: note ch=10 note=36 velocity=100 source=event matches=1 action=pad.one") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_panel_summary (2).find ("MIDI Input: note ch=10 note=36 velocity=100 source=event matches=1 action=pad.one") != std::string::npos);

	ReactiveExecutionResult cc = runner.execute_midi_bytes (cc_bytes, 3, target);
	CPPUNIT_ASSERT_EQUAL (true, cc.ok);

	summary = runner.last_midi_input_summary ();
	CPPUNIT_ASSERT_EQUAL (true, summary.available);
	CPPUNIT_ASSERT_EQUAL (std::string ("cc"), summary.event_type);
	CPPUNIT_ASSERT_EQUAL (int (1), summary.channel);
	CPPUNIT_ASSERT_EQUAL (int (22), summary.number);
	CPPUNIT_ASSERT_EQUAL (int (64), summary.value);
	CPPUNIT_ASSERT_EQUAL (true, summary.from_bytes);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.matched_action_count);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.matched_slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.live"), summary.matched_action_name);
	CPPUNIT_ASSERT (runner.format_midi_input_summary ().find ("MIDI Input: cc ch=1 cc=22 value=64 source=bytes matches=1 action=knob.live") != std::string::npos);

	ReactiveExecutionResult no_match = runner.execute_midi_event (ReactiveMidiEvent::control_change (1, 23, 64), target);
	CPPUNIT_ASSERT_EQUAL (false, no_match.ok);

	summary = runner.last_midi_input_summary ();
	CPPUNIT_ASSERT_EQUAL (true, summary.available);
	CPPUNIT_ASSERT_EQUAL (std::string ("cc"), summary.event_type);
	CPPUNIT_ASSERT_EQUAL (int (23), summary.number);
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary.matched_action_count);
	CPPUNIT_ASSERT_EQUAL (std::string (), summary.matched_action_name);
	CPPUNIT_ASSERT (runner.format_midi_input_summary ().find ("MIDI Input: cc ch=1 cc=23 value=64 source=event matches=0") != std::string::npos);

	ReactiveExecutionResult unsupported = runner.execute_midi_bytes (note_off, 3, target);
	CPPUNIT_ASSERT_EQUAL (false, unsupported.ok);
	ReactiveMidiInputSummary const after_unsupported = runner.last_midi_input_summary ();
	CPPUNIT_ASSERT_EQUAL (summary.event_type, after_unsupported.event_type);
	CPPUNIT_ASSERT_EQUAL (summary.number, after_unsupported.number);
	CPPUNIT_ASSERT_EQUAL (summary.value, after_unsupported.value);
	CPPUNIT_ASSERT_EQUAL (summary.matched_action_count, after_unsupported.matched_action_count);
}

void
ReactiveActionSlotRunnerTest::executeMarkerTriggerByDocumentOrder ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 9\n"
		"END\n"
		"ACTION first.marker\n"
		"TRIGGER marker Breakdown\n"
		"DO cue 0\n"
		"END\n"
		"ACTION second.marker\n"
		"TRIGGER marker Breakdown\n"
		"DO cue 1\n"
		"END\n",
		error));

	ReactiveExecutionResult result = runner.execute_marker_event (ReactiveMarkerEvent::named ("Breakdown"), target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:0"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("first.marker"), runner.last_action ());
	ReactiveActionSlotExecutionStatus const status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (1), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("first.marker"), status.action_name);
	CPPUNIT_ASSERT_EQUAL (true, status.result.ok);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("marker Breakdown") != std::string::npos);
}

void
ReactiveActionSlotRunnerTest::executeSceneTriggerByDocumentOrder ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 9\n"
		"END\n"
		"ACTION first.scene\n"
		"TRIGGER scene 3\n"
		"DO cue 3\n"
		"END\n"
		"ACTION second.scene\n"
		"TRIGGER scene 3\n"
		"DO cue 4\n"
		"END\n",
		error));

	ReactiveExecutionResult result = runner.execute_scene_event (ReactiveSceneEvent::numbered (3), target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:3"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("first.scene"), runner.last_action ());
	ReactiveActionSlotExecutionStatus const status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (1), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("first.scene"), status.action_name);
	CPPUNIT_ASSERT_EQUAL (true, status.result.ok);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("scene 3") != std::string::npos);
}

void
ReactiveActionSlotRunnerTest::executeRegionTriggerByDocumentOrder ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 9\n"
		"END\n"
		"ACTION first.region\n"
		"TRIGGER region Breakdown Loop\n"
		"DO cue 3\n"
		"END\n"
		"ACTION second.region\n"
		"TRIGGER region Breakdown Loop\n"
		"DO cue 4\n"
		"END\n",
		error));

	ReactiveExecutionResult result = runner.execute_region_event (ReactiveRegionEvent::named ("Breakdown Loop"), target);

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:3"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("first.region"), runner.last_action ());
	ReactiveActionSlotExecutionStatus const status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (1), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("first.region"), status.action_name);
	CPPUNIT_ASSERT_EQUAL (true, status.result.ok);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("region Breakdown Loop") != std::string::npos);
}

void
ReactiveActionSlotRunnerTest::reportMidiEventWithoutLoadedDocumentOrMatch ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	ReactiveExecutionResult missing = runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target);

	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("no reactive action document loaded") != std::string::npos);
	ReactiveActionSlotExecutionStatus status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT (target.calls.empty ());

	load_two_action_document (runner);
	ReactiveExecutionResult no_match = runner.execute_midi_event (ReactiveMidiEvent::control_change (1, 22, 64), target);

	CPPUNIT_ASSERT_EQUAL (false, no_match.ok);
	CPPUNIT_ASSERT (no_match.error.find ("no reactive action matched MIDI event") != std::string::npos);
	status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT_EQUAL (false, status.result.ok);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionSlotRunnerTest::reportMarkerEventWithoutLoadedDocumentOrMatch ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	ReactiveExecutionResult missing = runner.execute_marker_event (ReactiveMarkerEvent::named ("Breakdown"), target);

	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("no reactive action document loaded") != std::string::npos);
	ReactiveActionSlotExecutionStatus status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT (target.calls.empty ());

	load_two_action_document (runner);
	ReactiveExecutionResult no_match = runner.execute_marker_event (ReactiveMarkerEvent::named ("Breakdown"), target);

	CPPUNIT_ASSERT_EQUAL (false, no_match.ok);
	CPPUNIT_ASSERT (no_match.error.find ("no reactive action matched marker") != std::string::npos);
	status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT_EQUAL (false, status.result.ok);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionSlotRunnerTest::reportSceneEventWithoutLoadedDocumentOrMatch ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	ReactiveExecutionResult missing = runner.execute_scene_event (ReactiveSceneEvent::numbered (3), target);

	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("no reactive action document loaded") != std::string::npos);
	ReactiveActionSlotExecutionStatus status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT (target.calls.empty ());

	load_two_action_document (runner);
	ReactiveExecutionResult no_match = runner.execute_scene_event (ReactiveSceneEvent::numbered (3), target);

	CPPUNIT_ASSERT_EQUAL (false, no_match.ok);
	CPPUNIT_ASSERT (no_match.error.find ("no reactive action matched scene") != std::string::npos);
	status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT_EQUAL (false, status.result.ok);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionSlotRunnerTest::reportRegionEventWithoutLoadedDocumentOrMatch ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	ReactiveExecutionResult missing = runner.execute_region_event (ReactiveRegionEvent::named ("Breakdown Loop"), target);

	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("no reactive action document loaded") != std::string::npos);
	ReactiveActionSlotExecutionStatus status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT (target.calls.empty ());

	load_two_action_document (runner);
	ReactiveExecutionResult no_match = runner.execute_region_event (ReactiveRegionEvent::named ("Breakdown Loop"), target);

	CPPUNIT_ASSERT_EQUAL (false, no_match.ok);
	CPPUNIT_ASSERT (no_match.error.find ("no reactive action matched region") != std::string::npos);
	status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT_EQUAL (false, status.result.ok);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionSlotRunnerTest::reportUnsupportedMidiBytes ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	unsigned char const valid_note[] = { 0x99, 36, 100 };
	unsigned char const note_off[] = { 0x89, 36, 0 };
	unsigned char const short_message[] = { 0x99, 36 };

	ReactiveExecutionResult missing = runner.execute_midi_bytes (valid_note, 3, target);
	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("no reactive action document loaded") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());

	load_two_action_document (runner);

	ReactiveExecutionResult note_off_result = runner.execute_midi_bytes (note_off, 3, target);
	CPPUNIT_ASSERT_EQUAL (false, note_off_result.ok);
	CPPUNIT_ASSERT (note_off_result.error.find ("unsupported reactive MIDI byte message") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().result.ok);
	CPPUNIT_ASSERT (runner.last_execution_status ().result.error.find ("unsupported reactive MIDI byte message") != std::string::npos);

	ReactiveExecutionResult short_result = runner.execute_midi_bytes (short_message, 2, target);
	CPPUNIT_ASSERT_EQUAL (false, short_result.ok);
	CPPUNIT_ASSERT (short_result.error.find ("unsupported reactive MIDI byte message") != std::string::npos);

	ReactiveExecutionResult null_result = runner.execute_midi_bytes (0, 3, target);
	CPPUNIT_ASSERT_EQUAL (false, null_result.ok);
	CPPUNIT_ASSERT (null_result.error.find ("unsupported reactive MIDI byte message") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionSlotRunnerTest::executeBuiltInMvpFallbackRhythmDemo ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (ReactiveActionDocumentLoader::mvp_fallback_source (), error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (8), runner.action_count ());
	CPPUNIT_ASSERT_EQUAL (std::string ("mvp.cue.0"), runner.action_name (0));
	CPPUNIT_ASSERT_EQUAL (std::string ("mvp.cue.7"), runner.action_name (7));

	ReactiveExecutionResult install = runner.execute_slot (0, target);
	CPPUNIT_ASSERT_EQUAL (true, install.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (6), install.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (6), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm-insert:0"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm:density:1"), target.calls[1]);
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm:chance:1"), target.calls[2]);
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm:priority_mode:0"), target.calls[3]);
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm:rotation:0"), target.calls[4]);
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:0"), target.calls[5]);

	target.calls.clear ();
	ReactiveExecutionResult variation = runner.execute_slot (6, target);
	CPPUNIT_ASSERT_EQUAL (true, variation.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (3), variation.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (3), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm:chance:0.25"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("rhythm:priority_mode:2"), target.calls[1]);
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:6"), target.calls[2]);
}

void
ReactiveActionSlotRunnerTest::reportMissingDocumentAndOutOfRangeSlot ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	ReactiveExecutionResult missing = runner.execute_slot (0, target);

	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("no reactive action document loaded") != std::string::npos);
	ReactiveActionSlotExecutionStatus status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (0), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT_EQUAL (false, status.result.ok);
	CPPUNIT_ASSERT (status.result.error.find ("no reactive action document loaded") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (size_t (0), status.result.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());

	load_two_action_document (runner);
	ReactiveExecutionResult out_of_range = runner.execute_slot (2, target);

	CPPUNIT_ASSERT_EQUAL (false, out_of_range.ok);
	CPPUNIT_ASSERT (out_of_range.error.find ("reactive action slot 2 is out of range") != std::string::npos);
	status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (2), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string (), status.action_name);
	CPPUNIT_ASSERT_EQUAL (false, status.result.ok);
	CPPUNIT_ASSERT (status.result.error.find ("reactive action slot 2 is out of range") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionSlotRunnerTest::rejectInvalidSource ()
{
	ReactiveActionSlotRunner runner;
	std::string error;

	load_two_action_document (runner);
	CPPUNIT_ASSERT_EQUAL (true, runner.loaded ());
	CPPUNIT_ASSERT_EQUAL (size_t (2), runner.action_count ());

	CPPUNIT_ASSERT_EQUAL (false, runner.load_source (
		"ACTION broken\n"
		"DO warp now\n"
		"END\n",
		error));

	CPPUNIT_ASSERT (error.find ("unknown command") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (false, runner.loaded ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.action_count ());
}

void
ReactiveActionSlotRunnerTest::propagateTargetFailure ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target ("cue:2");

	load_two_action_document (runner);
	ReactiveExecutionResult result = runner.execute_slot (0, target);

	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("target failed cue:2") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (size_t (0), result.commands_executed);
	ReactiveActionSlotExecutionStatus const status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (0), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro"), status.action_name);
	CPPUNIT_ASSERT_EQUAL (false, status.result.ok);
	CPPUNIT_ASSERT (status.result.error.find ("target failed cue:2") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (size_t (0), status.result.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
}

void
ReactiveActionSlotRunnerTest::resetExecutionStatusOnClearAndLoad ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;

	load_two_action_document (runner);
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);
	CPPUNIT_ASSERT_EQUAL (true, runner.last_execution_status ().attempted);

	runner.clear ();
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().attempted);

	load_two_action_document (runner);
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().attempted);
}

void
ReactiveActionSlotRunnerTest::disabledPerformanceModeBlocksExecutionAndReportsStatus ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.performance_enabled ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Performance Mode: enabled"), runner.format_performance_mode_status ());

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION build\n"
		"CHAIN sequential\n"
		"DO macro filter 0.25\n"
		"DO cue 1\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	runner.set_performance_enabled (false);
	CPPUNIT_ASSERT_EQUAL (false, runner.performance_enabled ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Performance Mode: disabled"), runner.format_performance_mode_status ());

	ReactiveExecutionResult disabled = runner.execute_slot (0, target);
	CPPUNIT_ASSERT_EQUAL (false, disabled.ok);
	CPPUNIT_ASSERT (disabled.error.find ("disabled") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (size_t (0), disabled.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (std::string (), runner.last_action ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, runner.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (true, runner.last_execution_status ().attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.last_execution_status ().slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("build"), runner.last_execution_status ().action_name);
	CPPUNIT_ASSERT_EQUAL (false, runner.next_action_preview ().available);

	runner.set_performance_enabled (true);
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.25:0|0|0"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::summarizeActionBankForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION knob.high\n"
		"TRIGGER midi cc ch=1 cc=22 value>63\n"
		"DO macro filter 0.80\n"
		"END\n"
		"ACTION manual.only\n"
		"DO state section bridge\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactiveActionSlotSummary> summary = runner.action_bank_summary (8);

	CPPUNIT_ASSERT_EQUAL (size_t (3), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), summary[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI note ch=10 note=36"), summary[0].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[0].command_count);
	CPPUNIT_ASSERT_EQUAL (false, summary[0].latest_attempted);

	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.high"), summary[1].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI cc ch=1 cc=22 value>63"), summary[1].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].command_count);
	CPPUNIT_ASSERT_EQUAL (false, summary[1].latest_attempted);

	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[2].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("manual.only"), summary[2].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("manual"), summary[2].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[2].command_count);
	CPPUNIT_ASSERT_EQUAL (false, summary[2].latest_attempted);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);
	summary = runner.action_bank_summary (8);
	CPPUNIT_ASSERT_EQUAL (false, summary[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (true, summary[1].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (false, summary[2].latest_attempted);
}

void
ReactiveActionSlotRunnerTest::summarizePerformanceControlsForStatusPanel ()
{
	ReactiveActionSlotRunner runner;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION manual.only\n"
		"DO state section bridge\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactivePerformanceControlSummary> controls = runner.performance_control_summary (4);

	CPPUNIT_ASSERT_EQUAL (size_t (4), controls.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), controls[0].slot);
	CPPUNIT_ASSERT_EQUAL (true, controls[0].available);
	CPPUNIT_ASSERT_EQUAL (true, controls[0].enabled);
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), controls[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI note ch=10 note=36"), controls[0].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("0 pad.one"), controls[0].button_label);

	CPPUNIT_ASSERT_EQUAL (size_t (1), controls[1].slot);
	CPPUNIT_ASSERT_EQUAL (true, controls[1].available);
	CPPUNIT_ASSERT_EQUAL (true, controls[1].enabled);
	CPPUNIT_ASSERT_EQUAL (std::string ("manual.only"), controls[1].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("manual"), controls[1].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("1 manual.only"), controls[1].button_label);

	CPPUNIT_ASSERT_EQUAL (size_t (2), controls[2].slot);
	CPPUNIT_ASSERT_EQUAL (false, controls[2].available);
	CPPUNIT_ASSERT_EQUAL (false, controls[2].enabled);
	CPPUNIT_ASSERT_EQUAL (std::string ("2 empty"), controls[2].button_label);

	runner.set_performance_enabled (false);
	controls = runner.performance_control_summary (2);
	CPPUNIT_ASSERT_EQUAL (true, controls[0].available);
	CPPUNIT_ASSERT_EQUAL (false, controls[0].enabled);
	CPPUNIT_ASSERT_EQUAL (true, controls[1].available);
	CPPUNIT_ASSERT_EQUAL (false, controls[1].enabled);

	std::string const formatted = runner.format_performance_control_summary (4);
	CPPUNIT_ASSERT (formatted.find ("Performance controls: disabled") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("0: pad.one [MIDI note ch=10 note=36] - disabled") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("2: empty - unavailable") != std::string::npos);
}

void
ReactiveActionSlotRunnerTest::performanceControlsMarkLatestAttemptForPanelRefresh ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION manual.two\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactivePerformanceControlSummary> controls = runner.performance_control_summary (3);
	CPPUNIT_ASSERT_EQUAL (std::string ("0 pad.one"), controls[0].button_label);
	CPPUNIT_ASSERT_EQUAL (std::string ("1 manual.two"), controls[1].button_label);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target).ok);
	controls = runner.performance_control_summary (3);
	CPPUNIT_ASSERT_EQUAL (std::string ("> 0 pad.one"), controls[0].button_label);
	CPPUNIT_ASSERT_EQUAL (std::string ("1 manual.two"), controls[1].button_label);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);
	controls = runner.performance_control_summary (3);
	CPPUNIT_ASSERT_EQUAL (std::string ("0 pad.one"), controls[0].button_label);
	CPPUNIT_ASSERT_EQUAL (std::string ("> 1 manual.two"), controls[1].button_label);
}

void
ReactiveActionSlotRunnerTest::controllerFeedbackSummarizesLatestAttempt ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION manual.two\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactiveControllerFeedbackSummary> feedback = runner.controller_feedback_summary (3);
	CPPUNIT_ASSERT_EQUAL (size_t (3), feedback.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), feedback[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), feedback[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI note ch=10 note=36"), feedback[0].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (true, feedback[0].available);
	CPPUNIT_ASSERT_EQUAL (true, feedback[0].enabled);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (32, feedback[0].value);
	CPPUNIT_ASSERT_EQUAL (false, feedback[2].available);
	CPPUNIT_ASSERT_EQUAL (false, feedback[2].enabled);
	CPPUNIT_ASSERT_EQUAL (false, feedback[2].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (0, feedback[2].value);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target).ok);
	feedback = runner.controller_feedback_summary (3);
	CPPUNIT_ASSERT_EQUAL (true, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (127, feedback[0].value);
	CPPUNIT_ASSERT_EQUAL (false, feedback[1].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (32, feedback[1].value);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);
	feedback = runner.controller_feedback_summary (3);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (32, feedback[0].value);
	CPPUNIT_ASSERT_EQUAL (true, feedback[1].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (127, feedback[1].value);

	runner.set_performance_enabled (false);
	feedback = runner.controller_feedback_summary (2);
	CPPUNIT_ASSERT_EQUAL (true, feedback[0].available);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].enabled);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (0, feedback[0].value);
	CPPUNIT_ASSERT_EQUAL (true, feedback[1].available);
	CPPUNIT_ASSERT_EQUAL (false, feedback[1].enabled);
	CPPUNIT_ASSERT_EQUAL (false, feedback[1].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (0, feedback[1].value);
}

void
ReactiveActionSlotRunnerTest::controllerFeedbackSummarizesQueuedActions ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION queued.pad\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 1\n"
		"END\n"
		"ACTION instant.pad\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_or_queue_slot (
		0,
		target,
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (2, 1, 0)).ok);

	std::vector<ReactiveControllerFeedbackSummary> feedback = runner.controller_feedback_summary (2);
	CPPUNIT_ASSERT_EQUAL (true, feedback[0].queued);
	CPPUNIT_ASSERT_EQUAL (true, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (127, feedback[0].value);
	CPPUNIT_ASSERT_EQUAL (false, feedback[1].queued);
	CPPUNIT_ASSERT_EQUAL (32, feedback[1].value);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);
	feedback = runner.controller_feedback_summary (2);
	CPPUNIT_ASSERT_EQUAL (true, feedback[0].queued);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (96, feedback[0].value);
	CPPUNIT_ASSERT_EQUAL (false, feedback[1].queued);
	CPPUNIT_ASSERT_EQUAL (true, feedback[1].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (127, feedback[1].value);

	CPPUNIT_ASSERT_EQUAL (true, runner.release_due_queued_actions (Temporal::BBT_Time (2, 1, 0), target).ok);
	feedback = runner.controller_feedback_summary (2);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].queued);
	CPPUNIT_ASSERT_EQUAL (true, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (127, feedback[0].value);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);
	feedback = runner.controller_feedback_summary (2);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].queued);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (32, feedback[0].value);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_or_queue_slot (
		0,
		target,
		Temporal::BBT_Time (3, 1, 0),
		Temporal::BBT_Time (4, 1, 0)).ok);
	runner.set_performance_enabled (false);
	feedback = runner.controller_feedback_summary (2);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].queued);
	CPPUNIT_ASSERT_EQUAL (false, feedback[0].latest_attempted);
	CPPUNIT_ASSERT_EQUAL (0, feedback[0].value);
}

void
ReactiveActionSlotRunnerTest::controllerFeedbackMidiMessagesFollowSlotFeedbackState ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION manual.two\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactiveControllerFeedbackBinding> bindings;
	bindings.push_back (feedback_note_binding (0, 10, 36));
	bindings.push_back (feedback_cc_binding (1, 1, 22));
	bindings.push_back (feedback_note_binding (2, 10, 38));

	std::vector<ReactiveControllerFeedbackMidiMessage> messages = runner.controller_feedback_midi_messages (bindings);
	CPPUNIT_ASSERT_EQUAL (size_t (3), messages.size ());
	assert_feedback_message (messages[0], 0, 0x99, 36, 32);
	assert_feedback_message (messages[1], 1, 0xb0, 22, 32);
	assert_feedback_message (messages[2], 2, 0x99, 38, 0);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target).ok);
	messages = runner.controller_feedback_midi_messages (bindings);
	CPPUNIT_ASSERT_EQUAL (size_t (3), messages.size ());
	assert_feedback_message (messages[0], 0, 0x99, 36, 127);
	assert_feedback_message (messages[1], 1, 0xb0, 22, 32);
	assert_feedback_message (messages[2], 2, 0x99, 38, 0);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);
	messages = runner.controller_feedback_midi_messages (bindings);
	CPPUNIT_ASSERT_EQUAL (size_t (3), messages.size ());
	assert_feedback_message (messages[0], 0, 0x99, 36, 32);
	assert_feedback_message (messages[1], 1, 0xb0, 22, 127);
	assert_feedback_message (messages[2], 2, 0x99, 38, 0);

	runner.set_performance_enabled (false);
	messages = runner.controller_feedback_midi_messages (bindings);
	CPPUNIT_ASSERT_EQUAL (size_t (3), messages.size ());
	assert_feedback_message (messages[0], 0, 0x99, 36, 0);
	assert_feedback_message (messages[1], 1, 0xb0, 22, 0);
	assert_feedback_message (messages[2], 2, 0x99, 38, 0);
}

void
ReactiveActionSlotRunnerTest::controllerFeedbackMidiMessagesShowQueuedActions ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION queued.pad\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 1\n"
		"END\n"
		"ACTION instant.pad\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactiveControllerFeedbackBinding> bindings;
	bindings.push_back (feedback_note_binding (0, 10, 36));
	bindings.push_back (feedback_note_binding (1, 10, 37));

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_or_queue_slot (
		0,
		target,
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (2, 1, 0)).ok);
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);

	std::vector<ReactiveControllerFeedbackMidiMessage> messages = runner.controller_feedback_midi_messages (bindings);
	CPPUNIT_ASSERT_EQUAL (size_t (2), messages.size ());
	assert_feedback_message (messages[0], 0, 0x99, 36, 96);
	assert_feedback_message (messages[1], 1, 0x99, 37, 127);
}

void
ReactiveActionSlotRunnerTest::controllerFeedbackMidiMessagesIgnoreInvalidBindings ()
{
	ReactiveActionSlotRunner runner;
	std::string error;
	load_two_action_document (runner);

	std::vector<ReactiveControllerFeedbackBinding> bindings;
	bindings.push_back (feedback_note_binding (0, 1, 0));
	bindings.push_back (feedback_note_binding (1, 0, 36));
	bindings.push_back (feedback_note_binding (1, 17, 36));
	bindings.push_back (feedback_note_binding (1, 10, -1));
	bindings.push_back (feedback_cc_binding (1, 10, 128));

	std::vector<ReactiveControllerFeedbackMidiMessage> messages = runner.controller_feedback_midi_messages (bindings);
	CPPUNIT_ASSERT_EQUAL (size_t (1), messages.size ());
	assert_feedback_message (messages[0], 0, 0x90, 0, 32);
}

void
ReactiveActionSlotRunnerTest::summarizeMacroBankForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT (runner.macro_bank_summary (8).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Macro bank: none"), runner.format_macro_bank_summary (8));

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION setup\n"
		"DO macro filter 0.25\n"
		"DO macro delay_send 0.40\n"
		"END\n"
		"ACTION perform\n"
		"DO macro filter 0.80\n"
		"DO macro resonance 0.55\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactiveMacroSlotSummary> summary = runner.macro_bank_summary (8);

	CPPUNIT_ASSERT_EQUAL (size_t (3), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), summary[0].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, summary[0].value, 0.0001);

	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("delay_send"), summary[1].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, summary[1].value, 0.0001);

	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[2].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("resonance"), summary[2].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, summary[2].value, 0.0001);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);

	summary = runner.macro_bank_summary (2);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), summary[0].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, summary[0].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("delay_send"), summary[1].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.40, summary[1].value, 0.0001);

	std::string const formatted = runner.format_macro_bank_summary (8);
	CPPUNIT_ASSERT (formatted.find ("0: filter = 0.8") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("1: delay_send = 0.4") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("2: resonance = 0.55") != std::string::npos);
}

void
ReactiveActionSlotRunnerTest::summarizeMacroSnapshotsForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT (runner.macro_snapshot_summary (8, 2).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Macro snapshots: none"), runner.format_macro_snapshot_summary (8, 2));

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION store.verse\n"
		"DO macro filter 0.25\n"
		"DO macro resonance 0.70\n"
		"DO macro snapshot store verse\n"
		"END\n"
		"ACTION store.chorus\n"
		"DO macro drive 0.33\n"
		"DO macro filter 0.80\n"
		"DO macro resonance 0.20\n"
		"DO macro snapshot store chorus\n"
		"END\n"
		"ACTION recall.verse\n"
		"DO macro snapshot recall verse ramp 0|2|0\n"
		"END\n"
		"ACTION morph.half\n"
		"DO macro morph verse chorus amount 0.50 ramp 0|1|0\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT (runner.macro_snapshot_summary (8, 2).empty ());

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);

	std::vector<ReactiveMacroSnapshotSummary> summary = runner.macro_snapshot_summary (8, 2);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("verse"), summary[0].name);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[0].value_count);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[0].values.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), summary[0].values[0].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.25, summary[0].values[0].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("resonance"), summary[0].values[1].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.70, summary[0].values[1].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("chorus"), summary[1].name);
	CPPUNIT_ASSERT_EQUAL (size_t (3), summary[1].value_count);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[1].values.size ());

	std::string formatted = runner.format_macro_snapshot_summary (8, 2);
	CPPUNIT_ASSERT (formatted.find ("0: verse - 2 values (filter=0.25, resonance=0.7)") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("1: chorus - 3 values (drive=0.33, filter=0.8, ...)") != std::string::npos);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (2, target).ok);
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (3, target).ok);
	summary = runner.macro_snapshot_summary (8, 2);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("verse"), summary[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("chorus"), summary[1].name);

	formatted = runner.format_panel_summary (2);
	CPPUNIT_ASSERT (formatted.find ("Macro Snapshots: verse[filter=0.25, resonance=0.7], chorus[drive=0.33, filter=0.8, ...]") != std::string::npos);

	runner.clear ();
	CPPUNIT_ASSERT (runner.macro_snapshot_summary (8, 2).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Macro snapshots: none"), runner.format_macro_snapshot_summary (8, 2));
}

void
ReactiveActionSlotRunnerTest::summarizeStateBankForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT (runner.state_bank_summary (8).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("State bank: none"), runner.format_state_bank_summary (8));

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION setup\n"
		"DO state section intro\n"
		"DO state energy low\n"
		"END\n"
		"ACTION perform\n"
		"DO state section breakdown\n"
		"DO state mode mutate\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactiveStateSlotSummary> summary = runner.state_bank_summary (8);

	CPPUNIT_ASSERT_EQUAL (size_t (3), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("section"), summary[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string (), summary[0].value);

	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("energy"), summary[1].name);
	CPPUNIT_ASSERT_EQUAL (std::string (), summary[1].value);

	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[2].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("mode"), summary[2].name);
	CPPUNIT_ASSERT_EQUAL (std::string (), summary[2].value);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);

	summary = runner.state_bank_summary (2);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("section"), summary[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), summary[0].value);
	CPPUNIT_ASSERT_EQUAL (std::string ("energy"), summary[1].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("low"), summary[1].value);

	std::string const formatted = runner.format_state_bank_summary (8);
	CPPUNIT_ASSERT (formatted.find ("0: section = breakdown") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("1: energy = low") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("2: mode = mutate") != std::string::npos);
}

void
ReactiveActionSlotRunnerTest::summarizeHarmonyBankForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT (runner.harmony_bank_summary (8).empty ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Harmony bank: none"), runner.format_harmony_bank_summary (8));

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION setup\n"
		"DO harmony key C_minor\n"
		"DO harmony chord i\n"
		"END\n"
		"ACTION perform\n"
		"DO harmony chord V\n"
		"DO harmony scale aeolian\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	std::vector<ReactiveHarmonySlotSummary> summary = runner.harmony_bank_summary (8);

	CPPUNIT_ASSERT_EQUAL (size_t (3), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("key"), summary[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string (), summary[0].value);

	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[1].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("chord"), summary[1].name);
	CPPUNIT_ASSERT_EQUAL (std::string (), summary[1].value);

	CPPUNIT_ASSERT_EQUAL (size_t (2), summary[2].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("scale"), summary[2].name);
	CPPUNIT_ASSERT_EQUAL (std::string (), summary[2].value);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (1, target).ok);

	summary = runner.harmony_bank_summary (2);
	CPPUNIT_ASSERT_EQUAL (size_t (2), summary.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("key"), summary[0].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("C_minor"), summary[0].value);
	CPPUNIT_ASSERT_EQUAL (std::string ("chord"), summary[1].name);
	CPPUNIT_ASSERT_EQUAL (std::string ("V"), summary[1].value);
	CPPUNIT_ASSERT_EQUAL (std::string ("V"), runner.harmony_value ("chord"));

	std::string const formatted = runner.format_harmony_bank_summary (8);
	CPPUNIT_ASSERT (formatted.find ("0: key = C_minor") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("1: chord = V") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("2: scale = aeolian") != std::string::npos);
}

void
ReactiveActionSlotRunnerTest::previewSlotForPerformancePanelWithoutMutatingState ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (std::string ("Next action preview: none"), runner.format_next_action_preview ());

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION build\n"
		"TRIGGER midi note ch=10 note=36\n"
		"QUANTIZE 1|0|0\n"
		"CHAIN sequential\n"
		"DO macro filter 0.25\n"
		"DO cue 1\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveActionPreviewSummary preview = runner.preview_slot (0);

	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (size_t (0), preview.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("build"), preview.action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI note ch=10 note=36"), preview.primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("sequential"), preview.chain_mode);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|0|0"), preview.quantize);
	CPPUNIT_ASSERT_EQUAL (Temporal::BBT_Offset (1, 0, 0), preview.quantize_offset);
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_count);
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_summaries.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("macro filter = 0.25 ramp 0|0|0"), preview.command_summaries[0]);
	CPPUNIT_ASSERT_EQUAL (std::string (), runner.last_action ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, runner.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().attempted);
	CPPUNIT_ASSERT (target.calls.empty ());

	preview = runner.preview_slot (0);
	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_count);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.25:0|0|0"), target.calls[0]);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.25, runner.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("Next action preview: slot 0 (build)") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("sequential, quantize 1|0|0, 1 command") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("Commands:\n  - cue row 1") != std::string::npos);

	preview = runner.preview_slot (0);
	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_count);
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_summaries.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue row 1"), preview.command_summaries[0]);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);
	CPPUNIT_ASSERT_EQUAL (size_t (2), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:1"), target.calls[1]);
}

void
ReactiveActionSlotRunnerTest::previewMidiEventForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION first\n"
		"TRIGGER midi note ch=10 note=36\n"
		"QUANTIZE 0|1|0\n"
		"DO cue 0\n"
		"DO state section intro\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 1\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveActionPreviewSummary preview = runner.preview_midi_event (ReactiveMidiEvent::note_on (10, 36, 100));

	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (size_t (0), preview.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), preview.action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI note ch=10 note=36"), preview.primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("all"), preview.chain_mode);
	CPPUNIT_ASSERT_EQUAL (std::string ("0|1|0"), preview.quantize);
	CPPUNIT_ASSERT_EQUAL (size_t (2), preview.command_count);
	CPPUNIT_ASSERT_EQUAL (size_t (2), preview.command_summaries.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue row 0"), preview.command_summaries[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("state section = intro"), preview.command_summaries[1]);
	CPPUNIT_ASSERT_EQUAL (std::string (), runner.state_value ("section"));
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().attempted);

	ReactiveActionPreviewSummary missing = runner.preview_midi_event (ReactiveMidiEvent::control_change (1, 22, 64));
	CPPUNIT_ASSERT_EQUAL (false, missing.available);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target).ok);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("Next action preview: slot 0 (first)") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("MIDI note ch=10 note=36") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro"), runner.state_value ("section"));
}

void
ReactiveActionSlotRunnerTest::previewMidiEventUsesControllerValueForCommandDetails ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION seed.snapshots\n"
		"DO macro texture 0.20\n"
		"DO macro space 0.80\n"
		"DO macro snapshot store texture_low\n"
		"DO macro texture 0.80\n"
		"DO macro space 0.25\n"
		"DO macro snapshot store texture_high\n"
		"END\n"
		"ACTION morph.from.cc\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO macro morph texture_low texture_high amount midi-value ramp 0|1|0\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);

	ReactiveActionPreviewSummary preview = runner.preview_midi_event (ReactiveMidiEvent::control_change (1, 22, 64));

	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (std::string ("morph.from.cc"), preview.action_name);
	CPPUNIT_ASSERT_EQUAL (size_t (2), preview.command_count);
	CPPUNIT_ASSERT_EQUAL (size_t (2), preview.command_summaries.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("macro space = 0.522835 ramp 0|1|0"), preview.command_summaries[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("macro texture = 0.502362 ramp 0|1|0"), preview.command_summaries[1]);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, runner.macro_value ("texture"), 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.25, runner.macro_value ("space"), 0.0001);
}

void
ReactiveActionSlotRunnerTest::previewMidiEventShowsTriggerProbabilityCommandDetails ()
{
	ReactiveActionSlotRunner runner;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION knob.live.clip\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO trigger probability 0 1 midi-value\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveActionPreviewSummary preview = runner.preview_midi_event (ReactiveMidiEvent::control_change (1, 22, 64));

	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.live.clip"), preview.action_name);
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_count);
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_summaries.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("trigger route 0 row 1 probability = 0.503937"), preview.command_summaries[0]);
}

void
ReactiveActionSlotRunnerTest::previewMarkerEventForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION first\n"
		"TRIGGER marker Breakdown\n"
		"QUANTIZE 0|1|0\n"
		"DO cue 0\n"
		"DO state section breakdown\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER marker Breakdown\n"
		"DO cue 1\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveActionPreviewSummary preview = runner.preview_marker_event (ReactiveMarkerEvent::named ("Breakdown"));

	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (size_t (0), preview.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), preview.action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("marker Breakdown"), preview.primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("all"), preview.chain_mode);
	CPPUNIT_ASSERT_EQUAL (std::string ("0|1|0"), preview.quantize);
	CPPUNIT_ASSERT_EQUAL (size_t (2), preview.command_count);
	CPPUNIT_ASSERT_EQUAL (std::string (), runner.state_value ("section"));
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().attempted);

	ReactiveActionPreviewSummary missing = runner.preview_marker_event (ReactiveMarkerEvent::named ("Drop"));
	CPPUNIT_ASSERT_EQUAL (false, missing.available);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_marker_event (ReactiveMarkerEvent::named ("Breakdown"), target).ok);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("Next action preview: slot 0 (first)") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("marker Breakdown") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), runner.state_value ("section"));
}

void
ReactiveActionSlotRunnerTest::previewSceneEventForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION first\n"
		"TRIGGER scene 3\n"
		"QUANTIZE 0|1|0\n"
		"DO cue 3\n"
		"DO state section drop\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER scene 3\n"
		"DO cue 4\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveActionPreviewSummary preview = runner.preview_scene_event (ReactiveSceneEvent::numbered (3));

	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (size_t (0), preview.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), preview.action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("scene 3"), preview.primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("all"), preview.chain_mode);
	CPPUNIT_ASSERT_EQUAL (std::string ("0|1|0"), preview.quantize);
	CPPUNIT_ASSERT_EQUAL (size_t (2), preview.command_count);
	CPPUNIT_ASSERT_EQUAL (std::string (), runner.state_value ("section"));
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().attempted);

	ReactiveActionPreviewSummary missing = runner.preview_scene_event (ReactiveSceneEvent::numbered (4));
	CPPUNIT_ASSERT_EQUAL (false, missing.available);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_scene_event (ReactiveSceneEvent::numbered (3), target).ok);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("Next action preview: slot 0 (first)") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("scene 3") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (std::string ("drop"), runner.state_value ("section"));
}

void
ReactiveActionSlotRunnerTest::previewRegionEventForPerformancePanel ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION first\n"
		"TRIGGER region Breakdown Loop\n"
		"QUANTIZE 0|1|0\n"
		"DO cue 3\n"
		"DO state section drop\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER region Breakdown Loop\n"
		"DO cue 4\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveActionPreviewSummary preview = runner.preview_region_event (ReactiveRegionEvent::named ("Breakdown Loop"));

	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (size_t (0), preview.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), preview.action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("region Breakdown Loop"), preview.primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("all"), preview.chain_mode);
	CPPUNIT_ASSERT_EQUAL (std::string ("0|1|0"), preview.quantize);
	CPPUNIT_ASSERT_EQUAL (size_t (2), preview.command_count);
	CPPUNIT_ASSERT_EQUAL (std::string (), runner.state_value ("section"));
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().attempted);

	ReactiveActionPreviewSummary missing = runner.preview_region_event (ReactiveRegionEvent::named ("Verse Loop"));
	CPPUNIT_ASSERT_EQUAL (false, missing.available);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_region_event (ReactiveRegionEvent::named ("Breakdown Loop"), target).ok);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("Next action preview: slot 0 (first)") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("region Breakdown Loop") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (std::string ("drop"), runner.state_value ("section"));
}

void
ReactiveActionSlotRunnerTest::formatCompactPanelSummaryForCuePage ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (std::string ("Next: none\nMIDI Input: none\nMacros: none\nMacro Snapshots: none\nStates: none\nHarmony: none"), runner.format_panel_summary (2));

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION setup\n"
		"DO macro filter 0.25\n"
		"DO state section intro\n"
		"DO harmony key C_minor\n"
		"DO cue 0\n"
		"END\n"
		"ACTION perform\n"
		"DO macro resonance 0.55\n"
		"DO state section drop\n"
		"DO state energy high\n"
		"DO harmony chord V\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_slot (0, target).ok);

	std::string const formatted = runner.format_panel_summary (2);
	CPPUNIT_ASSERT (formatted.find ("Next: slot 0 setup - all, q 0|0|0, 4 cmds") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("Commands: macro filter = 0.25 ramp 0|0|0, state section = intro") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("Macros: filter=0.25, resonance=0") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("States: section=intro, energy=") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("Harmony: key=C_minor, chord=") != std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("Action bank:") == std::string::npos);
	CPPUNIT_ASSERT (formatted.find ("Reactive Performance Mode") == std::string::npos);
}

void
ReactiveActionSlotRunnerTest::transportProviderControlsSlotConditions ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;
	bool rolling = false;

	runner.set_transport_rolling_provider ([&rolling] () { return rolling; });

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION rolling.only\n"
		"WHEN transport rolling\n"
		"DO cue 0\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult stopped = runner.execute_slot (0, target);
	CPPUNIT_ASSERT_EQUAL (false, stopped.ok);
	CPPUNIT_ASSERT (stopped.error.find ("unmet condition") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());

	rolling = true;
	ReactiveExecutionResult live = runner.execute_slot (0, target);
	CPPUNIT_ASSERT_EQUAL (true, live.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:0"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::transportProviderControlsMidiConditions ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;
	bool rolling = true;

	runner.set_transport_rolling_provider ([&rolling] () { return rolling; });

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION stopped.pad\n"
		"TRIGGER midi note ch=10 note=36\n"
		"WHEN transport stopped\n"
		"DO cue 1\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult blocked = runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target);
	CPPUNIT_ASSERT_EQUAL (false, blocked.ok);
	CPPUNIT_ASSERT (blocked.error.find ("unmet condition") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());

	rolling = false;
	ReactiveExecutionResult stopped = runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target);
	CPPUNIT_ASSERT_EQUAL (true, stopped.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:1"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::transportProviderControlsPreviewConditions ()
{
	ReactiveActionSlotRunner runner;
	std::string error;
	bool rolling = false;

	runner.set_transport_rolling_provider ([&rolling] () { return rolling; });

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION rolling.pad\n"
		"TRIGGER midi note ch=10 note=36\n"
		"WHEN transport rolling\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	CPPUNIT_ASSERT_EQUAL (false, runner.preview_slot (0).available);
	CPPUNIT_ASSERT_EQUAL (false, runner.preview_midi_event (ReactiveMidiEvent::note_on (10, 36, 100)).available);

	rolling = true;
	CPPUNIT_ASSERT_EQUAL (true, runner.preview_slot (0).available);
	CPPUNIT_ASSERT_EQUAL (true, runner.preview_midi_event (ReactiveMidiEvent::note_on (10, 36, 100)).available);
}

void
ReactiveActionSlotRunnerTest::queueQuantizedSlotActionUntilDue ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION intro.drop\n"
		"TRIGGER midi note ch=10 note=36\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult queued = runner.execute_or_queue_slot (
		0,
		target,
		Temporal::BBT_Time (3, 2, 0),
		Temporal::BBT_Time (4, 1, 0));

	CPPUNIT_ASSERT_EQUAL (true, queued.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), queued.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), runner.last_action ());

	std::vector<ReactiveQueuedActionSummary> summary = runner.queued_action_summary (8, Temporal::BBT_Time (3, 3, 0));
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), summary[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("MIDI note ch=10 note=36"), summary[0].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|0|0"), summary[0].quantize);
	CPPUNIT_ASSERT_EQUAL (std::string ("3|2|0"), summary[0].requested_at);
	CPPUNIT_ASSERT_EQUAL (std::string ("4|1|0"), summary[0].due_at);
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary[0].command_count);
	CPPUNIT_ASSERT_EQUAL (false, summary[0].due);
	CPPUNIT_ASSERT (runner.format_queued_action_summary (8, Temporal::BBT_Time (3, 3, 0)).find ("Queued actions:") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_queued_action_summary (8, Temporal::BBT_Time (3, 3, 0)).find ("Commands:\n    - cue row 2") != std::string::npos);

	ReactiveActionSlotExecutionStatus status = runner.last_execution_status ();
	CPPUNIT_ASSERT_EQUAL (true, status.attempted);
	CPPUNIT_ASSERT_EQUAL (size_t (0), status.slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), status.action_name);
	CPPUNIT_ASSERT_EQUAL (true, status.result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), status.result.commands_executed);

	ReactiveExecutionResult early = runner.release_due_queued_actions (Temporal::BBT_Time (3, 3, 0), target);
	CPPUNIT_ASSERT_EQUAL (true, early.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), early.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());

	ReactiveExecutionResult due = runner.release_due_queued_actions (Temporal::BBT_Time (4, 1, 0), target);
	CPPUNIT_ASSERT_EQUAL (true, due.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), due.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:2"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.queued_action_count ());
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), runner.last_execution_status ().action_name);
	CPPUNIT_ASSERT_EQUAL (true, runner.last_execution_status ().result.ok);
}

void
ReactiveActionSlotRunnerTest::zeroQuantizeSlotActionExecutesImmediatelyThroughQueuePath ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION instant\n"
		"QUANTIZE 0|0|0\n"
		"DO cue 1\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult result = runner.execute_or_queue_slot (
		0,
		target,
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (2, 1, 0));

	CPPUNIT_ASSERT_EQUAL (true, result.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), result.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:1"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.queued_action_count ());
}

void
ReactiveActionSlotRunnerTest::queueQuantizedSlotActionUsingTempoMapClock ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	Temporal::TempoMap map = simple_tempo_map ();
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION intro.drop\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult queued = runner.execute_or_queue_slot (
		0,
		target,
		map,
		Temporal::BBT_Time (3, 2, 0));

	CPPUNIT_ASSERT_EQUAL (true, queued.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), queued.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());

	std::vector<ReactiveQueuedActionSummary> summary = runner.queued_action_summary (8, Temporal::BBT_Time (3, 2, 0));
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("3|2|0"), summary[0].requested_at);
	CPPUNIT_ASSERT_EQUAL (std::string ("4|1|0"), summary[0].due_at);

	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.release_due_queued_actions (Temporal::BBT_Time (3, 4, 0), target).commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());

	ReactiveExecutionResult due = runner.release_due_queued_actions (Temporal::BBT_Time (4, 1, 0), target);
	CPPUNIT_ASSERT_EQUAL (true, due.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), due.commands_executed);
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:2"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::queueQuantizedMidiActionPreservesControllerValue ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION knob.filter\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"QUANTIZE 0|1|0\n"
		"DO macro filter midi-value ramp 0|1|0\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult queued = runner.execute_or_queue_midi_event (
		ReactiveMidiEvent::control_change (1, 22, 64),
		target,
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (1, 2, 0));

	CPPUNIT_ASSERT_EQUAL (true, queued.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), queued.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, runner.macro_value ("filter"), 0.0001);

	ReactiveExecutionResult due = runner.release_due_queued_actions (Temporal::BBT_Time (1, 2, 0), target);
	CPPUNIT_ASSERT_EQUAL (true, due.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), due.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.503937:0|1|0"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::queueQuantizedMarkerActionUsingTempoMapClock ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	Temporal::TempoMap map = simple_tempo_map ();
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION drop.marker\n"
		"TRIGGER marker Drop\n"
		"QUANTIZE 0|1|0\n"
		"DO cue 7\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult queued = runner.execute_or_queue_marker_event (
		ReactiveMarkerEvent::named ("Drop"),
		target,
		map,
		Temporal::BBT_Time (1, 1, 120));

	CPPUNIT_ASSERT_EQUAL (true, queued.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), queued.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());

	std::vector<ReactiveQueuedActionSummary> summary = runner.queued_action_summary (8, Temporal::BBT_Time (1, 1, 120));
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("drop.marker"), summary[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("marker Drop"), summary[0].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|1|120"), summary[0].requested_at);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|2|0"), summary[0].due_at);

	ReactiveExecutionResult due = runner.release_due_queued_actions (Temporal::BBT_Time (1, 2, 0), target);
	CPPUNIT_ASSERT_EQUAL (true, due.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), due.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:7"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::queueQuantizedSceneActionUsingTempoMapClock ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	Temporal::TempoMap map = simple_tempo_map ();
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION drop.scene\n"
		"TRIGGER scene 3\n"
		"QUANTIZE 0|1|0\n"
		"DO cue 7\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult queued = runner.execute_or_queue_scene_event (
		ReactiveSceneEvent::numbered (3),
		target,
		map,
		Temporal::BBT_Time (1, 1, 120));

	CPPUNIT_ASSERT_EQUAL (true, queued.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), queued.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());

	std::vector<ReactiveQueuedActionSummary> summary = runner.queued_action_summary (8, Temporal::BBT_Time (1, 1, 120));
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("drop.scene"), summary[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("scene 3"), summary[0].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|1|120"), summary[0].requested_at);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|2|0"), summary[0].due_at);

	ReactiveExecutionResult due = runner.release_due_queued_actions (Temporal::BBT_Time (1, 2, 0), target);
	CPPUNIT_ASSERT_EQUAL (true, due.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), due.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:7"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::queueQuantizedRegionActionUsingTempoMapClock ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	Temporal::TempoMap map = simple_tempo_map ();
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION drop.region\n"
		"TRIGGER region Breakdown Loop\n"
		"QUANTIZE 0|1|0\n"
		"DO cue 7\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	ReactiveExecutionResult queued = runner.execute_or_queue_region_event (
		ReactiveRegionEvent::named ("Breakdown Loop"),
		target,
		map,
		Temporal::BBT_Time (1, 1, 120));

	CPPUNIT_ASSERT_EQUAL (true, queued.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), queued.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());

	std::vector<ReactiveQueuedActionSummary> summary = runner.queued_action_summary (8, Temporal::BBT_Time (1, 1, 120));
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), summary[0].slot);
	CPPUNIT_ASSERT_EQUAL (std::string ("drop.region"), summary[0].action_name);
	CPPUNIT_ASSERT_EQUAL (std::string ("region Breakdown Loop"), summary[0].primary_trigger);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|1|120"), summary[0].requested_at);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|2|0"), summary[0].due_at);

	ReactiveExecutionResult due = runner.release_due_queued_actions (Temporal::BBT_Time (1, 2, 0), target);
	CPPUNIT_ASSERT_EQUAL (true, due.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), due.commands_executed);
	CPPUNIT_ASSERT_EQUAL (size_t (1), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:7"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::clearAndLoadDocumentClearQueuedActions ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION queued\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 1\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_EQUAL (true, runner.execute_or_queue_slot (
		0,
		target,
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (2, 1, 0)).ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());

	runner.clear ();
	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.queued_action_count ());

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION replacement\n"
		"DO cue 2\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.queued_action_count ());
	CPPUNIT_ASSERT (runner.queued_action_summary (8, Temporal::BBT_Time (2, 1, 0)).empty ());
}

void
ReactiveActionSlotRunnerTest::queueQuantizedMidiBytesUsingTempoMapClock ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	Temporal::TempoMap map = simple_tempo_map ();
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION knob.filter\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"QUANTIZE 0|1|0\n"
		"DO macro filter midi-value ramp 0|1|0\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	unsigned char const message[] = { 0xb0, 22, 64 };
	ReactiveExecutionResult queued = runner.execute_or_queue_midi_bytes (
		message,
		sizeof (message),
		target,
		map,
		Temporal::BBT_Time (1, 1, 120));

	CPPUNIT_ASSERT_EQUAL (true, queued.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (0), queued.commands_executed);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.queued_action_count ());

	std::vector<ReactiveQueuedActionSummary> summary = runner.queued_action_summary (8, Temporal::BBT_Time (1, 1, 120));
	CPPUNIT_ASSERT_EQUAL (size_t (1), summary.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("1|1|120"), summary[0].requested_at);
	CPPUNIT_ASSERT_EQUAL (std::string ("1|2|0"), summary[0].due_at);

	ReactiveExecutionResult due = runner.release_due_queued_actions (Temporal::BBT_Time (1, 2, 0), target);
	CPPUNIT_ASSERT_EQUAL (true, due.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), due.commands_executed);
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.503937:0|1|0"), target.calls[0]);
}

void
ReactiveActionSlotRunnerTest::disabledPerformanceModeDoesNotQueueActions ()
{
	ReactiveActionSlotRunner runner;
	RecordingTarget target;
	std::string error;

	CPPUNIT_ASSERT_EQUAL (true, runner.load_source (
		"ACTION disabled.queue\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 3\n"
		"END\n",
		error));
	CPPUNIT_ASSERT (error.empty ());

	runner.set_performance_enabled (false);
	ReactiveExecutionResult result = runner.execute_or_queue_slot (
		0,
		target,
		Temporal::BBT_Time (1, 1, 0),
		Temporal::BBT_Time (2, 1, 0));

	CPPUNIT_ASSERT_EQUAL (false, result.ok);
	CPPUNIT_ASSERT (result.error.find ("disabled") != std::string::npos);
	CPPUNIT_ASSERT (target.calls.empty ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), runner.queued_action_count ());
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().result.ok);
	CPPUNIT_ASSERT (runner.last_execution_status ().result.error.find ("disabled") != std::string::npos);
}
