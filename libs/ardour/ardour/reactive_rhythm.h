#pragma once

#include <cstddef>
#include <vector>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

enum class ReactiveRhythmPriorityMode {
	Off,
	Downbeat,
	Pitch,
	Velocity
};

struct LIBARDOUR_API ReactiveRhythmSettings {
	size_t pattern_steps = 16;
	double density = 1.0;
	double chance = 1.0;
	ReactiveRhythmPriorityMode priority_mode = ReactiveRhythmPriorityMode::Off;
	int rotation = 0;
	size_t latch_steps = 1;
};

struct LIBARDOUR_API ReactiveRhythmEvent {
	size_t step = 0;
	int pitch = 0;
	int velocity = 0;
	double chance_value = 0.0;
};

struct LIBARDOUR_API ReactiveRhythmDecision {
	bool passes = false;
	size_t rotated_step = 0;
	double priority = 0.0;
};

class LIBARDOUR_API ReactiveRhythmState {
public:
	ReactiveRhythmState ();
	explicit ReactiveRhythmState (ReactiveRhythmSettings const&);

	ReactiveRhythmSettings const& settings () const { return _settings; }

	void set_settings (ReactiveRhythmSettings const&);
	void queue_settings (ReactiveRhythmSettings const&);
	bool has_pending_settings () const { return _has_pending_settings; }
	void advance_to_step (size_t step);

	std::vector<ReactiveRhythmDecision> evaluate (std::vector<ReactiveRhythmEvent> const&) const;

private:
	static ReactiveRhythmSettings normalized_settings (ReactiveRhythmSettings);
	size_t rotated_step (size_t step) const;
	double priority_for (ReactiveRhythmEvent const&, size_t rotated_step) const;

	ReactiveRhythmSettings _settings;
	ReactiveRhythmSettings _pending_settings;
	bool _has_pending_settings = false;
};

} // namespace ARDOUR
