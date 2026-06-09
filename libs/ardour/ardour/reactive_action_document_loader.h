#pragma once

#include <string>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class ReactiveActionSlotRunner;

struct LIBARDOUR_API ReactiveActionDocumentLoadResult {
	bool ok = false;
	bool used_fallback = false;
	std::string source;
	std::string path;
	std::string error;
};

class LIBARDOUR_API ReactiveActionDocumentLoader {
public:
	static const char* document_filename ();
	static std::string session_document_path (std::string const& session_directory);
	static std::string user_document_path (std::string const& user_config_directory);
	static std::string describe_load_result (ReactiveActionDocumentLoadResult const&);

	static bool load_from_paths (
		ReactiveActionSlotRunner& runner,
		std::string const& session_directory,
		std::string const& user_config_directory,
		std::string const& fallback_source,
		ReactiveActionDocumentLoadResult& result);
};

} // namespace ARDOUR
