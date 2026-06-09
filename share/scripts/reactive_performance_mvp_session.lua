ardour {
	["type"]    = "SessionInit",
	name        = "Reactive Performance MVP",
	category    = "Reactive Performance",
	description = [[Create a small MIDI-route layout for the Reactive Performance Mode MVP demo.]]
}

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

	Session:save_state ("")
end end
