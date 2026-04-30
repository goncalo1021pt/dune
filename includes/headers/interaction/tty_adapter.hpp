#pragma once

#include "interaction_adapter.hpp"
#include <optional>

// TtyAdapter: synchronous adapter that reads from stdin / writes to stdout.
// Drop-in replacement for the old interactiveMode + direct std::cin pattern.
//
// Handles primitive kinds inline ("yn", "int", "select").
// For compound kinds ("deployment", "movement") it casts migration_ctx to
// PhaseContext* and delegates to the existing InteractiveInput helpers.
// The migration_ctx field is removed in PR 4 once payloads are fully serialized.
class TtyAdapter : public IInteractionAdapter {
public:
	TtyAdapter() = default;
	~TtyAdapter() override = default;

	std::optional<DecisionResponse> requestDecision(const DecisionRequest& req) override;

private:
	// Primitive kind handlers.
	DecisionResponse handleYn(const DecisionRequest& req);
	DecisionResponse handleInt(const DecisionRequest& req);
	DecisionResponse handleSelect(const DecisionRequest& req);

	// Compound kind handlers (use migration_ctx).
	DecisionResponse handleDeployment(const DecisionRequest& req);
	DecisionResponse handleMovement(const DecisionRequest& req);
};
