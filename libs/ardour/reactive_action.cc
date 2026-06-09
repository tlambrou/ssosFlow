#include "ardour/reactive_action.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

using namespace ARDOUR;

namespace {

static std::string
trim (std::string const& text)
{
	std::string::size_type first = text.find_first_not_of (" \t\r\n");
	if (first == std::string::npos) {
		return std::string ();
	}

	std::string::size_type last = text.find_last_not_of (" \t\r\n");
	return text.substr (first, last - first + 1);
}

static std::vector<std::string>
split (std::string const& line)
{
	std::istringstream input (line);
	std::vector<std::string> tokens;
	std::string token;

	while (input >> token) {
		tokens.push_back (token);
	}

	return tokens;
}

static bool
starts_with (std::string const& text, std::string const& prefix)
{
	return text.compare (0, prefix.size (), prefix) == 0;
}

static bool
parse_int (std::string const& token, int& value)
{
	if (token.empty ()) {
		return false;
	}

	char* end = 0;
	errno = 0;
	long parsed = std::strtol (token.c_str (), &end, 10);
	if (errno || !end || *end || parsed < std::numeric_limits<int>::min () || parsed > std::numeric_limits<int>::max ()) {
		return false;
	}

	value = static_cast<int> (parsed);
	return true;
}

static bool
parse_nonnegative_int (std::string const& token, int& value)
{
	return parse_int (token, value) && value >= 0;
}

static bool
parse_double (std::string const& token, double& value)
{
	if (token.empty ()) {
		return false;
	}

	char* end = 0;
	errno = 0;
	double parsed = std::strtod (token.c_str (), &end);
	if (errno || !end || *end) {
		return false;
	}

	value = parsed;
	return true;
}

static bool
parse_bbt_offset (std::string const& token, Temporal::BBT_Offset& offset)
{
	std::string::size_type first = token.find ('|');
	if (first == std::string::npos) {
		return false;
	}

	std::string::size_type second = token.find ('|', first + 1);
	if (second == std::string::npos || token.find ('|', second + 1) != std::string::npos) {
		return false;
	}

	int bars = 0;
	int beats = 0;
	int ticks = 0;

	if (!parse_nonnegative_int (token.substr (0, first), bars) ||
	    !parse_nonnegative_int (token.substr (first + 1, second - first - 1), beats) ||
	    !parse_nonnegative_int (token.substr (second + 1), ticks)) {
		return false;
	}

	offset = Temporal::BBT_Offset (bars, beats, ticks);
	return true;
}

static std::string
line_error (size_t line_number, std::string const& message)
{
	std::ostringstream error;
	error << "line " << line_number << ": " << message;
	return error.str ();
}

static bool
require_action (bool in_action, size_t line_number, std::string const& directive, ReactiveActionParseResult& result)
{
	if (in_action) {
		return true;
	}

	result.error = line_error (line_number, directive + " outside ACTION block");
	return false;
}

static bool
parse_key_value_int (std::string const& token, std::string const& key, int& value)
{
	std::string prefix = key + "=";
	if (!starts_with (token, prefix)) {
		return false;
	}

	return parse_nonnegative_int (token.substr (prefix.size ()), value);
}

static bool
parse_trigger (std::vector<std::string> const& tokens, size_t line_number, ReactiveAction& action, ReactiveActionParseResult& result)
{
	if (tokens.size () < 2) {
		result.error = line_error (line_number, "invalid trigger");
		return false;
	}

	ReactiveTrigger trigger;

	if (tokens[1] == "marker") {
		if (tokens.size () < 3) {
			result.error = line_error (line_number, "invalid marker trigger");
			return false;
		}

		trigger.type = ReactiveTrigger::Marker;
		trigger.name = tokens[2];
		for (size_t i = 3; i < tokens.size (); ++i) {
			trigger.name += " " + tokens[i];
		}
		action.triggers.push_back (trigger);
		return true;
	}

	if (tokens.size () < 5 || tokens[1] != "midi") {
		result.error = line_error (line_number, "invalid trigger");
		return false;
	}

	if (tokens[2] == "note") {
		trigger.type = ReactiveTrigger::MidiNote;
		if (!parse_key_value_int (tokens[3], "ch", trigger.channel) ||
		    !parse_key_value_int (tokens[4], "note", trigger.number)) {
			result.error = line_error (line_number, "invalid midi note trigger");
			return false;
		}
	} else if (tokens[2] == "cc") {
		trigger.type = ReactiveTrigger::MidiCC;
		if (!parse_key_value_int (tokens[3], "ch", trigger.channel) ||
		    !parse_key_value_int (tokens[4], "cc", trigger.number)) {
			result.error = line_error (line_number, "invalid midi cc trigger");
			return false;
		}

		if (tokens.size () > 5) {
			if (!starts_with (tokens[5], "value>") || !parse_nonnegative_int (tokens[5].substr (6), trigger.threshold)) {
				result.error = line_error (line_number, "invalid midi cc threshold");
				return false;
			}
		}
	} else {
		result.error = line_error (line_number, "unknown midi trigger");
		return false;
	}

	action.triggers.push_back (trigger);
	return true;
}

static bool
parse_command (std::vector<std::string> const& tokens, size_t line_number, ReactiveAction& action, ReactiveActionParseResult& result)
{
	if (tokens.size () < 2) {
		result.error = line_error (line_number, "invalid command");
		return false;
	}

	ReactiveCommand command;
	std::string const& name = tokens[1];

	if (name == "cue") {
		if (tokens.size () != 3 || !parse_nonnegative_int (tokens[2], command.first)) {
			result.error = line_error (line_number, "invalid cue command");
			return false;
		}
		command.type = ReactiveCommand::Cue;
	} else if (name == "trigger") {
		if (tokens.size () != 4 || !parse_nonnegative_int (tokens[2], command.first) || !parse_nonnegative_int (tokens[3], command.second)) {
			result.error = line_error (line_number, "invalid trigger command");
			return false;
		}
		command.type = ReactiveCommand::Trigger;
	} else if (name == "trigger-stop") {
		if (tokens.size () != 3 || !parse_nonnegative_int (tokens[2], command.first)) {
			result.error = line_error (line_number, "invalid trigger-stop command");
			return false;
		}
		command.type = ReactiveCommand::TriggerStop;
	} else if (name == "stop-all") {
		if (tokens.size () != 2) {
			result.error = line_error (line_number, "invalid stop-all command");
			return false;
		}
		command.type = ReactiveCommand::StopAll;
	} else if (name == "transport") {
		if (tokens.size () < 3) {
			result.error = line_error (line_number, "invalid transport command");
			return false;
		}
		if (tokens[2] == "play") {
			command.type = ReactiveCommand::TransportPlay;
		} else if (tokens[2] == "stop") {
			command.type = ReactiveCommand::TransportStop;
		} else {
			result.error = line_error (line_number, "unknown transport command");
			return false;
		}
		if (tokens.size () > 3) {
			if (tokens.size () != 5 || tokens[3] != "after" || !parse_bbt_offset (tokens[4], command.ramp)) {
				result.error = line_error (line_number, "invalid transport after offset");
				return false;
			}
		}
	} else if (name == "scene") {
		if (tokens.size () != 4 || !parse_nonnegative_int (tokens[3], command.first)) {
			result.error = line_error (line_number, "invalid scene command");
			return false;
		}
		if (tokens[2] == "apply") {
			command.type = ReactiveCommand::SceneApply;
		} else if (tokens[2] == "store") {
			command.type = ReactiveCommand::SceneStore;
		} else {
			result.error = line_error (line_number, "unknown scene command");
			return false;
		}
	} else if (name == "macro") {
		if (tokens.size () != 4 && tokens.size () != 6) {
			result.error = line_error (line_number, "invalid macro command");
			return false;
		}
		command.type = ReactiveCommand::Macro;
		command.name = tokens[2];
		if (!parse_double (tokens[3], command.value)) {
			result.error = line_error (line_number, "invalid macro value");
			return false;
		}
		if (tokens.size () == 6 && (tokens[4] != "ramp" || !parse_bbt_offset (tokens[5], command.ramp))) {
			result.error = line_error (line_number, "invalid macro ramp");
			return false;
		}
	} else if (name == "state") {
		if (tokens.size () != 4) {
			result.error = line_error (line_number, "invalid state command");
			return false;
		}
		command.type = ReactiveCommand::State;
		command.name = tokens[2];
		command.text = tokens[3];
		parse_double (command.text, command.value);
	} else if (name == "rhythm") {
		if (tokens.size () != 4) {
			result.error = line_error (line_number, "invalid rhythm command");
			return false;
		}
		if (tokens[2] == "insert") {
			if (!parse_nonnegative_int (tokens[3], command.first)) {
				result.error = line_error (line_number, "invalid rhythm insert command");
				return false;
			}
			command.type = ReactiveCommand::RhythmInsert;
		} else {
			if (!parse_double (tokens[3], command.value)) {
				result.error = line_error (line_number, "invalid rhythm command");
				return false;
			}
			command.type = ReactiveCommand::Rhythm;
			command.name = tokens[2];
		}
	} else {
		result.error = line_error (line_number, "unknown command '" + name + "'");
		return false;
	}

	action.commands.push_back (command);
	return true;
}

} // namespace

ReactiveAction const*
ReactiveActionDocument::action_by_name (std::string const& name) const
{
	std::map<std::string, size_t>::const_iterator found = _index.find (name);
	if (found == _index.end ()) {
		return 0;
	}

	return &_actions[found->second];
}

ReactiveActionParseResult
ReactiveActionDocument::parse (std::string const& source)
{
	ReactiveActionParseResult result;
	ReactiveAction current;
	bool in_action = false;
	std::istringstream input (source);
	std::string raw_line;
	size_t line_number = 0;

	while (std::getline (input, raw_line)) {
		++line_number;

		std::string line = trim (raw_line);
		if (line.empty () || line[0] == '#') {
			continue;
		}

		std::vector<std::string> tokens = split (line);
		if (tokens.empty ()) {
			continue;
		}

		if (tokens[0] == "ACTION") {
			if (in_action) {
				result.error = line_error (line_number, "nested ACTION block");
				return result;
			}
			if (tokens.size () != 2) {
				result.error = line_error (line_number, "invalid action declaration");
				return result;
			}
			if (result.document._index.find (tokens[1]) != result.document._index.end ()) {
				result.error = line_error (line_number, "duplicate action '" + tokens[1] + "'");
				return result;
			}

			current = ReactiveAction ();
			current.name = tokens[1];
			in_action = true;
		} else if (tokens[0] == "END") {
			if (!require_action (in_action, line_number, "END", result)) {
				return result;
			}
			if (tokens.size () != 1) {
				result.error = line_error (line_number, "invalid END");
				return result;
			}
			result.document._index[current.name] = result.document._actions.size ();
			result.document._actions.push_back (current);
			current = ReactiveAction ();
			in_action = false;
		} else if (tokens[0] == "QUANTIZE") {
			if (!require_action (in_action, line_number, "QUANTIZE", result)) {
				return result;
			}
			if (tokens.size () != 2 || !parse_bbt_offset (tokens[1], current.quantize)) {
				result.error = line_error (line_number, "invalid quantize value");
				return result;
			}
		} else if (tokens[0] == "TRIGGER") {
			if (!require_action (in_action, line_number, "TRIGGER", result) || !parse_trigger (tokens, line_number, current, result)) {
				return result;
			}
		} else if (tokens[0] == "WHEN") {
			if (!require_action (in_action, line_number, "WHEN", result)) {
				return result;
			}
		} else if (tokens[0] == "CHAIN") {
			if (!require_action (in_action, line_number, "CHAIN", result)) {
				return result;
			}
			if (tokens.size () != 2) {
				result.error = line_error (line_number, "invalid chain mode");
				return result;
			}
			if (tokens[1] == "all") {
				current.chain_mode = ReactiveChainMode::All;
			} else if (tokens[1] == "sequential") {
				current.chain_mode = ReactiveChainMode::Sequential;
			} else if (tokens[1] == "random") {
				current.chain_mode = ReactiveChainMode::Random;
			} else {
				result.error = line_error (line_number, "invalid chain mode");
				return result;
			}
		} else if (tokens[0] == "DO") {
			if (!require_action (in_action, line_number, "DO", result) || !parse_command (tokens, line_number, current, result)) {
				return result;
			}
		} else {
			result.error = line_error (line_number, "unknown directive '" + tokens[0] + "'");
			return result;
		}
	}

	if (in_action) {
		result.error = "unterminated ACTION '" + current.name + "'";
		return result;
	}

	result.ok = true;
	return result;
}
