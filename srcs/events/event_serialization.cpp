#include "events/event_serialization.hpp"
#include "events/event.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace EventSerialization {

const char* eventTypeName(int ordinal) {
	switch (static_cast<EventType>(ordinal)) {
		case EventType::GAME_INITIALIZED:               return "GAME_INITIALIZED";
		case EventType::GAME_ENDED:                     return "GAME_ENDED";
		case EventType::TURN_STARTED:                   return "TURN_STARTED";
		case EventType::TURN_ENDED:                     return "TURN_ENDED";
		case EventType::PHASE_STARTED:                  return "PHASE_STARTED";
		case EventType::PHASE_ENDED:                    return "PHASE_ENDED";
		case EventType::PLAYER_INITIALIZED:             return "PLAYER_INITIALIZED";
		case EventType::FACTION_TURN_ORDER_CALCULATED:  return "FACTION_TURN_ORDER_CALCULATED";
		case EventType::UNITS_SHIPPED:                  return "UNITS_SHIPPED";
		case EventType::UNITS_MOVED:                    return "UNITS_MOVED";
		case EventType::UNITS_BATTLING:                 return "UNITS_BATTLING";
		case EventType::UNITS_KILLED:                   return "UNITS_KILLED";
		case EventType::LEADER_KILLED:                  return "LEADER_KILLED";
		case EventType::LEADER_REVIVED:                 return "LEADER_REVIVED";
		case EventType::BATTLE_STARTED:                 return "BATTLE_STARTED";
		case EventType::BATTLE_RESOLVED:                return "BATTLE_RESOLVED";
		case EventType::BATTLE_LEADER_SELECTED:         return "BATTLE_LEADER_SELECTED";
		case EventType::BATTLE_WHEEL_SELECTED:          return "BATTLE_WHEEL_SELECTED";
		case EventType::BATTLE_VALUE_CALCULATED:        return "BATTLE_VALUE_CALCULATED";
		case EventType::BATTLE_WINNER_DETERMINED:       return "BATTLE_WINNER_DETERMINED";
		case EventType::SPICE_COLLECTED:                return "SPICE_COLLECTED";
		case EventType::SPICE_BLOWN:                    return "SPICE_BLOWN";
		case EventType::SPICE_SPENT:                    return "SPICE_SPENT";
		case EventType::SPICE_CHARITY:                  return "SPICE_CHARITY";
		case EventType::STORM_PLACED:                   return "STORM_PLACED";
		case EventType::STORM_MOVED:                    return "STORM_MOVED";
		case EventType::BIDDING_STARTED:                return "BIDDING_STARTED";
		case EventType::BIDDING_ENDED:                  return "BIDDING_ENDED";
		case EventType::BID_PLACED:                     return "BID_PLACED";
		case EventType::REACTION_WINDOW_OPENED:         return "REACTION_WINDOW_OPENED";
		case EventType::REACTION_WINDOW_CLOSED:         return "REACTION_WINDOW_CLOSED";
		case EventType::DEBUG_INFO:                     return "DEBUG_INFO";
		case EventType::ERROR_EVENT:                    return "ERROR_EVENT";
	}
	return "UNKNOWN";
}

std::string toJson(const Event& event) {
	const char* visibility = (event.visibility == EventVisibility::PrivateToActor)
		? "private_to_actor" : "public";

	json out = {
		{"type",          eventTypeName(static_cast<int>(event.type))},
		{"message",       event.message},
		{"turn",          event.turnNumber},
		{"phase",         event.currentPhase},
		{"player_faction", event.playerFaction},
		{"territory",     event.territory},
		{"unit_count",    event.unitCount},
		{"spice_value",   event.spiceValue},
		{"leader_power",  event.leaderPower},
		{"visibility",    visibility},
		{"actor_faction", event.actorFactionIndex},
	};
	return out.dump();
}

} // namespace EventSerialization
