#pragma once
#include "factions/faction_ability.hpp"
#include <set>

class Player;
struct PhaseContext;

class BeneGesseritAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	void setupAtStart(Player* player) override;
	void placeStartingForces(PhaseContext& ctx) override;
	int getFreeRevivalsPerTurn() const override;
	bool alwaysReceivesCharity() const override;
	bool canUseWorthlessAsKarama() const override;
	void onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex, int amount,
		const std::string& destinationTerritory, bool fromOffPlanet) override;

	bool canFlipAdvisorsToFightersThisTurn(PhaseContext& ctx, const std::string& territoryName) const;
	int flipAdvisorsToFighters(PhaseContext& ctx, const std::string& territoryName);
	int flipFightersToAdvisors(PhaseContext& ctx, const std::string& territoryName);
	void beginTurn(int turnNumber);

	bool hasPrediction() const;
	bool predictionMatches(const std::string& winningFaction, int turnNumber) const;
	const std::string& getPredictedFaction() const;
	int getPredictedTurn() const;

private:
	void choosePrediction(PhaseContext& ctx, int beneIndex);

	bool predictionSet = false;
	std::string predictedFaction;
	int predictedTurn = 0;
	int lockedTurn = -1;
	std::set<std::string> lockedAdvisorTerritories;
};
