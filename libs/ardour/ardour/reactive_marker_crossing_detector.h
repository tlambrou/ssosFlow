#pragma once

#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

struct LIBARDOUR_API ReactiveMarkerObservation {
	static ReactiveMarkerObservation at (std::string const& name, samplepos_t sample);

	std::string name;
	samplepos_t sample = 0;
};

struct LIBARDOUR_API ReactiveMarkerCrossing {
	std::string name;
	samplepos_t sample = 0;
};

class LIBARDOUR_API ReactiveMarkerCrossingDetector {
public:
	void reset ();
	void reset (samplepos_t sample);

	std::vector<ReactiveMarkerCrossing> poll (
		samplepos_t transport_sample,
		bool transport_rolling,
		std::vector<ReactiveMarkerObservation> const& markers,
		bool discontinuity = false);

private:
	bool _has_position = false;
	samplepos_t _last_sample = 0;
};

} // namespace ARDOUR
