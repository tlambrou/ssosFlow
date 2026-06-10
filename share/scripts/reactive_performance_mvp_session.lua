ardour {
	["type"]    = "SessionInit",
	name        = "Reactive Performance MVP",
	category    = "Reactive Performance",
	description = [[Create a small MIDI-route layout for the Reactive Performance Mode MVP demo.]]
}

local demo_action_document = [[ACTION demo.reset
TRIGGER midi note ch=10 note=36
DO rhythm insert 0
DO rhythm route 0 density 1
DO rhythm route 0 chance 1
DO rhythm route 0 priority_mode 0
DO rhythm route 0 rotation 0
DO harmony key C_minor
DO harmony scale aeolian
DO harmony chord i
DO state groove reset
DO cue 0
END

ACTION demo.tighten
TRIGGER midi note ch=10 note=37
DO rhythm insert 0
DO rhythm route 0 density 0.75
DO rhythm route 0 chance 1
DO rhythm route 0 priority_mode 3
DO harmony chord iv
DO state groove tight
DO cue 1
END

ACTION demo.sparse
TRIGGER midi note ch=10 note=38
DO rhythm insert 0
DO rhythm route 0 density 0.35
DO rhythm route 0 chance 0.5
DO rhythm route 0 priority_mode 1
DO harmony chord VI
DO state groove sparse
DO cue 2
END

ACTION demo.rotate
TRIGGER midi note ch=10 note=39
DO rhythm insert 0
DO rhythm route 0 rotation 4
DO harmony chord V
DO state groove rotated
DO cue 3
END

ACTION demo.play.when.stopped
TRIGGER midi note ch=10 note=40
WHEN transport stopped
DO state transport stopped
DO transport play
END

ACTION demo.cue.when.rolling
TRIGGER midi note ch=10 note=41
TRIGGER midi note ch=10 note=46
WHEN transport rolling
QUANTIZE 1|0|0
DO state transport rolling
DO cue 4
END

ACTION demo.stop.when.rolling
TRIGGER midi note ch=10 note=42
WHEN transport rolling
DO state transport stopping
DO transport stop
END

ACTION demo.mark.stopped
TRIGGER midi note ch=10 note=43
WHEN transport stopped
DO state transport stopped-ready
END

ACTION demo.filter.sweep
TRIGGER midi cc ch=1 cc=22
DO rhythm insert 0
DO rhythm route 0 density midi-value
DO macro filter midi-value ramp 0|1|0
DO macro texture 0.20
DO macro space 0.80
DO macro snapshot store texture_low
DO macro texture 0.80
DO macro space 0.25
DO macro snapshot store texture_high
DO macro morph texture_low texture_high amount midi-value ramp 0|1|0
DO state macro filter
END

ACTION demo.marker.breakdown
TRIGGER marker Breakdown
QUANTIZE 1|0|0
DO rhythm insert 0
DO rhythm route 0 density 0.25
DO rhythm route 0 chance 0.35
DO macro filter 0.25 ramp 0|1|0
DO harmony chord bVII
DO state section breakdown
DO cue 2
END

ACTION demo.region.breakdown.loop
TRIGGER region Breakdown Loop
QUANTIZE 0|1|0
DO rhythm insert 0
DO rhythm route 0 density 0.55
DO rhythm route 0 chance 0.75
DO macro filter 0.55 ramp 0|1|0
DO harmony chord i7
DO state section region-breakdown
DO cue 3
END

ACTION demo.scene.drop
TRIGGER scene 3
QUANTIZE 1|0|0
DO rhythm insert 0
DO rhythm route 0 density 0.80
DO rhythm route 0 chance 0.90
DO rhythm route 0 rotation 2
DO macro filter 0.70 ramp 0|1|0
DO harmony chord V7
DO state section scene-drop
END
]]

local function install_demo_action_document ()
	if not io or not io.open then
		print ("Reactive Performance MVP: file I/O unavailable, skipping reactive-actions.txt install")
		return
	end

	local path = ARDOUR.LuaAPI.build_filename (Session:path (), "reactive-actions.txt")
	local existing = io.open (path, "r")
	if existing then
		existing:close ()
		return
	end

	local file = io.open (path, "w")
	if not file then
		print ("Reactive Performance MVP: could not write " .. path)
		return
	end

	file:write (demo_action_document)
	file:close ()
end

local function seed_demo_markers ()
	local sample_rate = 48000
	local session_sample_rate = Session:nominal_sample_rate ()
	if session_sample_rate and session_sample_rate > 0 then
		sample_rate = session_sample_rate
	end

	ARDOUR.LuaAPI.ensure_session_marker (Session, "Breakdown", Temporal.timepos_t (sample_rate * 16))
end

local function seed_demo_regions ()
	local sample_rate = 48000
	local session_sample_rate = Session:nominal_sample_rate ()
	if session_sample_rate and session_sample_rate > 0 then
		sample_rate = session_sample_rate
	end

	ARDOUR.LuaAPI.ensure_session_midi_region (
		Session,
		"Reactive Rhythm Lane",
		"Breakdown Loop",
		Temporal.timepos_t (sample_rate * 24),
		Temporal.timecnt_t (sample_rate * 4))
end

local function seed_demo_trigger_clips ()
	local sample_rate = 48000
	local session_sample_rate = Session:nominal_sample_rate ()
	if session_sample_rate and session_sample_rate > 0 then
		sample_rate = session_sample_rate
	end

	local clip_length = Temporal.timecnt_t (sample_rate * 4)
	local note_start = Temporal.Beats.from_double (0)
	local note_length = Temporal.Beats.from_double (1)

	ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, "Reactive Rhythm Lane", 0, "Reactive Cue 0 Reset", clip_length, note_start, note_length, 0, 36, 100)
	ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, "Reactive Rhythm Lane", 1, "Reactive Cue 1 Tighten", clip_length, note_start, note_length, 0, 38, 100)
	ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, "Reactive Rhythm Lane", 2, "Reactive Cue 2 Sparse", clip_length, note_start, note_length, 0, 41, 100)
	ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, "Reactive Harmony Lane", 0, "Reactive Harmony i", clip_length, note_start, note_length, 0, 48, 96)
	ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, "Reactive Macro Lane", 0, "Reactive Macro Filter", clip_length, note_start, note_length, 0, 60, 88)
	ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, "Reactive Macro Lane", 1, "Reactive Macro Texture", clip_length, note_start, note_length, 0, 67, 88)
end

function factory () return function ()
	local group = ARDOUR.RouteGroup ()
	local input = ARDOUR.ChanCount (ARDOUR.DataType ("midi"), 1)
	local output = ARDOUR.ChanCount (ARDOUR.DataType ("midi"), 1)
	local instrument = ARDOUR.PluginInfo ()

	Session:new_midi_track (
		input,
		output,
		false,
		instrument,
		nil,
		group,
		1,
		"Reactive Rhythm Lane",
		ARDOUR.PresentationInfo.max_order,
		ARDOUR.TrackMode.Normal,
		false,
		true)

	Session:new_midi_track (
		input,
		output,
		false,
		instrument,
		nil,
		group,
		1,
		"Reactive Harmony Lane",
		ARDOUR.PresentationInfo.max_order,
		ARDOUR.TrackMode.Normal,
		false,
		true)

	Session:new_midi_track (
		input,
		output,
		false,
		instrument,
		nil,
		group,
		1,
		"Reactive Macro Lane",
		ARDOUR.PresentationInfo.max_order,
		ARDOUR.TrackMode.Normal,
		false,
		true)

	seed_demo_markers ()
	seed_demo_regions ()
	seed_demo_trigger_clips ()
	install_demo_action_document ()

	Session:save_state ("")
end end
