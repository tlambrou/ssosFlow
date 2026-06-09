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
DO state groove reset
DO cue 0
END

ACTION demo.tighten
TRIGGER midi note ch=10 note=37
DO rhythm insert 0
DO rhythm route 0 density 0.75
DO rhythm route 0 chance 1
DO rhythm route 0 priority_mode 3
DO state groove tight
DO cue 1
END

ACTION demo.sparse
TRIGGER midi note ch=10 note=38
DO rhythm insert 0
DO rhythm route 0 density 0.35
DO rhythm route 0 chance 0.5
DO rhythm route 0 priority_mode 1
DO state groove sparse
DO cue 2
END

ACTION demo.rotate
TRIGGER midi note ch=10 note=39
DO rhythm insert 0
DO rhythm route 0 rotation 4
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
DO macro filter midi-value ramp 0|1|0
DO state macro filter
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
		false)

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
		false)

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
		false)

	install_demo_action_document ()

	Session:save_state ("")
end end
