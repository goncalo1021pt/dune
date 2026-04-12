#pragma once
#include "factions/faction_ability.hpp"

class Player;
struct PhaseContext;

class BeneGesseritAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	void setupAtStart(Player* player) override;
	void placeStartingForces(PhaseContext& ctx) override;
	int getFreeRevivalsPerTurn() const override;
	void onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex, int amount) override;

	bool hasPrediction() const;
	bool predictionMatches(const std::string& winningFaction, int turnNumber) const;
	const std::string& getPredictedFaction() const;
	int getPredictedTurn() const;

private:
	void choosePrediction(PhaseContext& ctx, int beneIndex);

	bool predictionSet = false;
	std::string predictedFaction;
	int predictedTurn = 0;
};
