ardour {
	["type"]    = "dsp",
	name        = "Reactive Rhythm State MVP",
	category    = "Utility",
	license     = "MIT",
	author      = "Reactive Performance Mode MVP",
	description = [[Prototype MIDI rhythm-state filter for Reactive Performance Mode. It gates note-on events by density, chance, priority, rotation, and quantized parameter latching while passing non-note MIDI through.]]
}

local PRIORITY_OFF = 0
local PRIORITY_DOWNBEAT = 1
local PRIORITY_PITCH = 2
local PRIORITY_VELOCITY = 3

local active_settings = nil
local pending_settings = nil
local last_seen_step = nil
local active_notes = {}
local rng_state = 1

function dsp_ioconfig ()
	return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0 }, }
end

function dsp_options ()
	return { time_info = true, regular_block_length = true }
end

function dsp_params ()
	return {
		{ type = "input", name = "Density %", min = 0, max = 100, default = 100, integer = true, doc = "percentage of active rhythm steps and chord notes to keep" },
		{ type = "input", name = "Chance %", min = 0, max = 100, default = 100, integer = true, doc = "probability that a density-selected note-on will pass" },
		{ type = "input", name = "Priority", min = 0, max = 3, default = 0, integer = true, enum = true, doc = "priority used when density filters simultaneous note-ons",
			scalepoints = { Off = 0, Downbeat = 1, Pitch = 2, Velocity = 3 } },
		{ type = "input", name = "Rotation", min = 0, max = 31, default = 0, integer = true, doc = "positive rhythm-step rotation" },
		{ type = "input", name = "Pattern Steps", min = 1, max = 32, default = 16, integer = true, doc = "number of rhythm steps before the density pattern repeats" },
		{ type = "input", name = "Latch Steps", min = 1, max = 64, default = 1, integer = true, doc = "apply changed parameters only on this rhythm-step boundary" },
		{ type = "input", name = "Steps Per Beat", min = 1, max = 16, default = 4, integer = true, doc = "rhythm-step resolution derived from Ardour's beat time" },
		{ type = "input", name = "Chance Seed", min = 1, max = 2147483647, default = 1, integer = true, doc = "deterministic chance seed for repeatable performance tests" },
	}
end

local function clamp (value, lo, hi)
	if value < lo then return lo end
	if value > hi then return hi end
	return value
end

local function round_int (value)
	return math.floor (value + 0.5)
end

local function read_settings ()
	local ctrl = CtrlPorts:array ()
	return {
		density = clamp (ctrl[1] or 100, 0, 100) / 100.0,
		chance = clamp (ctrl[2] or 100, 0, 100) / 100.0,
		priority = round_int (clamp (ctrl[3] or PRIORITY_OFF, PRIORITY_OFF, PRIORITY_VELOCITY)),
		rotation = round_int (clamp (ctrl[4] or 0, 0, 31)),
		pattern_steps = round_int (clamp (ctrl[5] or 16, 1, 32)),
		latch_steps = round_int (clamp (ctrl[6] or 1, 1, 64)),
		steps_per_beat = round_int (clamp (ctrl[7] or 4, 1, 16)),
		seed = round_int (clamp (ctrl[8] or 1, 1, 2147483647)),
	}
end

local function step_from_time (settings)
	local beat = 0.0
	if type (time) == "table" and type (time["beat"]) == "number" then
		beat = time["beat"]
	end
	if beat < 0 then
		beat = 0
	end
	return math.floor (beat * settings.steps_per_beat)
end

local function apply_settings (settings)
	local previous_seed = active_settings and active_settings.seed
	active_settings = settings
	if previous_seed ~= active_settings.seed then
		rng_state = active_settings.seed
	end
end

local function update_settings_for_block ()
	pending_settings = read_settings ()

	if active_settings == nil then
		rng_state = pending_settings.seed
		apply_settings (pending_settings)
		last_seen_step = step_from_time (active_settings)
		return last_seen_step
	end

	local step = step_from_time (active_settings)
	if step ~= last_seen_step then
		if (step % active_settings.latch_steps) == 0 then
			apply_settings (pending_settings)
			step = step_from_time (active_settings)
		end
		last_seen_step = step
	end

	return step
end

local function rotated_step (step, settings)
	return (step + settings.rotation) % settings.pattern_steps
end

local function density_gate_open (step, settings)
	if settings.density <= 0 then
		return false
	end
	if settings.density >= 1 then
		return true
	end

	local active_steps = math.floor (settings.pattern_steps * settings.density)
	if active_steps < 1 then
		active_steps = 1
	end
	return rotated_step (step, settings) < active_steps
end

local function next_chance ()
	rng_state = ((1664525 * rng_state) + 1013904223) & 0x7fffffff
	return rng_state / 2147483648.0
end

local function passes_chance (settings)
	if settings.chance <= 0 then
		return false
	end
	if settings.chance >= 1 then
		return true
	end
	return next_chance () < settings.chance
end

local function status_parts (data)
	if type (data) ~= "table" or #data < 1 then
		return nil, nil
	end
	return data[1] & 0xf0, data[1] & 0x0f
end

local function is_note_on (data)
	local event_type = status_parts (data)
	return #data >= 3 and event_type == 0x90 and data[3] > 0
end

local function is_note_off (data)
	local event_type = status_parts (data)
	return #data >= 3 and (event_type == 0x80 or (event_type == 0x90 and data[3] == 0))
end

local function note_key (data)
	local _, channel = status_parts (data)
	return (channel * 128) + data[2]
end

local function priority_for (candidate, step, settings)
	if settings.priority == PRIORITY_DOWNBEAT then
		return settings.pattern_steps - rotated_step (step, settings)
	elseif settings.priority == PRIORITY_PITCH then
		return candidate.note
	elseif settings.priority == PRIORITY_VELOCITY then
		return candidate.velocity
	end

	return -candidate.index
end

local function choose_note_on_passes (events, step, settings)
	local candidates = {}
	local passes = {}

	if not density_gate_open (step, settings) then
		return passes
	end

	for index, event in ipairs (events) do
		local data = event["data"]
		if is_note_on (data) then
			candidates[#candidates + 1] = {
				index = index,
				note = data[2],
				velocity = data[3],
			}
		end
	end

	if #candidates == 0 then
		return passes
	end

	for _, candidate in ipairs (candidates) do
		candidate.priority = priority_for (candidate, step, settings)
	end

	table.sort (candidates, function (a, b)
		if a.priority == b.priority then
			return a.index < b.index
		end
		return a.priority > b.priority
	end)

	local keep_count = #candidates
	if settings.density < 1 then
		keep_count = math.floor (#candidates * settings.density)
		if keep_count < 1 then
			keep_count = 1
		end
	end

	for i = 1, math.min (keep_count, #candidates) do
		local candidate = candidates[i]
		passes[candidate.index] = passes_chance (settings)
	end

	return passes
end

local function tx_midi (index, event)
	midiout[index] = event
	return index + 1
end

function dsp_run (_, _, n_samples)
	assert (type (midiin) == "table")
	assert (type (midiout) == "table")
	assert (type (time) == "table")

	local step = update_settings_for_block ()
	local note_on_passes = choose_note_on_passes (midiin, step, active_settings)
	local out_index = 1

	for index, event in ipairs (midiin) do
		local data = event["data"]

		if is_note_on (data) then
			local key = note_key (data)
			if note_on_passes[index] then
				active_notes[key] = (active_notes[key] or 0) + 1
				out_index = tx_midi (out_index, event)
			end
		elseif is_note_off (data) then
			local key = note_key (data)
			if (active_notes[key] or 0) > 0 then
				active_notes[key] = active_notes[key] - 1
				if active_notes[key] == 0 then
					active_notes[key] = nil
				end
				out_index = tx_midi (out_index, event)
			end
		else
			out_index = tx_midi (out_index, event)
		end
	end
end
