#pragma once

#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

struct LIBARDOUR_API ReactiveRegionObservation {
	static ReactiveRegionObservation at (std::string const& name, samplepos_t sample, std::string const& route_name = std::string (), size_t route_order = 0);

	std::string name;
	samplepos_t sample = 0;
	std::string route_name;
	size_t route_order = 0;
};

struct LIBARDOUR_API ReactiveRegionCrossing {
	std::string name;
	samplepos_t sample = 0;
	std::string route_name;
	size_t route_order = 0;
};

class LIBARDOUR_API ReactiveRegionCrossingDetector {
public:
	void reset ();
	void reset (samplepos_t sample);

	std::vector<ReactiveRegionCrossing> poll (
		samplepos_t transport_sample,
		bool transport_rolling,
		std::vector<ReactiveRegionObservation> const& regions,
		bool discontinuity = false);

private:
	bool _has_position = false;
	samplepos_t _last_sample = 0;
};

} // namespace ARDOUR
