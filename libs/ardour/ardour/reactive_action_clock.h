#pragma once

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

#include "temporal/bbt_time.h"

namespace Temporal {
class TempoMap;
}

namespace ARDOUR {

struct LIBARDOUR_API ReactiveActionClockPosition {
	Temporal::BBT_Time requested_at;
	Temporal::BBT_Time due_at;
};

class LIBARDOUR_API ReactiveActionClock
{
public:
	static ReactiveActionClockPosition quantize_bbt (
		Temporal::TempoMap const&,
		Temporal::BBT_Time const& requested_at,
		Temporal::BBT_Offset const& quantize);

	static ReactiveActionClockPosition quantize_sample (
		Temporal::TempoMap const&,
		samplepos_t sample,
		Temporal::BBT_Offset const& quantize);
};

} // namespace ARDOUR
