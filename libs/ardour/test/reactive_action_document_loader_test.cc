#include "reactive_action_document_loader_test.h"

#include "ardour/reactive_action_document_loader.h"
#include "ardour/reactive_action_slot_runner.h"

#include <fstream>
#include <string>

#include <glib.h>
#include <glibmm/miscutils.h>

#include "pbd/gstdio_compat.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReactiveActionDocumentLoaderTest);

using namespace ARDOUR;

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
			g_remove (Glib::build_filename (_path, ReactiveActionDocumentLoader::document_filename ()).c_str ());
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
action_source (std::string const& name, int cue)
{
	return
		"ACTION " + name + "\n"
		"DO cue " + std::to_string (cue) + "\n"
		"END\n";
}

static void
write_file (std::string const& path, std::string const& content)
{
	std::ofstream out (path.c_str ());
	CPPUNIT_ASSERT (out.good ());
	out << content;
	out.close ();
	CPPUNIT_ASSERT (out.good ());
}

} // namespace

void
ReactiveActionDocumentLoaderTest::preferSessionDocumentOverUserDocument ()
{
	TemporaryDirectory session_dir ("reactive-session-XXXXXX");
	TemporaryDirectory user_dir ("reactive-user-XXXXXX");
	ReactiveActionSlotRunner runner;
	ReactiveActionDocumentLoadResult result;

	write_file (ReactiveActionDocumentLoader::session_document_path (session_dir.path ()), action_source ("session.first", 1));
	write_file (ReactiveActionDocumentLoader::user_document_path (user_dir.path ()), action_source ("user.first", 2));

	CPPUNIT_ASSERT_EQUAL (true, ReactiveActionDocumentLoader::load_from_paths (
		runner,
		session_dir.path (),
		user_dir.path (),
		action_source ("fallback.first", 7),
		result));

	CPPUNIT_ASSERT_EQUAL (std::string ("session"), result.source);
	CPPUNIT_ASSERT_EQUAL (ReactiveActionDocumentLoader::session_document_path (session_dir.path ()), result.path);
	CPPUNIT_ASSERT_EQUAL (false, result.used_fallback);
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.action_count ());
	CPPUNIT_ASSERT_EQUAL (std::string ("session.first"), runner.action_name (0));
}

void
ReactiveActionDocumentLoaderTest::loadUserDocumentWhenSessionDocumentMissing ()
{
	TemporaryDirectory session_dir ("reactive-session-XXXXXX");
	TemporaryDirectory user_dir ("reactive-user-XXXXXX");
	ReactiveActionSlotRunner runner;
	ReactiveActionDocumentLoadResult result;

	write_file (ReactiveActionDocumentLoader::user_document_path (user_dir.path ()), action_source ("user.first", 2));

	CPPUNIT_ASSERT_EQUAL (true, ReactiveActionDocumentLoader::load_from_paths (
		runner,
		session_dir.path (),
		user_dir.path (),
		action_source ("fallback.first", 7),
		result));

	CPPUNIT_ASSERT_EQUAL (std::string ("user"), result.source);
	CPPUNIT_ASSERT_EQUAL (ReactiveActionDocumentLoader::user_document_path (user_dir.path ()), result.path);
	CPPUNIT_ASSERT_EQUAL (false, result.used_fallback);
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.action_count ());
	CPPUNIT_ASSERT_EQUAL (std::string ("user.first"), runner.action_name (0));
}

void
ReactiveActionDocumentLoaderTest::useFallbackSeedWhenNoDocumentFileExists ()
{
	TemporaryDirectory session_dir ("reactive-session-XXXXXX");
	TemporaryDirectory user_dir ("reactive-user-XXXXXX");
	ReactiveActionSlotRunner runner;
	ReactiveActionDocumentLoadResult result;

	CPPUNIT_ASSERT_EQUAL (true, ReactiveActionDocumentLoader::load_from_paths (
		runner,
		session_dir.path (),
		user_dir.path (),
		action_source ("fallback.first", 7),
		result));

	CPPUNIT_ASSERT_EQUAL (std::string ("fallback"), result.source);
	CPPUNIT_ASSERT (result.path.empty ());
	CPPUNIT_ASSERT_EQUAL (true, result.used_fallback);
	CPPUNIT_ASSERT_EQUAL (size_t (1), runner.action_count ());
	CPPUNIT_ASSERT_EQUAL (std::string ("fallback.first"), runner.action_name (0));
}

void
ReactiveActionDocumentLoaderTest::reportConfiguredDocumentParseFailureWithoutFallback ()
{
	TemporaryDirectory session_dir ("reactive-session-XXXXXX");
	TemporaryDirectory user_dir ("reactive-user-XXXXXX");
	ReactiveActionSlotRunner runner;
	ReactiveActionDocumentLoadResult result;

	CPPUNIT_ASSERT_EQUAL (true, ReactiveActionDocumentLoader::load_from_paths (
		runner,
		session_dir.path (),
		user_dir.path (),
		action_source ("fallback.first", 7),
		result));
	CPPUNIT_ASSERT_EQUAL (std::string ("fallback.first"), runner.action_name (0));

	write_file (
		ReactiveActionDocumentLoader::session_document_path (session_dir.path ()),
		"ACTION broken\n"
		"DO warp now\n"
		"END\n");

	CPPUNIT_ASSERT_EQUAL (false, ReactiveActionDocumentLoader::load_from_paths (
		runner,
		session_dir.path (),
		user_dir.path (),
		action_source ("fallback.first", 7),
		result));

	CPPUNIT_ASSERT_EQUAL (std::string ("session"), result.source);
	CPPUNIT_ASSERT_EQUAL (ReactiveActionDocumentLoader::session_document_path (session_dir.path ()), result.path);
	CPPUNIT_ASSERT_EQUAL (false, result.used_fallback);
	CPPUNIT_ASSERT (result.error.find ("unknown command") != std::string::npos);
	CPPUNIT_ASSERT (result.error.find (result.path) != std::string::npos);
	CPPUNIT_ASSERT_EQUAL (false, runner.loaded ());
}
