#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "temporal/bbt_time.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

enum class ReactiveChainMode {
	All,
	Sequential,
	Random
};

struct LIBARDOUR_API ReactiveTrigger {
	enum Type {
		None,
		MidiNote,
		MidiCC,
		Marker
	};

	Type type = None;
	int channel = 0;
	int number = 0;
	int threshold = -1;
	std::string name;
};

struct LIBARDOUR_API ReactiveCommand {
	enum Type {
		Cue,
		Trigger,
		TriggerStop,
		StopAll,
		TransportPlay,
		TransportStop,
		SceneApply,
		SceneStore,
		Macro,
		MacroSnapshotStore,
		MacroSnapshotRecall,
		State,
		Rhythm,
		RhythmRoute,
		RhythmInsert
	};

	enum ValueSource {
		LiteralValue,
		MidiEventValue
	};

	Type type = Cue;
	std::string name;
	std::string text;
	double value = 0.0;
	ValueSource value_source = LiteralValue;
	int first = 0;
	int second = 0;
	Temporal::BBT_Offset ramp;
};

struct LIBARDOUR_API ReactiveAction {
	std::string name;
	ReactiveChainMode chain_mode = ReactiveChainMode::All;
	Temporal::BBT_Offset quantize;
	std::vector<ReactiveTrigger> triggers;
	std::vector<ReactiveCommand> commands;
};

struct ReactiveActionParseResult;

class LIBARDOUR_API ReactiveActionDocument {
public:
	static ReactiveActionParseResult parse (std::string const&);

	std::vector<ReactiveAction> const& actions () const { return _actions; }
	ReactiveAction const* action_by_name (std::string const&) const;

private:
	std::vector<ReactiveAction> _actions;
	std::map<std::string, size_t> _index;

	friend struct ReactiveActionParseResult;
};

struct LIBARDOUR_API ReactiveActionParseResult {
	bool ok = false;
	std::string error;
	ReactiveActionDocument document;
};

} // namespace ARDOUR
