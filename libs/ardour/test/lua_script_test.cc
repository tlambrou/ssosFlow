#include <list>
#include <fstream>
#include <glibmm.h>

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/location.h"
#include "ardour/luabindings.h"
#include "ardour/luascripting.h"
#include "ardour/lua_script_params.h"
#include "ardour/plugin_manager.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"

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
		"  timepos_t = function (samples)\n"
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
