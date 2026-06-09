#include "ardour/reactive_rhythm.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ARDOUR;

namespace {

static double
clamped_unit (double value)
{
	if (value < 0.0) {
		return 0.0;
	}

	if (value > 1.0) {
		return 1.0;
	}

	return value;
}

} // namespace

ReactiveRhythmState::ReactiveRhythmState ()
	: _settings (normalized_settings (ReactiveRhythmSettings ()))
	, _pending_settings (_settings)
{
}

ReactiveRhythmState::ReactiveRhythmState (ReactiveRhythmSettings const& settings)
	: _settings (normalized_settings (settings))
	, _pending_settings (_settings)
{
}

void
ReactiveRhythmState::set_settings (ReactiveRhythmSettings const& settings)
{
	_settings = normalized_settings (settings);
	_pending_settings = _settings;
	_has_pending_settings = false;
}

void
ReactiveRhythmState::queue_settings (ReactiveRhythmSettings const& settings)
{
	_pending_settings = normalized_settings (settings);
	_has_pending_settings = true;
}

void
ReactiveRhythmState::advance_to_step (size_t step)
{
	if (!_has_pending_settings) {
		return;
	}

	if (step % _settings.latch_steps != 0) {
		return;
	}

	set_settings (_pending_settings);
}

std::vector<ReactiveRhythmDecision>
ReactiveRhythmState::evaluate (std::vector<ReactiveRhythmEvent> const& events) const
{
	std::vector<ReactiveRhythmDecision> decisions (events.size ());
	std::vector<size_t> indices;
	indices.reserve (events.size ());

	for (size_t i = 0; i < events.size (); ++i) {
		decisions[i].rotated_step = rotated_step (events[i].step);
		decisions[i].priority = priority_for (events[i], decisions[i].rotated_step);
		indices.push_back (i);
	}

	size_t keep_count = 0;
	if (_settings.density >= 1.0) {
		keep_count = events.size ();
	} else if (_settings.density > 0.0) {
		keep_count = static_cast<size_t> (std::floor (events.size () * _settings.density));
	}

	if (keep_count == 0) {
		return decisions;
	}

	std::sort (indices.begin (), indices.end (), [&decisions] (size_t lhs, size_t rhs) {
		if (decisions[lhs].priority == decisions[rhs].priority) {
			return lhs < rhs;
		}

		return decisions[lhs].priority > decisions[rhs].priority;
	});

	for (size_t i = 0; i < keep_count && i < indices.size (); ++i) {
		size_t const index = indices[i];
		decisions[index].passes = clamped_unit (events[index].chance_value) < _settings.chance;
	}

	return decisions;
}

ReactiveRhythmSettings
ReactiveRhythmState::normalized_settings (ReactiveRhythmSettings settings)
{
	if (settings.pattern_steps == 0) {
		settings.pattern_steps = 1;
	}

	if (settings.latch_steps == 0) {
		settings.latch_steps = 1;
	}

	settings.density = clamped_unit (settings.density);
	settings.chance = clamped_unit (settings.chance);

	return settings;
}

size_t
ReactiveRhythmState::rotated_step (size_t step) const
{
	size_t const pattern_steps = _settings.pattern_steps;
	size_t const normalized_step = step % pattern_steps;
	size_t rotation = 0;

	if (_settings.rotation >= 0) {
		rotation = static_cast<size_t> (_settings.rotation) % pattern_steps;
	} else {
		size_t const reverse_rotation = static_cast<size_t> (-static_cast<long long> (_settings.rotation)) % pattern_steps;
		if (reverse_rotation != 0) {
			rotation = pattern_steps - reverse_rotation;
		}
	}

	return (normalized_step + static_cast<size_t> (rotation)) % pattern_steps;
}

double
ReactiveRhythmState::priority_for (ReactiveRhythmEvent const& event, size_t rotated_step) const
{
	switch (_settings.priority_mode) {
	case ReactiveRhythmPriorityMode::Downbeat:
		if (rotated_step == 0) {
			return 4.0;
		}
		if (rotated_step % 4 == 0) {
			return 3.0;
		}
		if (rotated_step % 2 == 0) {
			return 2.0;
		}
		return 1.0;
	case ReactiveRhythmPriorityMode::Pitch:
		return static_cast<double> (event.pitch);
	case ReactiveRhythmPriorityMode::Velocity:
		return static_cast<double> (event.velocity);
	case ReactiveRhythmPriorityMode::Off:
		break;
	}

	return 0.0;
}
