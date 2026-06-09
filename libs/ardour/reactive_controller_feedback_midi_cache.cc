#include "ardour/reactive_controller_feedback_midi_cache.h"

using namespace ARDOUR;

void
ReactiveControllerFeedbackMidiCache::set_messages (std::vector<ReactiveControllerFeedbackMidiMessage> const& messages)
{
	PBD::Mutex::Lock lock (_lock);
	_messages = messages;
}

void
ReactiveControllerFeedbackMidiCache::clear ()
{
	PBD::Mutex::Lock lock (_lock);
	_messages.clear ();
}

size_t
ReactiveControllerFeedbackMidiCache::message_count () const
{
	PBD::Mutex::Lock lock (_lock);
	return _messages.size ();
}

std::vector<ReactiveControllerFeedbackMidiMessage>
ReactiveControllerFeedbackMidiCache::snapshot () const
{
	PBD::Mutex::Lock lock (_lock);
	return _messages;
}
