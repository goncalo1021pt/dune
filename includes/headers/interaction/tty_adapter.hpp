#pragma once

#include "interaction_adapter.hpp"
#include <optional>

// TtyAdapter: synchronous adapter that reads from stdin / writes to stdout.
// Handles the only kinds the engine emits since PR 4c: "yn", "int", "select".
class TtyAdapter : public IInteractionAdapter {
public:
	TtyAdapter() = default;
	~TtyAdapter() override = default;

	std::optional<DecisionResponse> requestDecision(const DecisionRequest& req) override;

private:
	DecisionResponse handleYn(const DecisionRequest& req);
	DecisionResponse handleInt(const DecisionRequest& req);
	DecisionResponse handleSelect(const DecisionRequest& req);
};
