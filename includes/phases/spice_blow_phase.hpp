#pragma once

#include "phase.hpp"

class SpiceBlowPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;

private:
	spiceCard drawSpiceCard(std::vector<spiceCard>& spiceDeck, size_t& spiceDeckIndex,
	                        std::vector<spiceCard>& spiceDiscardPileA,
	                        std::vector<spiceCard>& spiceDiscardPileB,
	                        std::mt19937& rng);
	void discardSpiceCard(const spiceCard& card, int discardPileIndex,
	                      bool useExtended,
	                      std::vector<spiceCard>& spiceDiscardPileA,
	                      std::vector<spiceCard>& spiceDiscardPileB);
	void resolveWormOnTerritory(const std::string& territoryName, GameMap& map);
};
