#pragma once

#include <cstddef>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API ReactiveRhythmStepSource {
public:
	ReactiveRhythmStepSource (samplepos_t origin_frame = 0, samplecnt_t frames_per_step = 1);

	void configure (samplepos_t origin_frame, samplecnt_t frames_per_step);

	samplepos_t origin_frame () const { return _origin_frame; }
	samplecnt_t frames_per_step () const { return _frames_per_step; }

	size_t step_for (samplepos_t frame) const;

private:
	static samplecnt_t normalized_frames_per_step (samplecnt_t frames_per_step);

	samplepos_t _origin_frame;
	samplecnt_t _frames_per_step;
};

} // namespace ARDOUR
