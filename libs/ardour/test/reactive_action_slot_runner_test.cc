#include "reactive_action_slot_runner_test.h"

#include "ardour/reactive_action_document_loader.h"
#include "ardour/reactive_action_slot_runner.h"

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
		"DO macro filter 0.80\n"
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
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, runner.macro_value ("filter"), 0.0001);

	CPPUNIT_ASSERT_EQUAL (size_t (2), target.calls.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("cue:0"), target.calls[0]);
	CPPUNIT_ASSERT_EQUAL (std::string ("macro:filter:0.8:0|0|0"), target.calls[1]);
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
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_count);
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

	preview = runner.preview_slot (0);
	CPPUNIT_ASSERT_EQUAL (true, preview.available);
	CPPUNIT_ASSERT_EQUAL (size_t (1), preview.command_count);

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
	CPPUNIT_ASSERT_EQUAL (std::string (), runner.state_value ("section"));
	CPPUNIT_ASSERT_EQUAL (false, runner.last_execution_status ().attempted);

	ReactiveActionPreviewSummary missing = runner.preview_midi_event (ReactiveMidiEvent::control_change (1, 22, 64));
	CPPUNIT_ASSERT_EQUAL (false, missing.available);

	CPPUNIT_ASSERT_EQUAL (true, runner.execute_midi_event (ReactiveMidiEvent::note_on (10, 36, 100), target).ok);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("Next action preview: slot 0 (first)") != std::string::npos);
	CPPUNIT_ASSERT (runner.format_next_action_preview ().find ("MIDI note ch=10 note=36") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro"), runner.state_value ("section"));
}
