#include "reactive_controller_feedback_midi_cache_test.h"

#include "ardour/reactive_controller_feedback_midi_cache.h"

#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveControllerFeedbackMidiCacheTest);

using namespace ARDOUR;

namespace {

static ReactiveControllerFeedbackMidiMessage
message (size_t slot, std::vector<unsigned char> const& bytes)
{
	ReactiveControllerFeedbackMidiMessage message;
	message.slot = slot;
	message.bytes = bytes;
	return message;
}

class RecordingWriter {
public:
	void operator() (ReactiveControllerFeedbackMidiMessage const& message)
	{
		writes.push_back (message);
	}

	std::vector<ReactiveControllerFeedbackMidiMessage> writes;
};

} // namespace

void
ReactiveControllerFeedbackMidiCacheTest::storeSnapshotAndClearMessages ()
{
	ReactiveControllerFeedbackMidiCache cache;

	CPPUNIT_ASSERT_EQUAL (size_t (0), cache.message_count ());
	CPPUNIT_ASSERT (cache.snapshot ().empty ());

	cache.set_messages ({
		message (0, { 0x99, 36, 32 }),
		message (1, { 0xb0, 22, 127 })
	});

	CPPUNIT_ASSERT_EQUAL (size_t (2), cache.message_count ());
	std::vector<ReactiveControllerFeedbackMidiMessage> snapshot = cache.snapshot ();
	CPPUNIT_ASSERT_EQUAL (size_t (2), snapshot.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), snapshot[0].slot);
	CPPUNIT_ASSERT_EQUAL (size_t (3), snapshot[0].bytes.size ());
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0x99), snapshot[0].bytes[0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (36), snapshot[0].bytes[1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (32), snapshot[0].bytes[2]);
	CPPUNIT_ASSERT_EQUAL (size_t (1), snapshot[1].slot);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0xb0), snapshot[1].bytes[0]);

	cache.clear ();
	CPPUNIT_ASSERT_EQUAL (size_t (0), cache.message_count ());
	CPPUNIT_ASSERT (cache.snapshot ().empty ());
}

void
ReactiveControllerFeedbackMidiCacheTest::tryWriteCachedMessagesSkipsEmptyMessages ()
{
	ReactiveControllerFeedbackMidiCache cache;
	cache.set_messages ({
		message (0, { 0x99, 36, 32 }),
		message (1, {}),
		message (2, { 0x99, 38, 0 })
	});

	RecordingWriter writer;
	CPPUNIT_ASSERT_EQUAL (true, cache.try_write_messages (writer));

	CPPUNIT_ASSERT_EQUAL (size_t (2), writer.writes.size ());
	CPPUNIT_ASSERT_EQUAL (size_t (0), writer.writes[0].slot);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0x99), writer.writes[0].bytes[0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (36), writer.writes[0].bytes[1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (32), writer.writes[0].bytes[2]);
	CPPUNIT_ASSERT_EQUAL (size_t (2), writer.writes[1].slot);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0x99), writer.writes[1].bytes[0]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (38), writer.writes[1].bytes[1]);
	CPPUNIT_ASSERT_EQUAL (static_cast<unsigned char> (0), writer.writes[1].bytes[2]);
}
