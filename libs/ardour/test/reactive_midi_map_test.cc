#include "reactive_midi_map_test.h"

#include <cstdlib>
#include <map>
#include <string>
#include <utility>

#include <glibmm/miscutils.h>

#include "pbd/xml++.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveMidiMapTest);

using namespace PBD;

namespace {

typedef std::pair<std::string, std::string> BindingKey;

static std::string
reactive_midi_map_path ()
{
	const char* midi_maps_path = std::getenv ("ARDOUR_MIDIMAPS_PATH");
	CPPUNIT_ASSERT_MESSAGE ("ARDOUR_MIDIMAPS_PATH is not set", midi_maps_path != 0);

	return Glib::build_filename (midi_maps_path, "reactive-performance-mvp.map");
}

static std::string
required_property (XMLNode const& node, std::string const& name)
{
	const XMLProperty* property = node.property (name);
	CPPUNIT_ASSERT_MESSAGE ("missing XML property: " + name, property != 0);
	return property->value ();
}

} // namespace

void
ReactiveMidiMapTest::mapContainsExpectedControllerBindings ()
{
	XMLTree tree;
	CPPUNIT_ASSERT_MESSAGE ("could not parse Reactive Performance MIDI map", tree.read (reactive_midi_map_path ().c_str ()));

	XMLNode* root = tree.root ();
	CPPUNIT_ASSERT (root != 0);
	CPPUNIT_ASSERT_EQUAL (std::string ("ArdourMIDIBindings"), root->name ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Performance MVP"), required_property (*root, "name"));

	std::map<BindingKey, std::string> actions_by_note;
	XMLNodeList const& children = root->children ();
	for (XMLNodeConstIterator i = children.begin (); i != children.end (); ++i) {
		XMLNode const* child = *i;
		if (child->name () != "Binding") {
			continue;
		}
		if (!child->property ("note")) {
			continue;
		}

		BindingKey key (required_property (*child, "channel"), required_property (*child, "note"));
		actions_by_note[key] = required_property (*child, "action");
	}

	for (int slot = 0; slot < 8; ++slot) {
		BindingKey const key ("10", std::to_string (36 + slot));
		std::string const expected = "Reactive/trigger-action-" + std::to_string (slot);
		CPPUNIT_ASSERT (actions_by_note.find (key) != actions_by_note.end ());
		CPPUNIT_ASSERT_EQUAL (expected, actions_by_note[key]);
	}

	CPPUNIT_ASSERT (actions_by_note.find (BindingKey ("10", "44")) != actions_by_note.end ());
	CPPUNIT_ASSERT_EQUAL (
		std::string ("Reactive/show-action-document-status"),
		actions_by_note[BindingKey ("10", "44")]);
	CPPUNIT_ASSERT (actions_by_note.find (BindingKey ("10", "45")) != actions_by_note.end ());
	CPPUNIT_ASSERT_EQUAL (
		std::string ("Reactive/reload-action-document"),
		actions_by_note[BindingKey ("10", "45")]);
}
