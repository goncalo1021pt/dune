#pragma once

#include <string>
#include <sstream>

// Event types for the game engine
enum class EventType {
	// Game lifecycle
	GAME_INITIALIZED,
	GAME_ENDED,
	
	// Turn management
	TURN_STARTED,
	TURN_ENDED,
	
	// Phase management
	PHASE_STARTED,
	PHASE_ENDED,
	
	// Player/Faction events
	PLAYER_INITIALIZED,
	FACTION_TURN_ORDER_CALCULATED,
	
	// Territory and unit management
	UNITS_SHIPPED,
	UNITS_MOVED,
	UNITS_BATTLING,
	UNITS_KILLED,
	LEADER_KILLED,
	LEADER_REVIVED,
	
	// Battle events
	BATTLE_STARTED,
	BATTLE_RESOLVED,
	BATTLE_LEADER_SELECTED,
	BATTLE_WHEEL_SELECTED,
	BATTLE_VALUE_CALCULATED,
	BATTLE_WINNER_DETERMINED,
	
	// Spice events
	SPICE_COLLECTED,
	SPICE_BLOWN,
	SPICE_SPENT,
	SPICE_CHARITY,
	
	// Storm events
	STORM_PLACED,
	STORM_MOVED,
	
	// Bidding events
	BIDDING_STARTED,
	BIDDING_ENDED,
	BID_PLACED,
	
	// Debug/Info
	DEBUG_INFO,
	ERROR_EVENT
};

// Game event structure
struct Event {
	EventType type;
	std::string message;
	int turnNumber;
	std::string currentPhase;
	
	// Optional context
	std::string playerFaction;
	std::string territory;
	int unitCount;
	int spiceValue;
	int leaderPower;
	
	Event() 
		: type(EventType::DEBUG_INFO), message(""), turnNumber(0), currentPhase(""),
		  playerFaction(""), territory(""), unitCount(0), spiceValue(0), leaderPower(0) {}
	
	Event(EventType eventType, const std::string& msg, int turn, const std::string& phase)
		: type(eventType), message(msg), turnNumber(turn), currentPhase(phase),
		  playerFaction(""), territory(""), unitCount(0), spiceValue(0), leaderPower(0) {}
	
	// Helper method to create event with all fields
	static Event createWithContext(
		EventType eventType,
		const std::string& msg,
		int turn,
		const std::string& phase,
		const std::string& faction = "",
		const std::string& terr = "",
		int units = 0,
		int spice = 0,
		int power = 0
	) {
		Event e;
		e.type = eventType;
		e.message = msg;
		e.turnNumber = turn;
		e.currentPhase = phase;
		e.playerFaction = faction;
		e.territory = terr;
		e.unitCount = units;
		e.spiceValue = spice;
		e.leaderPower = power;
		return e;
	}
};
