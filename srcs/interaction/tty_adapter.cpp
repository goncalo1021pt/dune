#include "interaction/tty_adapter.hpp"
#include "interactive_input.hpp"
#include "phases/phase_context.hpp"

#include <iostream>
#include <stdexcept>

std::optional<DecisionResponse> TtyAdapter::requestDecision(const DecisionRequest& req) {
	if (req.kind == "yn")         return handleYn(req);
	if (req.kind == "int")        return handleInt(req);
	if (req.kind == "select")     return handleSelect(req);
	if (req.kind == "deployment") return handleDeployment(req);
	if (req.kind == "movement")   return handleMovement(req);

	// Unknown kind: log and return invalid response rather than crashing.
	std::cerr << "[TtyAdapter] Unknown decision kind: " << req.kind << std::endl;
	return DecisionResponse{req.correlation_id, "", false};
}

// ---------------------------------------------------------------------------
// Primitive handlers
// ---------------------------------------------------------------------------

DecisionResponse TtyAdapter::handleYn(const DecisionRequest& req) {
	std::cout << "  " << req.prompt << " (y/n): ";
	while (true) {
		std::string input;
		std::getline(std::cin >> std::ws, input);
		if (input == "y" || input == "Y" || input == "yes" || input == "Yes") {
			return {req.correlation_id, "y", true};
		}
		if (input == "n" || input == "N" || input == "no" || input == "No") {
			return {req.correlation_id, "n", true};
		}
		std::cout << "  Invalid input. Enter y or n: ";
	}
}

DecisionResponse TtyAdapter::handleInt(const DecisionRequest& req) {
	std::cout << "  " << req.prompt;
	while (true) {
		std::string input;
		std::getline(std::cin >> std::ws, input);
		try {
			int value = std::stoi(input);
			if (value >= req.int_min && value <= req.int_max) {
				return {req.correlation_id, std::to_string(value), true};
			}
		} catch (...) {}
		std::cout << "  Invalid. Enter " << req.int_min << "-" << req.int_max << ": ";
	}
}

DecisionResponse TtyAdapter::handleSelect(const DecisionRequest& req) {
	if (!req.prompt.empty()) {
		std::cout << "  " << req.prompt << std::endl;
	}
	for (size_t i = 0; i < req.options.size(); ++i) {
		std::cout << "  " << (i + 1) << ". " << req.options[i] << std::endl;
	}
	if (req.allow_none) {
		std::cout << "  0. None" << std::endl;
	}

	const std::string rangeHint = req.allow_none
		? "(0-" + std::to_string(req.options.size()) + "): "
		: "(1-" + std::to_string(req.options.size()) + "): ";

	while (true) {
		std::cout << "  Choose " << rangeHint;
		std::string input;
		std::getline(std::cin >> std::ws, input);

		// Try number
		try {
			int choice = std::stoi(input);
			if (req.allow_none && choice == 0) {
				return {req.correlation_id, "", true};
			}
			if (choice >= 1 && choice <= static_cast<int>(req.options.size())) {
				return {req.correlation_id, req.options[choice - 1], true};
			}
		} catch (...) {}

		// Try name match (case-insensitive)
		for (const std::string& opt : req.options) {
			if (opt == input) {
				return {req.correlation_id, opt, true};
			}
		}
		std::cout << "  Invalid choice. Try again." << std::endl;
	}
}

// ---------------------------------------------------------------------------
// Compound handlers — use migration_ctx until PR 4 serializes these fully.
// ---------------------------------------------------------------------------

DecisionResponse TtyAdapter::handleDeployment(const DecisionRequest& req) {
	if (!req.migration_ctx) {
		std::cerr << "[TtyAdapter] deployment request missing migration_ctx" << std::endl;
		return {req.correlation_id, "{\"skip\":true}", false};
	}
	PhaseContext& ctx = *static_cast<PhaseContext*>(req.migration_ctx);
	if (req.actor_index < 0 || req.actor_index >= static_cast<int>(ctx.players.size())) {
		return {req.correlation_id, "{\"skip\":true}", false};
	}
	Player* player = ctx.players[req.actor_index];

	// valid_targets are passed as options
	auto choice = InteractiveInput::getDeploymentDecision(ctx, player, req.options);

	if (!choice.shouldDeploy) {
		return {req.correlation_id, "{\"skip\":true}", true};
	}

	// Serialize result as minimal JSON (nlohmann not included here; hand-rolled is fine
	// for this small struct — PR 4 will use the proper serializer).
	std::string json = "{\"territory\":\"" + choice.territoryName + "\""
		+ ",\"normal\":" + std::to_string(choice.normalUnits)
		+ ",\"elite\":" + std::to_string(choice.eliteUnits)
		+ ",\"sector\":" + std::to_string(choice.sector)
		+ ",\"skip\":false}";
	return {req.correlation_id, json, true};
}

DecisionResponse TtyAdapter::handleMovement(const DecisionRequest& req) {
	if (!req.migration_ctx) {
		std::cerr << "[TtyAdapter] movement request missing migration_ctx" << std::endl;
		return {req.correlation_id, "{\"skip\":true}", false};
	}
	PhaseContext& ctx = *static_cast<PhaseContext*>(req.migration_ctx);
	if (req.actor_index < 0 || req.actor_index >= static_cast<int>(ctx.players.size())) {
		return {req.correlation_id, "{\"skip\":true}", false};
	}
	Player* player = ctx.players[req.actor_index];

	// options = territories with units, int_max = movement range
	auto choice = InteractiveInput::getMovementDecision(ctx, player, req.options, req.int_max);

	if (!choice.shouldMove) {
		return {req.correlation_id, "{\"skip\":true}", true};
	}

	std::string json = "{\"from\":\"" + choice.fromTerritory + "\""
		+ ",\"to\":\"" + choice.toTerritory + "\""
		+ ",\"normal\":" + std::to_string(choice.normalUnits)
		+ ",\"elite\":" + std::to_string(choice.eliteUnits)
		+ ",\"from_sector\":" + std::to_string(choice.fromSector)
		+ ",\"to_sector\":" + std::to_string(choice.toSector)
		+ ",\"skip\":false}";
	return {req.correlation_id, json, true};
}
