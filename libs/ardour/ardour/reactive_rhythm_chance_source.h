#pragma once

#include <stdint.h>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API ReactiveRhythmChanceSource {
public:
	explicit ReactiveRhythmChanceSource (uint32_t seed = 0x6d2b79f5u);

	void reset (uint32_t seed);
	double next ();

	uint32_t seed () const { return _seed; }
	uint32_t state () const { return _state; }

private:
	static uint32_t normalized_seed (uint32_t seed);

	uint32_t _seed;
	uint32_t _state;
};

} // namespace ARDOUR
