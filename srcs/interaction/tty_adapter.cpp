#include "interaction/tty_adapter.hpp"

#include <iostream>

std::optional<DecisionResponse> TtyAdapter::requestDecision(const DecisionRequest& req) {
	if (req.kind == "yn")     return handleYn(req);
	if (req.kind == "int")    return handleInt(req);
	if (req.kind == "select") return handleSelect(req);

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

