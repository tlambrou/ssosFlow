#include <list>
#include <fstream>
#include <glibmm.h>

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/location.h"
#include "ardour/lua_api.h"
#include "ardour/luabindings.h"
#include "ardour/luascripting.h"
#include "ardour/lua_script_params.h"
#include "ardour/midi_track.h"
#include "ardour/midi_region.h"
#include "ardour/playlist.h"
#include "ardour/plugin_manager.h"
#include "ardour/plugin_insert.h"
#include "ardour/region.h"
#include "ardour/session.h"
#include "ardour/triggerbox.h"

#include "lua_script_test.h"

#include "pbd/gstdio_compat.h"

#include "lua/luastate.h"

using namespace ARDOUR;

CPPUNIT_TEST_SUITE_REGISTRATION(LuaScriptTest);

namespace {

class TemporaryDirectory {
public:
	explicit TemporaryDirectory (std::string const& prefix)
	{
		GError* error = 0;
		char* tmp = g_dir_make_tmp (prefix.c_str (), &error);
		if (!tmp) {
			std::string message = error ? error->message : "unknown temporary directory error";
			if (error) {
				g_error_free (error);
			}
			CPPUNIT_FAIL ("could not create temporary directory: " + message);
		}

		_path = tmp;
		g_free (tmp);
	}

	~TemporaryDirectory ()
	{
		if (!_path.empty ()) {
			g_remove (Glib::build_filename (_path, "reactive-actions.txt").c_str ());
			g_rmdir (_path.c_str ());
		}
	}

	std::string const& path () const
	{
		return _path;
	}

private:
	std::string _path;
};

static std::string
lua_literal (std::string const& value)
{
	CPPUNIT_ASSERT_MESSAGE ("test path cannot be represented as a Lua long string", value.find ("]]") == std::string::npos);
	return "[[" + value + "]]";
}

static void
write_file (std::string const& path, std::string const& content)
{
	std::ofstream out (path.c_str ());
	CPPUNIT_ASSERT (out.good ());
	out << content;
	out.close ();
}

static size_t
count_named_markers (Session& session, std::string const& name)
{
	size_t count = 0;

	Locations const* locations = session.locations ();
	if (!locations) {
		return count;
	}

	Locations::LocationList const& list = locations->list ();
	for (Locations::LocationList::const_iterator location = list.begin (); location != list.end (); ++location) {
		if (*location && (*location)->is_mark () && !(*location)->is_hidden () && (*location)->name () == name) {
			++count;
		}
	}

	return count;
}

static size_t
count_named_regions_on_route (Session& session, std::string const& route_name, std::string const& region_name)
{
	std::shared_ptr<Route> route = session.route_by_name (route_name);
	std::shared_ptr<MidiTrack> midi_track = std::dynamic_pointer_cast<MidiTrack> (route);
	if (!midi_track) {
		return 0;
	}

	std::shared_ptr<Playlist> playlist = midi_track->playlist ();
	if (!playlist) {
		return 0;
	}

	size_t count = 0;
	playlist->foreach_region ([&count, &region_name] (std::shared_ptr<Region> region) {
		if (region && !region->hidden () && region->name () == region_name) {
			++count;
		}
	});

	return count;
}

static size_t
count_named_trigger_regions_on_route (Session& session, std::string const& route_name, std::string const& region_name)
{
	std::shared_ptr<Route> route = session.route_by_name (route_name);
	if (!route || !route->triggerbox ()) {
		return 0;
	}

	size_t count = 0;
	std::shared_ptr<TriggerBox> triggerbox = route->triggerbox ();
	for (int slot = 0; slot < TriggerBox::default_triggers_per_box; ++slot) {
		TriggerPtr trigger = triggerbox->trigger (slot);
		std::shared_ptr<Region> region = trigger ? trigger->the_region () : std::shared_ptr<Region> ();
		if (region && !region->hidden () && region->name () == region_name) {
			++count;
		}
	}

	return count;
}

static std::shared_ptr<MidiRegion>
named_trigger_midi_region_on_route (Session& session, std::string const& route_name, std::string const& region_name)
{
	std::shared_ptr<Route> route = session.route_by_name (route_name);
	if (!route || !route->triggerbox ()) {
		return std::shared_ptr<MidiRegion> ();
	}

	std::shared_ptr<TriggerBox> triggerbox = route->triggerbox ();
	for (int slot = 0; slot < TriggerBox::default_triggers_per_box; ++slot) {
		TriggerPtr trigger = triggerbox->trigger (slot);
		std::shared_ptr<Region> region = trigger ? trigger->the_region () : std::shared_ptr<Region> ();
		if (region && !region->hidden () && region->name () == region_name) {
			return std::dynamic_pointer_cast<MidiRegion> (region);
		}
	}

	return std::shared_ptr<MidiRegion> ();
}

static size_t
count_notes_in_named_trigger_region (Session& session, std::string const& route_name, std::string const& region_name)
{
	std::shared_ptr<MidiRegion> region = named_trigger_midi_region_on_route (session, route_name, region_name);
	if (!region || !region->model ()) {
		return 0;
	}

	return region->model ()->notes ().size ();
}

static int
first_note_number_in_named_trigger_region (Session& session, std::string const& route_name, std::string const& region_name)
{
	std::shared_ptr<MidiRegion> region = named_trigger_midi_region_on_route (session, route_name, region_name);
	if (!region || !region->model () || region->model ()->notes ().empty ()) {
		return -1;
	}

	return (*region->model ()->notes ().begin ())->note ();
}

static std::string
reactive_template_path ()
{
	LuaScriptList scripts (LuaScripting::instance ().scripts (LuaScriptInfo::SessionInit));

	for (LuaScriptList::const_iterator s = scripts.begin(); s != scripts.end(); ++s) {
		if ((*s)->name == "Reactive Performance MVP") {
			return (*s)->path;
		}
	}

	CPPUNIT_FAIL ("Reactive Performance MVP SessionInit script was not discoverable");
	return std::string ();
}

static void
run_reactive_template_with_fake_session (std::string const& session_path, bool file_io_available = true)
{
	LuaState lua (true, false);

	lua.do_command (
		"function ardour (entry) ardour_metadata = entry end\n"
		"created_tracks = {}\n"
		"created_markers = {}\n"
		"created_regions = {}\n"
		"created_trigger_regions = {}\n"
		"created_trigger_notes = {}\n"
		"saved = false\n"
		"ARDOUR = {\n"
		"  LuaAPI = {\n"
		"    build_filename = function (...)\n"
		"      local path = ''\n"
		"      for i = 1, select ('#', ...) do\n"
		"        local part = select (i, ...)\n"
		"        if i == 1 then path = part else path = path .. '/' .. part end\n"
		"      end\n"
		"      return path\n"
		"    end,\n"
		"    ensure_session_marker = function (session, name, position)\n"
		"      table.insert (created_markers, { name = name, position = position })\n"
		"      return true\n"
		"    end,\n"
		"    ensure_session_midi_region = function (session, route_name, region_name, position, length)\n"
		"      table.insert (created_regions, { route_name = route_name, region_name = region_name, position = position, length = length })\n"
		"      return true\n"
		"    end,\n"
		"    ensure_session_midi_trigger_region = function (session, route_name, slot, region_name, length)\n"
		"      table.insert (created_trigger_regions, { route_name = route_name, slot = slot, region_name = region_name, length = length })\n"
		"      return true\n"
		"    end,\n"
		"    ensure_session_midi_trigger_region_with_note = function (session, route_name, slot, region_name, length, note_start, note_length, channel, note, velocity)\n"
		"      table.insert (created_trigger_regions, { route_name = route_name, slot = slot, region_name = region_name, length = length })\n"
		"      table.insert (created_trigger_notes, { route_name = route_name, slot = slot, region_name = region_name, length = length, note_start = note_start, note_length = note_length, channel = channel, note = note, velocity = velocity })\n"
		"      return true\n"
		"    end\n"
		"  },\n"
		"  DataType = function (name) return { name = name } end,\n"
		"  ChanCount = function (data_type, count) return { data_type = data_type, count = count } end,\n"
		"  PluginInfo = function () return {} end,\n"
		"  RouteGroup = function () return {} end,\n"
		"  PresentationInfo = { max_order = 0 },\n"
		"  TrackMode = { Normal = 0 }\n"
		"}\n"
		"Temporal = {\n"
		"  Beats = { from_double = function (beats) return { beats = function () return beats end } end },\n"
		"  timepos_t = function (samples)\n"
		"    return { samples = function () return samples end }\n"
		"  end,\n"
		"  timecnt_t = function (samples)\n"
		"    return { samples = function () return samples end }\n"
		"  end\n"
		"}\n"
		"Session = {\n"
		"  path = function () return " + lua_literal (session_path) + " end,\n"
		"  name = function () return 'Reactive Template Test' end,\n"
		"  nominal_sample_rate = function () return 48000 end,\n"
		"  new_midi_track = function (...)\n"
		"    local name = select (9, ...)\n"
		"    local trigger_visible = select (13, ...)\n"
		"    table.insert (created_tracks, { name = name, trigger_visible = trigger_visible })\n"
		"  end,\n"
		"  save_state = function (...) saved = true end\n"
		"}\n");

	if (!file_io_available) {
		lua.do_command ("io = nil");
	}

	int const load_type = lua.do_file (reactive_template_path ());
	CPPUNIT_ASSERT_EQUAL (0, load_type);
	int const run_type = lua.do_command ("factory () ()");
	CPPUNIT_ASSERT_EQUAL (0, run_type);

	int const track_count_type = lua.do_command ("assert (#created_tracks == 3, 'expected three MIDI tracks')");
	CPPUNIT_ASSERT_EQUAL (0, track_count_type);
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_tracks[1].name == 'Reactive Rhythm Lane', 'expected rhythm lane')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_tracks[2].name == 'Reactive Harmony Lane', 'expected harmony lane')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_tracks[3].name == 'Reactive Macro Lane', 'expected macro lane')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_tracks[1].trigger_visible == true, 'expected rhythm lane to be trigger visible')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_tracks[2].trigger_visible == true, 'expected harmony lane to be trigger visible')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_tracks[3].trigger_visible == true, 'expected macro lane to be trigger visible')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (#created_markers == 1, 'expected one seeded marker')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_markers[1].name == 'Breakdown', 'expected Breakdown marker')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_markers[1].position:samples () > 0, 'expected positive marker position')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (#created_regions == 1, 'expected one seeded region landmark')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_regions[1].route_name == 'Reactive Rhythm Lane', 'expected region on rhythm lane')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_regions[1].region_name == 'Breakdown Loop', 'expected Breakdown Loop region')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_regions[1].position:samples () > 0, 'expected positive region position')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_regions[1].length:samples () > 0, 'expected positive region length')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (#created_trigger_regions == 6, 'expected six seeded trigger clip regions')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_regions[1].route_name == 'Reactive Rhythm Lane', 'expected first trigger on rhythm lane')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_regions[1].slot == 0, 'expected first trigger in slot 0')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_regions[1].region_name == 'Reactive Cue 0 Reset', 'expected cue 0 reset clip')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_regions[4].route_name == 'Reactive Harmony Lane', 'expected fourth trigger on harmony lane')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_regions[4].slot == 0, 'expected harmony trigger in slot 0')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_regions[6].route_name == 'Reactive Macro Lane', 'expected final trigger on macro lane')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_regions[6].slot == 1, 'expected final macro trigger in slot 1')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_regions[6].length:samples () > 0, 'expected positive trigger clip length')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (#created_trigger_notes == 6, 'expected six seeded trigger clip notes')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[1].route_name == 'Reactive Rhythm Lane', 'expected first note on rhythm lane')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[1].slot == 0, 'expected first note in slot 0')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[1].region_name == 'Reactive Cue 0 Reset', 'expected first note in cue 0 reset clip')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[1].note_start:beats () == 0, 'expected first note at beat zero')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[1].note_length:beats () > 0, 'expected positive note length')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[1].channel == 0, 'expected zero-based MIDI channel')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[1].note == 36, 'expected reset clip root note')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[1].velocity > 0, 'expected positive note velocity')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[4].region_name == 'Reactive Harmony i', 'expected harmony clip note')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[4].note == 48, 'expected harmony clip root note')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[6].region_name == 'Reactive Macro Texture', 'expected texture clip note')"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (created_trigger_notes[6].note == 67, 'expected texture clip note')"));
	int const save_type = lua.do_command ("assert (saved == true, 'expected template to save session')");
	CPPUNIT_ASSERT_EQUAL (0, save_type);
}

} // anonymous namespace

void
LuaScriptTest::session_script_test ()
{
	LuaScriptList scripts (LuaScripting::instance ().scripts (LuaScriptInfo::Session));
	printf("\n * Testing %ld Lua session scripts\n", scripts.size());

	for (LuaScriptList::const_iterator s = scripts.begin(); s != scripts.end(); ++s) {
		const LuaScriptInfoPtr& spi (*s);

		std::string script = "";

		if (Glib::path_get_basename (spi->path).find ("__") == 0) {
			continue;
		}

		if (Glib::path_get_basename (spi->path).at(0) == '_') {
			std::cout << "LuaSession: " << spi->name << " (not bundled)\n";
		} else {
			std::cout << "LuaSession: " << spi->name << "\n";
		}

		try {
			script = Glib::file_get_contents (spi->path);
		} catch (Glib::FileError const& e) {
			CPPUNIT_FAIL (spi->name + ": Cannot read script file");
			continue;
		}

		try {
			LuaScriptParamList lsp = LuaScriptParams::script_params (spi, "sess_params");
			_session->register_lua_function ("test", script, lsp);
		} catch (...) {
			CPPUNIT_FAIL (spi->name + ": Cannot add script to session");
			continue;
		}
		CPPUNIT_ASSERT_MESSAGE (spi->name, !_session->registered_lua_functions ().empty());
		Glib::usleep(200000); // wait to script to execute during process()
		// if the script fails, it'll be removed.
		CPPUNIT_ASSERT_MESSAGE (spi->name, !_session->registered_lua_functions ().empty());
		_session->unregister_lua_function ("test");
		CPPUNIT_ASSERT_MESSAGE (spi->name, _session->registered_lua_functions ().empty());
	}
}

void
LuaScriptTest::reactive_performance_session_init_script_test ()
{
	LuaScriptInfoPtr reactive_template;
	LuaScriptList scripts (LuaScripting::instance ().scripts (LuaScriptInfo::SessionInit));

	for (LuaScriptList::const_iterator s = scripts.begin(); s != scripts.end(); ++s) {
		if ((*s)->name == "Reactive Performance MVP") {
			reactive_template = *s;
			break;
		}
	}

	CPPUNIT_ASSERT_MESSAGE ("Reactive Performance MVP SessionInit script was not discoverable", reactive_template);

	std::string script;
	try {
		script = Glib::file_get_contents (reactive_template->path);
	} catch (Glib::FileError const&) {
		CPPUNIT_FAIL ("Reactive Performance MVP SessionInit script could not be read");
	}

	CPPUNIT_ASSERT_MESSAGE (
		"Reactive Performance MVP SessionInit factory did not compile",
		LuaScripting::try_compile (script, LuaScriptParamList ()));
}

void
LuaScriptTest::reactive_performance_session_init_installs_demo_action_document_test ()
{
	TemporaryDirectory session_dir ("reactive-template-session-XXXXXX");

	run_reactive_template_with_fake_session (session_dir.path ());

	std::string const generated_path = Glib::build_filename (session_dir.path (), "reactive-actions.txt");
	CPPUNIT_ASSERT_MESSAGE ("Reactive Performance MVP template did not install reactive-actions.txt", Glib::file_test (generated_path, Glib::FILE_TEST_IS_REGULAR));

	std::string const generated = Glib::file_get_contents (generated_path);
	std::string const example = Glib::file_get_contents (Glib::build_filename ("examples", "reactive-performance-mvp", "reactive-actions.txt"));
	CPPUNIT_ASSERT_EQUAL (example, generated);
}

void
LuaScriptTest::reactive_performance_session_init_keeps_existing_action_document_test ()
{
	TemporaryDirectory session_dir ("reactive-template-session-XXXXXX");
	std::string const generated_path = Glib::build_filename (session_dir.path (), "reactive-actions.txt");
	std::string const existing = "ACTION custom.keep\nDO state preserved yes\nEND\n";
	write_file (generated_path, existing);

	run_reactive_template_with_fake_session (session_dir.path ());

	CPPUNIT_ASSERT_EQUAL (existing, Glib::file_get_contents (generated_path));
}

void
LuaScriptTest::reactive_performance_session_init_tolerates_unavailable_file_io_test ()
{
	TemporaryDirectory session_dir ("reactive-template-session-XXXXXX");

	run_reactive_template_with_fake_session (session_dir.path (), false);

	std::string const generated_path = Glib::build_filename (session_dir.path (), "reactive-actions.txt");
	CPPUNIT_ASSERT_EQUAL (false, Glib::file_test (generated_path, Glib::FILE_TEST_EXISTS));
}

void
LuaScriptTest::reactive_performance_lua_api_ensures_session_marker_test ()
{
	LuaState lua (false, false);
	LuaBindings::stddef (lua.getState ());
	LuaBindings::common (lua.getState ());
	LuaBindings::non_rt (lua.getState ());
	LuaBindings::set_session (lua.getState (), _session);

	CPPUNIT_ASSERT_EQUAL (size_t (0), count_named_markers (*_session, "Breakdown"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_marker (Session, 'Breakdown', Temporal.timepos_t (48000)) == true)"));
	CPPUNIT_ASSERT_EQUAL (size_t (1), count_named_markers (*_session, "Breakdown"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_marker (Session, 'Breakdown', Temporal.timepos_t (96000)) == true)"));
	CPPUNIT_ASSERT_EQUAL (size_t (1), count_named_markers (*_session, "Breakdown"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_marker (Session, '', Temporal.timepos_t (48000)) == false)"));
}

void
LuaScriptTest::reactive_performance_lua_api_ensures_midi_region_test ()
{
	LuaState lua (false, false);
	LuaBindings::stddef (lua.getState ());
	LuaBindings::common (lua.getState ());
	LuaBindings::non_rt (lua.getState ());
	LuaBindings::set_session (lua.getState (), _session);

	std::list<std::shared_ptr<MidiTrack> > tracks = _session->new_midi_track (
		ChanCount (DataType::MIDI, 1),
		ChanCount (DataType::MIDI, 1),
		false,
		PluginInfoPtr (),
		nullptr,
		std::shared_ptr<RouteGroup> (),
		1,
		"Reactive Rhythm Lane",
		PresentationInfo::max_order,
		Normal,
		false,
		true);
	CPPUNIT_ASSERT_EQUAL (size_t (1), tracks.size ());

	CPPUNIT_ASSERT_EQUAL (size_t (0), count_named_regions_on_route (*_session, "Reactive Rhythm Lane", "Breakdown Loop"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_region (Session, 'Reactive Rhythm Lane', 'Breakdown Loop', Temporal.timepos_t (48000 * 16), Temporal.timecnt_t (48000 * 4)) == true)"));
	CPPUNIT_ASSERT_EQUAL (size_t (1), count_named_regions_on_route (*_session, "Reactive Rhythm Lane", "Breakdown Loop"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_region (Session, 'Reactive Rhythm Lane', 'Breakdown Loop', Temporal.timepos_t (48000 * 20), Temporal.timecnt_t (48000 * 4)) == true)"));
	CPPUNIT_ASSERT_EQUAL (size_t (1), count_named_regions_on_route (*_session, "Reactive Rhythm Lane", "Breakdown Loop"));
	CPPUNIT_ASSERT (!ARDOUR::LuaAPI::ensure_session_midi_region (nullptr, "Reactive Rhythm Lane", "Breakdown Loop", Temporal::timepos_t (48000), Temporal::timecnt_t (48000)));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_region (Session, '', 'Missing Route Name', Temporal.timepos_t (48000), Temporal.timecnt_t (48000)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_region (Session, 'Reactive Rhythm Lane', '', Temporal.timepos_t (48000), Temporal.timecnt_t (48000)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_region (Session, 'Reactive Rhythm Lane', 'Bad Position', Temporal.timepos_t (-1), Temporal.timecnt_t (48000)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_region (Session, 'Reactive Rhythm Lane', 'Bad Length', Temporal.timepos_t (48000), Temporal.timecnt_t (0)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_region (Session, 'Missing Lane', 'Missing Loop', Temporal.timepos_t (48000), Temporal.timecnt_t (48000)) == false)"));

	AudioTrackList audio_tracks = _session->new_audio_track (
		1,
		1,
		std::shared_ptr<RouteGroup> (),
		1,
		"Audio Lane",
		PresentationInfo::max_order,
		Normal,
		false);
	CPPUNIT_ASSERT_EQUAL (size_t (1), audio_tracks.size ());
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_region (Session, 'Audio Lane', 'Audio Region Rejected', Temporal.timepos_t (48000), Temporal.timecnt_t (48000)) == false)"));
}

void
LuaScriptTest::reactive_performance_lua_api_ensures_midi_trigger_region_test ()
{
	LuaState lua (false, false);
	LuaBindings::stddef (lua.getState ());
	LuaBindings::common (lua.getState ());
	LuaBindings::non_rt (lua.getState ());
	LuaBindings::set_session (lua.getState (), _session);

	std::list<std::shared_ptr<MidiTrack> > tracks = _session->new_midi_track (
		ChanCount (DataType::MIDI, 1),
		ChanCount (DataType::MIDI, 1),
		false,
		PluginInfoPtr (),
		nullptr,
		std::shared_ptr<RouteGroup> (),
		1,
		"Reactive Rhythm Lane",
		PresentationInfo::max_order,
		Normal,
		false,
		true);
	CPPUNIT_ASSERT_EQUAL (size_t (1), tracks.size ());

	CPPUNIT_ASSERT_EQUAL (size_t (0), count_named_trigger_regions_on_route (*_session, "Reactive Rhythm Lane", "Reactive Cue 0 Reset"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, 'Reactive Rhythm Lane', 0, 'Reactive Cue 0 Reset', Temporal.timecnt_t (48000 * 4), Temporal.Beats.from_double (0), Temporal.Beats.from_double (1), 0, 36, 100) == true)"));
	CPPUNIT_ASSERT_EQUAL (size_t (1), count_named_trigger_regions_on_route (*_session, "Reactive Rhythm Lane", "Reactive Cue 0 Reset"));
	CPPUNIT_ASSERT_EQUAL (size_t (1), count_notes_in_named_trigger_region (*_session, "Reactive Rhythm Lane", "Reactive Cue 0 Reset"));
	CPPUNIT_ASSERT_EQUAL (36, first_note_number_in_named_trigger_region (*_session, "Reactive Rhythm Lane", "Reactive Cue 0 Reset"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, 'Reactive Rhythm Lane', 0, 'Reactive Cue 0 Reset', Temporal.timecnt_t (48000 * 8), Temporal.Beats.from_double (0), Temporal.Beats.from_double (1), 0, 40, 100) == true)"));
	CPPUNIT_ASSERT_EQUAL (size_t (1), count_named_trigger_regions_on_route (*_session, "Reactive Rhythm Lane", "Reactive Cue 0 Reset"));
	CPPUNIT_ASSERT_EQUAL (size_t (1), count_notes_in_named_trigger_region (*_session, "Reactive Rhythm Lane", "Reactive Cue 0 Reset"));
	CPPUNIT_ASSERT_EQUAL (36, first_note_number_in_named_trigger_region (*_session, "Reactive Rhythm Lane", "Reactive Cue 0 Reset"));

	TriggerPtr trigger = tracks.front ()->triggerbox ()->trigger (0);
	CPPUNIT_ASSERT (trigger);
	CPPUNIT_ASSERT (trigger->the_region ());
	CPPUNIT_ASSERT_EQUAL (std::string ("Reactive Cue 0 Reset"), trigger->the_region ()->name ());
	CPPUNIT_ASSERT (trigger->playable ());
	CPPUNIT_ASSERT (!tracks.front ()->triggerbox ()->empty ());

	CPPUNIT_ASSERT (!ARDOUR::LuaAPI::ensure_session_midi_trigger_region (nullptr, "Reactive Rhythm Lane", 0, "Reactive Cue 0 Reset", Temporal::timecnt_t (48000)));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region (Session, '', 0, 'Missing Route Name', Temporal.timecnt_t (48000)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region (Session, 'Reactive Rhythm Lane', 0, '', Temporal.timecnt_t (48000)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region (Session, 'Reactive Rhythm Lane', -1, 'Bad Slot', Temporal.timecnt_t (48000)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region (Session, 'Reactive Rhythm Lane', 999, 'Bad Slot', Temporal.timecnt_t (48000)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region (Session, 'Reactive Rhythm Lane', 1, 'Bad Length', Temporal.timecnt_t (0)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region (Session, 'Missing Lane', 1, 'Missing Clip', Temporal.timecnt_t (48000)) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, 'Reactive Rhythm Lane', 1, 'Bad Note Start', Temporal.timecnt_t (48000), Temporal.Beats.from_double (-1), Temporal.Beats.from_double (1), 0, 36, 100) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, 'Reactive Rhythm Lane', 1, 'Bad Note Length', Temporal.timecnt_t (48000), Temporal.Beats.from_double (0), Temporal.Beats.from_double (0), 0, 36, 100) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, 'Reactive Rhythm Lane', 1, 'Bad Channel', Temporal.timecnt_t (48000), Temporal.Beats.from_double (0), Temporal.Beats.from_double (1), 16, 36, 100) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, 'Reactive Rhythm Lane', 1, 'Bad Note Number', Temporal.timecnt_t (48000), Temporal.Beats.from_double (0), Temporal.Beats.from_double (1), 0, 128, 100) == false)"));
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region_with_note (Session, 'Reactive Rhythm Lane', 1, 'Bad Velocity', Temporal.timecnt_t (48000), Temporal.Beats.from_double (0), Temporal.Beats.from_double (1), 0, 36, 0) == false)"));

	AudioTrackList audio_tracks = _session->new_audio_track (
		1,
		1,
		std::shared_ptr<RouteGroup> (),
		1,
		"Audio Lane",
		PresentationInfo::max_order,
		Normal,
		false);
	CPPUNIT_ASSERT_EQUAL (size_t (1), audio_tracks.size ());
	CPPUNIT_ASSERT_EQUAL (0, lua.do_command ("assert (ARDOUR.LuaAPI.ensure_session_midi_trigger_region (Session, 'Audio Lane', 0, 'Audio Trigger Rejected', Temporal.timecnt_t (48000)) == false)"));
}

void
LuaScriptTest::dsp_script_test ()
{
	PluginManager& pm = PluginManager::instance ();
	AudioTrackList tracks;

	tracks = _session->new_audio_track (2, 2, NULL, 1, "", PresentationInfo::max_order);
	CPPUNIT_ASSERT (tracks.size() == 1);
	std::shared_ptr<Route> r = tracks.front ();

	std::cout << "\n";
	const PluginInfoList& plugs = pm.lua_plugin_info();
	for (PluginInfoList::const_iterator i = plugs.begin(); i != plugs.end(); ++i) {

		if (Glib::path_get_basename ((*i)->path).find ("__") == 0) {
			/* Example scripts (filename with leading underscore), that
			 * use a double-underscore at the beginning of the file-name
			 * are excluded from unit-tests (e.g. "Lua Convolver"
			 * requires IR files).
			 */
			continue;
		}

		if (Glib::path_get_basename ((*i)->path).at(0) == '_') {
			std::cout << "LuaProc: " <<(*i)->name << " (not bundled)\n";
		} else {
			std::cout << "LuaProc: " <<(*i)->name << "\n";
		}

		PluginPtr p = (*i)->load (*_session);
		CPPUNIT_ASSERT_MESSAGE ((*i)->name, p);

		std::shared_ptr<Processor> processor (new PluginInsert (*_session, Temporal::TimeDomainProvider (r->time_domain()), p));
		processor->enable (true);

		int rv = r->add_processor (processor, std::shared_ptr<Processor>(), 0);
		CPPUNIT_ASSERT_MESSAGE ((*i)->name, rv == 0);
		processor->enable (true);
		Glib::usleep(200000); // run process, failing plugins will be deactivated.
		CPPUNIT_ASSERT_MESSAGE ((*i)->name, processor->active());
		rv = r->remove_processor (processor, NULL, true);
		CPPUNIT_ASSERT_MESSAGE ((*i)->name, rv == 0);
	}
}

void
LuaScriptTest::reactive_rhythm_luaproc_script_test ()
{
	PluginManager& pm = PluginManager::instance ();
	AudioTrackList tracks = _session->new_audio_track (2, 2, NULL, 1, "", PresentationInfo::max_order);
	CPPUNIT_ASSERT (tracks.size() == 1);
	std::shared_ptr<Route> r = tracks.front ();

	PluginInfoPtr reactive_rhythm_info;
	const PluginInfoList& plugs = pm.lua_plugin_info();
	for (PluginInfoList::const_iterator i = plugs.begin(); i != plugs.end(); ++i) {
		if ((*i)->name == "Reactive Rhythm State MVP") {
			reactive_rhythm_info = *i;
			break;
		}
	}

	CPPUNIT_ASSERT_MESSAGE ("Reactive Rhythm State MVP LuaProc script was not discoverable", reactive_rhythm_info);

	PluginPtr p = reactive_rhythm_info->load (*_session);
	CPPUNIT_ASSERT_MESSAGE (reactive_rhythm_info->name, p);

	std::shared_ptr<Processor> processor (new PluginInsert (*_session, Temporal::TimeDomainProvider (r->time_domain()), p));
	processor->enable (true);

	int rv = r->add_processor (processor, std::shared_ptr<Processor>(), 0);
	CPPUNIT_ASSERT_MESSAGE (reactive_rhythm_info->name, rv == 0);
	processor->enable (true);
	Glib::usleep(200000);
	CPPUNIT_ASSERT_MESSAGE (reactive_rhythm_info->name, processor->active());
	rv = r->remove_processor (processor, NULL, true);
	CPPUNIT_ASSERT_MESSAGE (reactive_rhythm_info->name, rv == 0);
}
