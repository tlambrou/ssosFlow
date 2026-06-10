#include "ardour/reactive_action_clock.h"

#include "temporal/bbt_argument.h"
#include "temporal/tempo.h"
#include "temporal/timeline.h"
#include "temporal/types.h"

using namespace ARDOUR;

namespace {

static bool
zero_quantize (Temporal::BBT_Offset const& quantize)
{
	return quantize.bars == 0 && quantize.beats == 0 && quantize.ticks == 0;
}

static Temporal::BBT_Time
walk_bbt (Temporal::TempoMap const& map, Temporal::BBT_Time const& start, Temporal::BBT_Offset const& offset)
{
	return map.bbt_walk (Temporal::BBT_Argument (start), offset);
}

static Temporal::BBT_Time
next_bar_boundary (Temporal::TempoMap const& map, Temporal::BBT_Time const& requested_at, Temporal::BBT_Offset const& quantize)
{
	Temporal::BBT_Time const boundary = map.round_up_to_bar (Temporal::BBT_Argument (requested_at));
	int32_t bars_to_walk = quantize.bars;
	if (boundary != requested_at && bars_to_walk > 0) {
		--bars_to_walk;
	}

	return bars_to_walk == 0 ? boundary : walk_bbt (map, boundary, Temporal::BBT_Offset (bars_to_walk, 0, 0));
}

static Temporal::BBT_Time
next_beat_boundary (Temporal::TempoMap const& map, Temporal::BBT_Time const& requested_at, Temporal::BBT_Offset const& quantize)
{
	Temporal::TempoMetric const& metric = map.metric_at (Temporal::BBT_Argument (requested_at));
	Temporal::BBT_Time const boundary = metric.meter ().round_up_to_beat (requested_at);
	int32_t beats_to_walk = quantize.beats;
	if (boundary != requested_at && beats_to_walk > 0) {
		--beats_to_walk;
	}

	return beats_to_walk == 0 ? boundary : walk_bbt (map, boundary, Temporal::BBT_Offset (0, beats_to_walk, 0));
}

static Temporal::BBT_Time
next_tick_boundary (Temporal::TempoMap const& map, Temporal::BBT_Time const& requested_at, Temporal::BBT_Offset const& quantize)
{
	if (quantize.ticks <= 0) {
		return requested_at;
	}

	int32_t next_tick = requested_at.ticks + (quantize.ticks - (requested_at.ticks % quantize.ticks));
	if ((requested_at.ticks % quantize.ticks) == 0) {
		next_tick = requested_at.ticks + quantize.ticks;
	}

	if (next_tick < Temporal::ticks_per_beat) {
		return Temporal::BBT_Time (requested_at.bars, requested_at.beats, next_tick);
	}

	return walk_bbt (
		map,
		Temporal::BBT_Time (requested_at.bars, requested_at.beats, 0),
		Temporal::BBT_Offset (0, 1, next_tick - Temporal::ticks_per_beat));
}

} // namespace

ReactiveActionClockPosition
ReactiveActionClock::quantize_bbt (
	Temporal::TempoMap const& map,
	Temporal::BBT_Time const& requested_at,
	Temporal::BBT_Offset const& quantize)
{
	ReactiveActionClockPosition position;
	position.requested_at = requested_at;
	position.due_at = requested_at;

	if (zero_quantize (quantize)) {
		return position;
	}

	if (quantize.bars > 0 && quantize.beats == 0 && quantize.ticks == 0) {
		position.due_at = next_bar_boundary (map, requested_at, quantize);
		return position;
	}

	if (quantize.bars == 0 && quantize.beats > 0 && quantize.ticks == 0) {
		position.due_at = next_beat_boundary (map, requested_at, quantize);
		return position;
	}

	if (quantize.bars == 0 && quantize.beats == 0 && quantize.ticks > 0) {
		position.due_at = next_tick_boundary (map, requested_at, quantize);
		return position;
	}

	position.due_at = walk_bbt (map, requested_at, quantize);
	return position;
}

ReactiveActionClockPosition
ReactiveActionClock::quantize_sample (
	Temporal::TempoMap const& map,
	samplepos_t sample,
	Temporal::BBT_Offset const& quantize)
{
	return quantize_bbt (map, map.bbt_at (Temporal::timepos_t (sample)), quantize);
}
