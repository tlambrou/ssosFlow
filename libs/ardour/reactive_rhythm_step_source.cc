#include "ardour/reactive_rhythm_step_source.h"

using namespace ARDOUR;

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

size_t
ReactiveRhythmStepSource::step_for (samplepos_t frame) const
{
	if (frame <= _origin_frame) {
		return 0;
	}

	return static_cast<size_t> ((frame - _origin_frame) / _frames_per_step);
}

samplecnt_t
ReactiveRhythmStepSource::normalized_frames_per_step (samplecnt_t frames_per_step)
{
	if (frames_per_step <= 0) {
		return 1;
	}

	return frames_per_step;
}
