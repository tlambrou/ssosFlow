#pragma once

#include <memory>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class PluginInsert;
class Processor;
class Route;
class Session;

enum class ReactiveRhythmRouteInsertionStatus {
	Inserted,
	AlreadyPresent,
	MissingLuaProc,
	PluginLoadFailed,
	ConfigureFailed,
	AddFailed,
	InvalidRoute
};

struct LIBARDOUR_API ReactiveRhythmRouteInsertionResult {
	ReactiveRhythmRouteInsertionStatus status = ReactiveRhythmRouteInsertionStatus::InvalidRoute;
	std::shared_ptr<PluginInsert> insert;
	int route_result = 0;

	bool ok () const;
};

class LIBARDOUR_API ReactiveRhythmRouteInserter
{
public:
	static const char* lua_proc_name ();
	static bool is_reactive_rhythm_insert (std::shared_ptr<Processor> const&);
	static ReactiveRhythmRouteInsertionResult ensure_inserted (Session&, std::shared_ptr<Route> const&);
};

} // namespace ARDOUR
