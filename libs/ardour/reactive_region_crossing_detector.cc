#include "ardour/reactive_region_crossing_detector.h"

#include <algorithm>

using namespace ARDOUR;

namespace {

struct OrderedRegion {
	ReactiveRegionObservation region;
	size_t index = 0;
};

static bool
region_order (OrderedRegion const& lhs, OrderedRegion const& rhs)
{
	if (lhs.region.sample != rhs.region.sample) {
		return lhs.region.sample < rhs.region.sample;
	}

	if (lhs.region.route_order != rhs.region.route_order) {
		return lhs.region.route_order < rhs.region.route_order;
	}

	return lhs.index < rhs.index;
}

} // namespace

ReactiveRegionObservation
ReactiveRegionObservation::at (std::string const& name, samplepos_t sample, std::string const& route_name, size_t route_order)
{
	ReactiveRegionObservation region;
	region.name = name;
	region.sample = sample;
	region.route_name = route_name;
	region.route_order = route_order;
	return region;
}

void
ReactiveRegionCrossingDetector::reset ()
{
	_has_position = false;
	_last_sample = 0;
}

void
ReactiveRegionCrossingDetector::reset (samplepos_t sample)
{
	_has_position = true;
	_last_sample = sample;
}

std::vector<ReactiveRegionCrossing>
ReactiveRegionCrossingDetector::poll (
	samplepos_t transport_sample,
	bool transport_rolling,
	std::vector<ReactiveRegionObservation> const& regions,
	bool discontinuity)
{
	std::vector<ReactiveRegionCrossing> crossings;

	if (!transport_rolling || discontinuity) {
		reset (transport_sample);
		return crossings;
	}

	if (!_has_position || transport_sample <= _last_sample) {
		reset (transport_sample);
		return crossings;
	}

	std::vector<OrderedRegion> ordered;
	ordered.reserve (regions.size ());
	for (size_t index = 0; index < regions.size (); ++index) {
		if (regions[index].name.empty ()) {
			continue;
		}

		OrderedRegion row;
		row.region = regions[index];
		row.index = index;
		ordered.push_back (row);
	}

	std::stable_sort (ordered.begin (), ordered.end (), region_order);

	for (std::vector<OrderedRegion>::const_iterator region = ordered.begin (); region != ordered.end (); ++region) {
		if (region->region.sample <= _last_sample || region->region.sample > transport_sample) {
			continue;
		}

		ReactiveRegionCrossing crossing;
		crossing.name = region->region.name;
		crossing.sample = region->region.sample;
		crossing.route_name = region->region.route_name;
		crossing.route_order = region->region.route_order;
		crossings.push_back (crossing);
	}

	_last_sample = transport_sample;
	return crossings;
}
