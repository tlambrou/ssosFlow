#include "ardour/reactive_rhythm_step_source.h"

#include <cmath>

using namespace ARDOUR;

namespace {

static double
positive_or_one (double value)
{
	if (value <= 0.0) {
		return 1.0;
	}

	return value;
}

} // namespace

ReactiveRhythmStepSource::ReactiveRhythmStepSource (samplepos_t origin_frame, samplecnt_t frames_per_step)
	: _origin_frame (origin_frame)
	, _frames_per_step (normalized_frames_per_step (frames_per_step))
{
}

void
ReactiveRhythmStepSource::configure (samplepos_t origin_frame, samplecnt_t frames_per_step)
{
	_origin_frame = origin_frame;
	_frames_per_step = normalized_frames_per_step (frames_per_step);
}

void
ReactiveRhythmStepSource::configure_from_tempo (samplepos_t origin_frame, double sample_rate, double bpm, size_t steps_per_beat)
{
	configure (origin_frame, frames_per_step_for_tempo (sample_rate, bpm, steps_per_beat));
}

size_t
ReactiveRhythmStepSource::step_for (samplepos_t frame) const
{
	if (frame <= _origin_frame) {
		return 0;
	}

	return static_cast<size_t> ((frame - _origin_frame) / _frames_per_step);
}

samplecnt_t
ReactiveRhythmStepSource::frames_per_step_for_tempo (double sample_rate, double bpm, size_t steps_per_beat)
{
	size_t const normalized_steps_per_beat = steps_per_beat == 0 ? 1 : steps_per_beat;
	double const frames_per_step = (positive_or_one (sample_rate) * 60.0) / (positive_or_one (bpm) * static_cast<double> (normalized_steps_per_beat));
	samplecnt_t const rounded_frames = static_cast<samplecnt_t> (std::floor (frames_per_step + 0.5));

	return normalized_frames_per_step (rounded_frames);
}

samplecnt_t
ReactiveRhythmStepSource::normalized_frames_per_step (samplecnt_t frames_per_step)
{
	if (frames_per_step <= 0) {
		return 1;
	}

	return frames_per_step;
}
