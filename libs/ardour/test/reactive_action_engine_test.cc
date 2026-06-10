#include "reactive_action_engine_test.h"

#include "ardour/reactive_action.h"
#include "ardour/reactive_action_engine.h"

#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionEngineTest);

using namespace ARDOUR;

static ReactiveActionEngine
engine_from_source (char const* src)
{
	ReactiveActionParseResult parsed = ReactiveActionDocument::parse (src);
	CPPUNIT_ASSERT_EQUAL (true, parsed.ok);

	ReactiveActionEngine engine;
	std::string error;
	CPPUNIT_ASSERT_EQUAL (true, engine.load_document (parsed.document, error));
	return engine;
}

void
ReactiveActionEngineTest::matchMidiNoteTrigger ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"QUANTIZE 1|0|0\n"
		"CHAIN sequential\n"
		"DO cue 0\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 100));

	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT (matches[0].action != 0);
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT (ReactiveChainMode::Sequential == matches[0].chain_mode);
	CPPUNIT_ASSERT_EQUAL (1, matches[0].quantize.bars);
}

void
ReactiveActionEngineTest::mapMidiNoteBytesToTriggerEvent ()
{
	ReactiveMidiEvent event;
	unsigned char const bytes[] = { 0x99, 36, 100 };

	CPPUNIT_ASSERT_EQUAL (true, ReactiveMidiEvent::from_midi_bytes (bytes, 3, event));
	CPPUNIT_ASSERT_EQUAL (ReactiveMidiEvent::NoteOn, event.type);
	CPPUNIT_ASSERT_EQUAL (10, event.channel);
	CPPUNIT_ASSERT_EQUAL (36, event.number);
	CPPUNIT_ASSERT_EQUAL (100, event.value);

	ReactiveActionEngine engine = engine_from_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (event);
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("pad.one"), matches[0].action->name);
}

void
ReactiveActionEngineTest::mapMidiCCBytesToTriggerEvent ()
{
	ReactiveMidiEvent event;
	unsigned char const bytes[] = { 0xb0, 22, 64 };

	CPPUNIT_ASSERT_EQUAL (true, ReactiveMidiEvent::from_midi_bytes (bytes, 3, event));
	CPPUNIT_ASSERT_EQUAL (ReactiveMidiEvent::ControlChange, event.type);
	CPPUNIT_ASSERT_EQUAL (1, event.channel);
	CPPUNIT_ASSERT_EQUAL (22, event.number);
	CPPUNIT_ASSERT_EQUAL (64, event.value);

	ReactiveActionEngine engine = engine_from_source (
		"ACTION knob.high\n"
		"TRIGGER midi cc ch=1 cc=22 value>63\n"
		"DO macro filter 0.80\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (event);
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.high"), matches[0].action->name);
}

void
ReactiveActionEngineTest::ignoreUnsupportedMidiBytes ()
{
	ReactiveMidiEvent event;
	unsigned char const note_off[] = { 0x89, 36, 0 };
	unsigned char const zero_velocity_note_on[] = { 0x99, 36, 0 };
	unsigned char const pitch_bend[] = { 0xe0, 0, 64 };
	unsigned char const short_message[] = { 0x99, 36 };

	CPPUNIT_ASSERT_EQUAL (false, ReactiveMidiEvent::from_midi_bytes (note_off, 3, event));
	CPPUNIT_ASSERT_EQUAL (false, ReactiveMidiEvent::from_midi_bytes (zero_velocity_note_on, 3, event));
	CPPUNIT_ASSERT_EQUAL (false, ReactiveMidiEvent::from_midi_bytes (pitch_bend, 3, event));
	CPPUNIT_ASSERT_EQUAL (false, ReactiveMidiEvent::from_midi_bytes (short_message, 2, event));
	CPPUNIT_ASSERT_EQUAL (false, ReactiveMidiEvent::from_midi_bytes (0, 3, event));
}

void
ReactiveActionEngineTest::ignoreMismatchedMidiNote ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (9, 36, 100)).empty ());
	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (10, 37, 100)).empty ());
}

void
ReactiveActionEngineTest::ignoreZeroVelocityNoteOn ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 0)).empty ());
}

void
ReactiveActionEngineTest::matchMidiCCThreshold ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION knob.high\n"
		"TRIGGER midi cc ch=1 cc=22 value>63\n"
		"DO macro filter 0.80 ramp 0|1|0\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::control_change (1, 22, 63)).empty ());

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (ReactiveMidiEvent::control_change (1, 22, 64));
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.high"), matches[0].action->name);
}

void
ReactiveActionEngineTest::matchMidiCCWithoutThreshold ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION knob.any\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO macro filter 0.80\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (ReactiveMidiEvent::control_change (1, 22, 0));
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.any"), matches[0].action->name);
}

void
ReactiveActionEngineTest::ignoreMarkerTriggersForMidi ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION breakdown\n"
		"TRIGGER marker Breakdown\n"
		"DO transport stop\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 100)).empty ());
	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::control_change (1, 22, 127)).empty ());
}

void
ReactiveActionEngineTest::matchMarkerTriggerByName ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION breakdown\n"
		"TRIGGER marker Breakdown\n"
		"QUANTIZE 0|1|0\n"
		"CHAIN sequential\n"
		"DO transport stop\n"
		"END\n"
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 1\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_marker_event (ReactiveMarkerEvent::named ("Breakdown"));

	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT (matches[0].action != 0);
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT (ReactiveChainMode::Sequential == matches[0].chain_mode);
	CPPUNIT_ASSERT_EQUAL (1, matches[0].quantize.beats);
	CPPUNIT_ASSERT (engine.match_marker_event (ReactiveMarkerEvent::named ("Drop")).empty ());
}

void
ReactiveActionEngineTest::keepDocumentOrderForMultipleMarkerMatches ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION first\n"
		"TRIGGER marker Breakdown\n"
		"DO cue 0\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER marker Breakdown\n"
		"DO cue 1\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_marker_event (ReactiveMarkerEvent::named ("Breakdown"));

	CPPUNIT_ASSERT_EQUAL (size_t (2), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (std::string ("second"), matches[1].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches[1].action_index);
}

void
ReactiveActionEngineTest::ignoreSceneTriggersForMidiAndMarker ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION scene.drop\n"
		"TRIGGER scene 3\n"
		"DO cue 3\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 100)).empty ());
	CPPUNIT_ASSERT (engine.match_marker_event (ReactiveMarkerEvent::named ("Drop")).empty ());
}

void
ReactiveActionEngineTest::matchSceneTriggerByIndex ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION scene.drop\n"
		"TRIGGER scene 3\n"
		"QUANTIZE 0|1|0\n"
		"CHAIN sequential\n"
		"DO cue 3\n"
		"END\n"
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 1\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_scene_event (ReactiveSceneEvent::numbered (3));

	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT (matches[0].action != 0);
	CPPUNIT_ASSERT_EQUAL (std::string ("scene.drop"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT (ReactiveChainMode::Sequential == matches[0].chain_mode);
	CPPUNIT_ASSERT_EQUAL (1, matches[0].quantize.beats);
	CPPUNIT_ASSERT (engine.match_scene_event (ReactiveSceneEvent::numbered (4)).empty ());
}

void
ReactiveActionEngineTest::keepDocumentOrderForMultipleSceneMatches ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION first\n"
		"TRIGGER scene 2\n"
		"DO cue 0\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER scene 2\n"
		"DO cue 1\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_scene_event (ReactiveSceneEvent::numbered (2));

	CPPUNIT_ASSERT_EQUAL (size_t (2), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (std::string ("second"), matches[1].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches[1].action_index);
}

void
ReactiveActionEngineTest::ignoreRegionTriggersForOtherFamilies ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION region.drop\n"
		"TRIGGER region Breakdown Loop\n"
		"DO cue 3\n"
		"END\n");

	CPPUNIT_ASSERT (engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 100)).empty ());
	CPPUNIT_ASSERT (engine.match_marker_event (ReactiveMarkerEvent::named ("Breakdown Loop")).empty ());
	CPPUNIT_ASSERT (engine.match_scene_event (ReactiveSceneEvent::numbered (3)).empty ());
}

void
ReactiveActionEngineTest::matchRegionTriggerByName ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION region.drop\n"
		"TRIGGER region Breakdown Loop\n"
		"QUANTIZE 0|1|0\n"
		"CHAIN sequential\n"
		"DO cue 3\n"
		"END\n"
		"ACTION pad.one\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 1\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_region_event (ReactiveRegionEvent::named ("Breakdown Loop"));

	CPPUNIT_ASSERT_EQUAL (size_t (1), matches.size ());
	CPPUNIT_ASSERT (matches[0].action != 0);
	CPPUNIT_ASSERT_EQUAL (std::string ("region.drop"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT (ReactiveChainMode::Sequential == matches[0].chain_mode);
	CPPUNIT_ASSERT_EQUAL (1, matches[0].quantize.beats);
	CPPUNIT_ASSERT (engine.match_region_event (ReactiveRegionEvent::named ("Verse Loop")).empty ());
}

void
ReactiveActionEngineTest::keepDocumentOrderForMultipleRegionMatches ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION first\n"
		"TRIGGER region Breakdown Loop\n"
		"DO cue 0\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER region Breakdown Loop\n"
		"DO cue 1\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_region_event (ReactiveRegionEvent::named ("Breakdown Loop"));

	CPPUNIT_ASSERT_EQUAL (size_t (2), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (std::string ("second"), matches[1].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches[1].action_index);
}

void
ReactiveActionEngineTest::keepDocumentOrderForMultipleMatches ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION first\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 0\n"
		"END\n"
		"ACTION second\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO cue 1\n"
		"END\n");

	std::vector<ReactiveActionMatch> matches = engine.match_midi_event (ReactiveMidiEvent::note_on (10, 36, 127));

	CPPUNIT_ASSERT_EQUAL (size_t (2), matches.size ());
	CPPUNIT_ASSERT_EQUAL (std::string ("first"), matches[0].action->name);
	CPPUNIT_ASSERT_EQUAL (std::string ("second"), matches[1].action->name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), matches[0].action_index);
	CPPUNIT_ASSERT_EQUAL (size_t (1), matches[1].action_index);
}

void
ReactiveActionEngineTest::triggerAllChainReturnsAllCommandsAndMetadata ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION intro.drop\n"
		"TRIGGER midi note ch=10 note=36\n"
		"QUANTIZE 1|0|0\n"
		"DO cue 0\n"
		"DO macro filter 0.72 ramp 0|2|0\n"
		"DO scene apply 0\n"
		"END\n");

	ReactiveActionPlan plan = engine.trigger_action ("intro.drop");

	CPPUNIT_ASSERT_EQUAL (true, plan.ok);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), plan.action_name);
	CPPUNIT_ASSERT_EQUAL (size_t (0), plan.action_index);
	CPPUNIT_ASSERT (ReactiveChainMode::All == plan.chain_mode);
	CPPUNIT_ASSERT_EQUAL (1, plan.quantize.bars);
	CPPUNIT_ASSERT_EQUAL (size_t (3), plan.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Cue, plan.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, plan.commands[1].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::SceneApply, plan.commands[2].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("intro.drop"), engine.last_action ());
}

void
ReactiveActionEngineTest::triggerActionWithMidiEventResolvesMacroValue ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION knob.live\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO macro filter midi-value ramp 0|1|0\n"
		"END\n");
	ReactiveMidiEvent event = ReactiveMidiEvent::control_change (1, 22, 64);

	ReactiveActionPlan plan = engine.trigger_action ("knob.live", &event);

	CPPUNIT_ASSERT_EQUAL (true, plan.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), plan.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, plan.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::MidiEventValue, plan.commands[0].value_source);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, plan.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (1, plan.commands[0].ramp.beats);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.live"), engine.last_action ());

	ReactiveActionEngine pad_engine = engine_from_source (
		"ACTION pad.live\n"
		"TRIGGER midi note ch=10 note=36\n"
		"DO macro accent midi-value\n"
		"END\n");
	ReactiveMidiEvent note = ReactiveMidiEvent::note_on (10, 36, 100);
	ReactiveActionPlan note_plan = pad_engine.trigger_action ("pad.live", &note);

	CPPUNIT_ASSERT_EQUAL (true, note_plan.ok);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, note_plan.commands[0].type);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (100.0 / 127.0, note_plan.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (100.0 / 127.0, pad_engine.macro_value ("accent"), 0.0001);
}

void
ReactiveActionEngineTest::triggerActionWithMidiEventResolvesRhythmRouteValue ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION knob.live.rhythm\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO rhythm route 0 density midi-value\n"
		"END\n");
	ReactiveMidiEvent event = ReactiveMidiEvent::control_change (1, 22, 64);

	ReactiveActionPlan plan = engine.trigger_action ("knob.live.rhythm", &event);

	CPPUNIT_ASSERT_EQUAL (true, plan.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), plan.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::RhythmRoute, plan.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (0, plan.commands[0].first);
	CPPUNIT_ASSERT_EQUAL (std::string ("density"), plan.commands[0].name);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::MidiEventValue, plan.commands[0].value_source);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, plan.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.live.rhythm"), engine.last_action ());
}

void
ReactiveActionEngineTest::triggerActionWithMidiEventResolvesTriggerProbabilityValue ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION knob.live.clip\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO trigger probability 0 1 midi-value\n"
		"END\n");
	ReactiveMidiEvent event = ReactiveMidiEvent::control_change (1, 22, 64);

	ReactiveActionPlan plan = engine.trigger_action ("knob.live.clip", &event);

	CPPUNIT_ASSERT_EQUAL (true, plan.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), plan.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::TriggerProbability, plan.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (0, plan.commands[0].first);
	CPPUNIT_ASSERT_EQUAL (1, plan.commands[0].second);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::MidiEventValue, plan.commands[0].value_source);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, plan.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("knob.live.clip"), engine.last_action ());
}

void
ReactiveActionEngineTest::rotateSequentialChainCommands ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION drums.mutate\n"
		"CHAIN sequential\n"
		"DO rhythm density 0.35\n"
		"DO rhythm density 0.55\n"
		"DO rhythm density 0.80\n"
		"END\n");

	ReactiveActionPlan first = engine.trigger_action ("drums.mutate");
	ReactiveActionPlan second = engine.trigger_action ("drums.mutate");
	ReactiveActionPlan third = engine.trigger_action ("drums.mutate");
	ReactiveActionPlan wrapped = engine.trigger_action ("drums.mutate");

	CPPUNIT_ASSERT_EQUAL (true, first.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), first.commands.size ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.35, first.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.55, second.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, third.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.35, wrapped.commands[0].value, 0.0001);
}

void
ReactiveActionEngineTest::rejectUnknownActionWithoutChangingLastAction ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION known\n"
		"DO cue 0\n"
		"END\n");

	ReactiveActionPlan known = engine.trigger_action ("known");
	ReactiveActionPlan missing = engine.trigger_action ("missing");

	CPPUNIT_ASSERT_EQUAL (true, known.ok);
	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("unknown action") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (std::string ("known"), engine.last_action ());
}

void
ReactiveActionEngineTest::trackMacroAndStateValuesFromSelectedCommands ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION performance.mode\n"
		"DO macro filter 0.80\n"
		"DO state section breakdown\n"
		"END\n");

	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string (), engine.state_value ("section"));

	ReactiveActionPlan plan = engine.trigger_action ("performance.mode");

	CPPUNIT_ASSERT_EQUAL (true, plan.ok);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("breakdown"), engine.state_value ("section"));
}

void
ReactiveActionEngineTest::trackHarmonyValuesAndConditions ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION set.key\n"
		"DO harmony key C_minor\n"
		"END\n"
		"ACTION gated.chord\n"
		"WHEN harmony key C_minor\n"
		"DO harmony chord i\n"
		"END\n"
		"ACTION blocked.chord\n"
		"WHEN harmony key D_minor\n"
		"DO harmony chord iv\n"
		"END\n");

	CPPUNIT_ASSERT_EQUAL (std::string (), engine.harmony_value ("key"));
	CPPUNIT_ASSERT_EQUAL (std::string (), engine.harmony_value ("chord"));

	ReactiveActionPlan preview = engine.preview_action ("set.key");
	CPPUNIT_ASSERT_EQUAL (true, preview.ok);
	CPPUNIT_ASSERT_EQUAL (std::string (), engine.harmony_value ("key"));

	ReactiveActionPlan set = engine.trigger_action ("set.key");
	CPPUNIT_ASSERT_EQUAL (true, set.ok);
	CPPUNIT_ASSERT_EQUAL (std::string ("C_minor"), engine.harmony_value ("key"));

	ReactiveActionPlan gated = engine.trigger_action ("gated.chord");
	CPPUNIT_ASSERT_EQUAL (true, gated.ok);
	CPPUNIT_ASSERT_EQUAL (std::string ("i"), engine.harmony_value ("chord"));

	ReactiveActionPlan blocked = engine.trigger_action ("blocked.chord");
	CPPUNIT_ASSERT_EQUAL (false, blocked.ok);
	CPPUNIT_ASSERT (blocked.error.find ("unmet condition") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (std::string ("i"), engine.harmony_value ("chord"));
	CPPUNIT_ASSERT_EQUAL (std::string ("gated.chord"), engine.last_action ());
}

void
ReactiveActionEngineTest::storeAndRecallMacroSnapshotValues ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION store.verse\n"
		"DO macro filter 0.25\n"
		"DO macro resonance 0.70\n"
		"DO macro snapshot store verse\n"
		"END\n"
		"ACTION move.away\n"
		"DO macro filter 0.90\n"
		"DO macro resonance 0.10\n"
		"END\n"
		"ACTION recall.verse\n"
		"DO macro snapshot recall verse ramp 0|2|0\n"
		"END\n");

	ReactiveActionPlan stored = engine.trigger_action ("store.verse");
	ReactiveActionPlan moved = engine.trigger_action ("move.away");
	ReactiveActionPlan recalled = engine.trigger_action ("recall.verse");

	CPPUNIT_ASSERT_EQUAL (true, stored.ok);
	CPPUNIT_ASSERT_EQUAL (true, moved.ok);
	CPPUNIT_ASSERT_EQUAL (true, recalled.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (2), recalled.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, recalled.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), recalled.commands[0].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.25, recalled.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (2, recalled.commands[0].ramp.beats);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, recalled.commands[1].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("resonance"), recalled.commands[1].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.70, recalled.commands[1].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (2, recalled.commands[1].ramp.beats);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.25, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.70, engine.macro_value ("resonance"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("recall.verse"), engine.last_action ());
}

void
ReactiveActionEngineTest::rejectMissingMacroSnapshotRecallWithoutChangingLastAction ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION set.filter\n"
		"DO macro filter 0.80\n"
		"END\n"
		"ACTION recall.missing\n"
		"DO macro filter 0.20\n"
		"DO macro snapshot recall missing\n"
		"END\n");

	ReactiveActionPlan set = engine.trigger_action ("set.filter");
	ReactiveActionPlan missing = engine.trigger_action ("recall.missing");

	CPPUNIT_ASSERT_EQUAL (true, set.ok);
	CPPUNIT_ASSERT_EQUAL (false, missing.ok);
	CPPUNIT_ASSERT (missing.error.find ("unknown macro snapshot") != std::string::npos);
	CPPUNIT_ASSERT (missing.commands.empty ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("set.filter"), engine.last_action ());
}

void
ReactiveActionEngineTest::morphBetweenMacroSnapshots ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION store.verse\n"
		"DO macro filter 0.20\n"
		"DO macro resonance 0.80\n"
		"DO macro snapshot store verse\n"
		"END\n"
		"ACTION store.chorus\n"
		"DO macro filter 0.80\n"
		"DO macro resonance 0.20\n"
		"DO macro snapshot store chorus\n"
		"END\n"
		"ACTION morph.half\n"
		"DO macro morph verse chorus amount 0.50 ramp 0|2|0\n"
		"END\n");

	CPPUNIT_ASSERT_EQUAL (true, engine.trigger_action ("store.verse").ok);
	CPPUNIT_ASSERT_EQUAL (true, engine.trigger_action ("store.chorus").ok);
	ReactiveActionPlan morphed = engine.trigger_action ("morph.half");

	CPPUNIT_ASSERT_EQUAL (true, morphed.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (2), morphed.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, morphed.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), morphed.commands[0].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.50, morphed.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (2, morphed.commands[0].ramp.beats);
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, morphed.commands[1].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("resonance"), morphed.commands[1].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.50, morphed.commands[1].value, 0.0001);
	CPPUNIT_ASSERT_EQUAL (2, morphed.commands[1].ramp.beats);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.50, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.50, engine.macro_value ("resonance"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("morph.half"), engine.last_action ());
}

void
ReactiveActionEngineTest::morphBetweenMacroSnapshotsWithMidiValue ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION store.a\n"
		"DO macro filter 0.00\n"
		"DO macro snapshot store a\n"
		"END\n"
		"ACTION store.b\n"
		"DO macro filter 1.00\n"
		"DO macro snapshot store b\n"
		"END\n"
		"ACTION morph.live\n"
		"TRIGGER midi cc ch=1 cc=22\n"
		"DO macro morph a b amount midi-value\n"
		"END\n");

	CPPUNIT_ASSERT_EQUAL (true, engine.trigger_action ("store.a").ok);
	CPPUNIT_ASSERT_EQUAL (true, engine.trigger_action ("store.b").ok);
	ReactiveMidiEvent event = ReactiveMidiEvent::control_change (1, 22, 64);
	ReactiveActionPlan morphed = engine.trigger_action ("morph.live", &event);

	CPPUNIT_ASSERT_EQUAL (true, morphed.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), morphed.commands.size ());
	CPPUNIT_ASSERT_EQUAL (ReactiveCommand::Macro, morphed.commands[0].type);
	CPPUNIT_ASSERT_EQUAL (std::string ("filter"), morphed.commands[0].name);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, morphed.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (64.0 / 127.0, engine.macro_value ("filter"), 0.0001);
}

void
ReactiveActionEngineTest::rejectMacroMorphWithoutSharedSnapshotValues ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION store.empty\n"
		"DO macro snapshot store empty\n"
		"END\n"
		"ACTION store.b\n"
		"DO macro resonance 0.80\n"
		"DO macro snapshot store b\n"
		"END\n"
		"ACTION morph.empty\n"
		"DO macro filter 0.90\n"
		"DO macro morph empty b amount 0.50\n"
		"END\n");

	CPPUNIT_ASSERT_EQUAL (true, engine.trigger_action ("store.empty").ok);
	CPPUNIT_ASSERT_EQUAL (true, engine.trigger_action ("store.b").ok);
	ReactiveActionPlan morphed = engine.trigger_action ("morph.empty");

	CPPUNIT_ASSERT_EQUAL (false, morphed.ok);
	CPPUNIT_ASSERT (morphed.error.find ("no shared macro values") != std::string::npos);
	CPPUNIT_ASSERT (morphed.commands.empty ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.80, engine.macro_value ("resonance"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("store.b"), engine.last_action ());
}

void
ReactiveActionEngineTest::blockUnmetStateConditionWithoutMutatingState ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION set.section\n"
		"DO state section breakdown\n"
		"END\n"
		"ACTION gated\n"
		"WHEN state section breakdown\n"
		"DO macro filter 0.70\n"
		"END\n"
		"ACTION blocked\n"
		"WHEN state section drop\n"
		"DO macro filter 0.20\n"
		"END\n");

	ReactiveActionPlan preview = engine.preview_action ("gated");
	ReactiveActionPlan blocked_before = engine.trigger_action ("gated");

	CPPUNIT_ASSERT_EQUAL (false, preview.ok);
	CPPUNIT_ASSERT (preview.error.find ("condition") != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (false, blocked_before.ok);
	CPPUNIT_ASSERT (blocked_before.error.find ("condition") != std::string::npos);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.0, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string (), engine.last_action ());

	ReactiveActionPlan set = engine.trigger_action ("set.section");
	ReactiveActionPlan gated = engine.trigger_action ("gated");

	CPPUNIT_ASSERT_EQUAL (true, set.ok);
	CPPUNIT_ASSERT_EQUAL (true, gated.ok);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.70, engine.macro_value ("filter"), 0.0001);

	ReactiveActionPlan blocked_after = engine.trigger_action ("blocked");

	CPPUNIT_ASSERT_EQUAL (false, blocked_after.ok);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.70, engine.macro_value ("filter"), 0.0001);
	CPPUNIT_ASSERT_EQUAL (std::string ("gated"), engine.last_action ());
}

void
ReactiveActionEngineTest::matchMacroConditionAfterMacroChanges ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION set.filter\n"
		"DO macro filter 0.75\n"
		"END\n"
		"ACTION gated\n"
		"WHEN macro filter 0.75\n"
		"DO state section drop\n"
		"END\n");

	ReactiveActionPlan blocked = engine.trigger_action ("gated");

	CPPUNIT_ASSERT_EQUAL (false, blocked.ok);
	CPPUNIT_ASSERT_EQUAL (std::string (), engine.state_value ("section"));

	ReactiveActionPlan set = engine.trigger_action ("set.filter");
	ReactiveActionPlan gated = engine.trigger_action ("gated");

	CPPUNIT_ASSERT_EQUAL (true, set.ok);
	CPPUNIT_ASSERT_EQUAL (true, gated.ok);
	CPPUNIT_ASSERT_EQUAL (std::string ("drop"), engine.state_value ("section"));
}

void
ReactiveActionEngineTest::blockTransportConditionWithoutAdvancingSequentialChain ()
{
	ReactiveActionEngine engine = engine_from_source (
		"ACTION gated.seq\n"
		"CHAIN sequential\n"
		"WHEN transport rolling\n"
		"DO macro step 0.10\n"
		"DO macro step 0.20\n"
		"END\n");

	ReactiveActionPlan first_blocked = engine.trigger_action ("gated.seq");
	ReactiveActionPlan second_blocked = engine.trigger_action ("gated.seq");
	engine.set_transport_rolling (true);
	ReactiveActionPlan first_allowed = engine.trigger_action ("gated.seq");

	CPPUNIT_ASSERT_EQUAL (false, first_blocked.ok);
	CPPUNIT_ASSERT_EQUAL (false, second_blocked.ok);
	CPPUNIT_ASSERT_EQUAL (true, first_allowed.ok);
	CPPUNIT_ASSERT_EQUAL (size_t (1), first_allowed.commands.size ());
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.10, first_allowed.commands[0].value, 0.0001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (0.10, engine.macro_value ("step"), 0.0001);
}
