#include "ardour/reactive_rhythm_route_inserter.h"

#include "ardour/audioengine.h"
#include "ardour/chan_count.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/processor.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include <string>

using namespace ARDOUR;

namespace {

static PluginInfoPtr
reactive_rhythm_info ()
{
	PluginManager& pm = PluginManager::instance ();
	const PluginInfoList& plugins = pm.lua_plugin_info ();
	for (PluginInfoList::const_iterator i = plugins.begin (); i != plugins.end (); ++i) {
		if ((*i)->name == ReactiveRhythmRouteInserter::lua_proc_name ()) {
			return *i;
		}
	}
	return PluginInfoPtr ();
}

static std::shared_ptr<PluginInsert>
existing_reactive_rhythm_insert (std::shared_ptr<Route> const& route)
{
	std::shared_ptr<PluginInsert> found;
	if (!route) {
		return found;
	}

	route->foreach_processor ([&found] (std::weak_ptr<Processor> wp) {
		if (found) {
			return;
		}

		std::shared_ptr<Processor> processor = wp.lock ();
		if (processor && ReactiveRhythmRouteInserter::is_reactive_rhythm_insert (processor)) {
			found = std::dynamic_pointer_cast<PluginInsert> (processor);
		}
	});

	return found;
}

} // namespace

bool
ReactiveRhythmRouteInsertionResult::ok () const
{
	return status == ReactiveRhythmRouteInsertionStatus::Inserted || status == ReactiveRhythmRouteInsertionStatus::AlreadyPresent;
}

const char*
ReactiveRhythmRouteInserter::lua_proc_name ()
{
	return "Reactive Rhythm State MVP";
}

bool
ReactiveRhythmRouteInserter::is_reactive_rhythm_insert (std::shared_ptr<Processor> const& processor)
{
	std::shared_ptr<PluginInsert> insert = std::dynamic_pointer_cast<PluginInsert> (processor);
	if (!insert || !insert->plugin ()) {
		return false;
	}

	return lua_proc_name () == std::string (insert->plugin ()->name ());
}

ReactiveRhythmRouteInsertionResult
ReactiveRhythmRouteInserter::ensure_inserted (Session& session, std::shared_ptr<Route> const& route)
{
	ReactiveRhythmRouteInsertionResult result;

	if (!route) {
		result.status = ReactiveRhythmRouteInsertionStatus::InvalidRoute;
		return result;
	}

	if ((result.insert = existing_reactive_rhythm_insert (route))) {
		result.status = ReactiveRhythmRouteInsertionStatus::AlreadyPresent;
		return result;
	}

	PluginInfoPtr info = reactive_rhythm_info ();
	if (!info) {
		result.status = ReactiveRhythmRouteInsertionStatus::MissingLuaProc;
		return result;
	}

	PluginPtr plugin = info->load (session);
	if (!plugin) {
		result.status = ReactiveRhythmRouteInsertionStatus::PluginLoadFailed;
		return result;
	}

	std::shared_ptr<PluginInsert> insert (new PluginInsert (session, Temporal::TimeDomainProvider (route->time_domain ()), plugin));
	ChanCount midi_io (DataType::MIDI, 1);
	{
		PBD::Mutex::Lock lm (AudioEngine::instance ()->process_lock ());
		if (!insert->configure_io (midi_io, midi_io)) {
			result.status = ReactiveRhythmRouteInsertionStatus::ConfigureFailed;
			return result;
		}
	}

	session.ensure_buffers_unlocked (insert->required_buffers ());
	insert->enable (true);

	result.route_result = route->add_processor (insert, std::shared_ptr<Processor> (), 0);
	if (result.route_result != 0) {
		result.status = ReactiveRhythmRouteInsertionStatus::AddFailed;
		return result;
	}

	insert->enable (true);
	result.status = ReactiveRhythmRouteInsertionStatus::Inserted;
	result.insert = insert;
	return result;
}
