#include "ardour/reactive_marker_crossing_detector.h"

#include <algorithm>

using namespace ARDOUR;

namespace {

struct OrderedMarker {
	ReactiveMarkerObservation marker;
	size_t index = 0;
};

static bool
marker_order (OrderedMarker const& lhs, OrderedMarker const& rhs)
{
	if (lhs.marker.sample != rhs.marker.sample) {
		return lhs.marker.sample < rhs.marker.sample;
	}

	return lhs.index < rhs.index;
}

} // namespace

ReactiveMarkerObservation
ReactiveMarkerObservation::at (std::string const& name, samplepos_t sample)
{
	ReactiveMarkerObservation marker;
	marker.name = name;
	marker.sample = sample;
	return marker;
}

void
ReactiveMarkerCrossingDetector::reset ()
{
	_has_position = false;
	_last_sample = 0;
}

void
ReactiveMarkerCrossingDetector::reset (samplepos_t sample)
{
	_has_position = true;
	_last_sample = sample;
}

std::vector<ReactiveMarkerCrossing>
ReactiveMarkerCrossingDetector::poll (
	samplepos_t transport_sample,
	bool transport_rolling,
	std::vector<ReactiveMarkerObservation> const& markers,
	bool discontinuity)
{
	std::vector<ReactiveMarkerCrossing> crossings;

	if (!transport_rolling || discontinuity) {
		reset (transport_sample);
		return crossings;
	}

	if (!_has_position || transport_sample <= _last_sample) {
		reset (transport_sample);
		return crossings;
	}

	std::vector<OrderedMarker> ordered;
	ordered.reserve (markers.size ());
	for (size_t index = 0; index < markers.size (); ++index) {
		if (markers[index].name.empty ()) {
			continue;
		}

		OrderedMarker row;
		row.marker = markers[index];
		row.index = index;
		ordered.push_back (row);
	}

	std::stable_sort (ordered.begin (), ordered.end (), marker_order);

	for (std::vector<OrderedMarker>::const_iterator marker = ordered.begin (); marker != ordered.end (); ++marker) {
		if (marker->marker.sample <= _last_sample || marker->marker.sample > transport_sample) {
			continue;
		}

		ReactiveMarkerCrossing crossing;
		crossing.name = marker->marker.name;
		crossing.sample = marker->marker.sample;
		crossings.push_back (crossing);
	}

	_last_sample = transport_sample;
	return crossings;
}
