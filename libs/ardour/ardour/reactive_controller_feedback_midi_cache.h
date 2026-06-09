#pragma once

#include <cstddef>
#include <vector>

#include "pbd/mutex.h"

#include "ardour/libardour_visibility.h"
#include "ardour/reactive_action_slot_runner.h"

namespace ARDOUR {

class LIBARDOUR_API ReactiveControllerFeedbackMidiCache {
public:
	void set_messages (std::vector<ReactiveControllerFeedbackMidiMessage> const&);
	void clear ();
	size_t message_count () const;
	std::vector<ReactiveControllerFeedbackMidiMessage> snapshot () const;

	template <typename Writer>
	bool try_write_messages (Writer& writer) const
	{
		PBD::Mutex::Lock lock (_lock, PBD::Mutex::TryLock);
		if (!lock.locked ()) {
			return false;
		}

		for (std::vector<ReactiveControllerFeedbackMidiMessage>::const_iterator message = _messages.begin (); message != _messages.end (); ++message) {
			if (!message->bytes.empty ()) {
				writer (*message);
			}
		}

		return true;
	}

private:
	mutable PBD::Mutex _lock;
	std::vector<ReactiveControllerFeedbackMidiMessage> _messages;
};

} // namespace ARDOUR
