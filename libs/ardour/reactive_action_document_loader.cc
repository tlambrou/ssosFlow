#include "ardour/reactive_action_document_loader.h"

#include "ardour/reactive_action_slot_runner.h"

#include <exception>
#include <string>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/compose.h"

using namespace ARDOUR;
using namespace PBD;

namespace {

static bool
load_configured_file (
	ReactiveActionSlotRunner& runner,
	std::string const& source,
	std::string const& path,
	ReactiveActionDocumentLoadResult& result)
{
	result.ok = false;
	result.used_fallback = false;
	result.source = source;
	result.path = path;
	result.error.clear ();

	if (!Glib::file_test (path, Glib::FILE_TEST_IS_REGULAR)) {
		runner.clear ();
		result.error = string_compose ("%1 is not a regular reactive action document", path);
		return false;
	}

	std::string content;
	try {
		content = Glib::file_get_contents (path);
	} catch (std::exception const& e) {
		runner.clear ();
		result.error = string_compose ("Could not read reactive action document %1: %2", path, e.what ());
		return false;
	}

	std::string error;
	if (!runner.load_source (content, error)) {
		result.error = string_compose ("%1: %2", path, error);
		return false;
	}

	result.ok = true;
	result.error.clear ();
	return true;
}

static bool
maybe_load_configured_file (
	ReactiveActionSlotRunner& runner,
	std::string const& source,
	std::string const& path,
	ReactiveActionDocumentLoadResult& result,
	bool& configured)
{
	configured = false;

	if (path.empty () || !Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	configured = true;
	return load_configured_file (runner, source, path, result);
}

} // namespace

const char*
ReactiveActionDocumentLoader::document_filename ()
{
	return "reactive-actions.txt";
}

std::string
ReactiveActionDocumentLoader::session_document_path (std::string const& session_directory)
{
	return session_directory.empty () ? std::string () : Glib::build_filename (session_directory, document_filename ());
}

std::string
ReactiveActionDocumentLoader::user_document_path (std::string const& user_config_directory)
{
	return user_config_directory.empty () ? std::string () : Glib::build_filename (user_config_directory, document_filename ());
}

std::string
ReactiveActionDocumentLoader::describe_load_result (ReactiveActionDocumentLoadResult const& result)
{
	if (!result.ok) {
		return result.error.empty () ? "Reactive action document load failed" : result.error;
	}

	if (result.used_fallback || result.source == "fallback") {
		return "Loaded built-in Reactive Performance MVP fallback";
	}

	if (!result.path.empty ()) {
		return string_compose ("Loaded %1 reactive action document: %2", result.source, result.path);
	}

	return string_compose ("Loaded %1 reactive action document", result.source);
}

bool
ReactiveActionDocumentLoader::load_from_paths (
	ReactiveActionSlotRunner& runner,
	std::string const& session_directory,
	std::string const& user_config_directory,
	std::string const& fallback_source,
	ReactiveActionDocumentLoadResult& result)
{
	result = ReactiveActionDocumentLoadResult ();

	bool configured = false;
	if (maybe_load_configured_file (runner, "session", session_document_path (session_directory), result, configured)) {
		return true;
	}
	if (configured) {
		return false;
	}

	if (maybe_load_configured_file (runner, "user", user_document_path (user_config_directory), result, configured)) {
		return true;
	}
	if (configured) {
		return false;
	}

	result.source = "fallback";
	result.path.clear ();
	result.used_fallback = true;
	result.error.clear ();

	if (fallback_source.empty ()) {
		runner.clear ();
		result.error = "No reactive action document configured and no fallback source is available";
		return false;
	}

	std::string error;
	if (!runner.load_source (fallback_source, error)) {
		result.error = string_compose ("Built-in reactive action fallback failed: %1", error);
		return false;
	}

	result.ok = true;
	return true;
}
