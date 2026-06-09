#include "ardour/reactive_rhythm_chance_source.h"

using namespace ARDOUR;

ReactiveRhythmChanceSource::ReactiveRhythmChanceSource (uint32_t seed)
	: _seed (normalized_seed (seed))
	, _state (_seed)
{
}

void
ReactiveRhythmChanceSource::reset (uint32_t seed)
{
	_seed = normalized_seed (seed);
	_state = _seed;
}

double
ReactiveRhythmChanceSource::next ()
{
	uint32_t value = _state;
	value ^= value << 13;
	value ^= value >> 17;
	value ^= value << 5;
	_state = value;

	return static_cast<double> (value) / 4294967296.0;
}

uint32_t
ReactiveRhythmChanceSource::normalized_seed (uint32_t seed)
{
	if (seed == 0) {
		return 0x6d2b79f5u;
	}

	return seed;
}
