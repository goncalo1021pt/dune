#include "doctest/doctest.h"
#include "reactions/reaction_engine.hpp"
#include "reactions/reaction_window.hpp"
#include "player.hpp"
#include "leader.hpp"
#include "cards/treachery_deck.hpp"
#include "factions/faction_ability.hpp"
#include <random>

namespace {
// Minimal stub for testing BG canUseWorthlessAsKarama path without pulling
// in BeneGesseritAbility's full setup machinery.
struct WorthlessAsKaramaAbility : public FactionAbility {
	std::string getFactionName() const override { return "TestBG"; }
	bool canUseWorthlessAsKarama() const override { return true; }
};
} // namespace

// These tests lock in the legality contract: a reaction card is legally
// playable iff its window is currently open. AnytimeSafe is satisfied by
// any open window. Outside any window, all reactions are illegal.
//
// Window-side-effect behavior (Weather Control accept/decline, Hajr,
// Harkonnen capture) is tested end-to-end via the seed-42 smoke run; this
// suite is the unit-level legality check.

TEST_SUITE("ReactionEngine") {

TEST_CASE("default window is None and reactions are illegal") {
	ReactionEngine engine;
	CHECK(engine.currentWindow() == ReactionWindow::None);
	CHECK_FALSE(engine.isWindowOpen());
	CHECK_FALSE(engine.isReactionLegalNow("Weather Control"));
	CHECK_FALSE(engine.isReactionLegalNow("Hajr"));
	CHECK_FALSE(engine.isReactionLegalNow("Tleilaxu Ghola"));
}

TEST_CASE("unknown card names are never legal") {
	ReactionEngine engine;
	CHECK_FALSE(engine.isReactionLegalNow("Crysknife"));
	CHECK_FALSE(engine.isReactionLegalNow(""));
	CHECK_FALSE(engine.isReactionLegalNow("Some Random Card"));
}

TEST_CASE("reactionWindowName produces stable strings") {
	CHECK(std::string(reactionWindowName(ReactionWindow::BeforeStormMove)) == "BeforeStormMove");
	CHECK(std::string(reactionWindowName(ReactionWindow::AfterMovement))  == "AfterMovement");
	CHECK(std::string(reactionWindowName(ReactionWindow::AnytimeSafe))    == "AnytimeSafe");
	CHECK(std::string(reactionWindowName(ReactionWindow::None))           == "None");
}

TEST_CASE("Weather Control is legal only inside BeforeStormMove") {
	ReactionEngine engine;
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeStormMove);
		CHECK(engine.isReactionLegalNow("Weather Control"));
	}
	CHECK_FALSE(engine.isReactionLegalNow("Weather Control"));

	// Wrong window must reject.
	ReactionEngine::WindowGuard g(engine, ReactionWindow::AfterMovement);
	CHECK_FALSE(engine.isReactionLegalNow("Weather Control"));
}

TEST_CASE("Hajr is legal only inside AfterMovement") {
	ReactionEngine engine;
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::AfterMovement);
		CHECK(engine.isReactionLegalNow("Hajr"));
	}
	CHECK_FALSE(engine.isReactionLegalNow("Hajr"));

	ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeStormMove);
	CHECK_FALSE(engine.isReactionLegalNow("Hajr"));
}

TEST_CASE("Tleilaxu Ghola is legal at AnytimeSafe and rejected when no window is open") {
	ReactionEngine engine;
	CHECK_FALSE(engine.isReactionLegalNow("Tleilaxu Ghola"));

	ReactionEngine::WindowGuard g(engine, ReactionWindow::AnytimeSafe);
	CHECK(engine.isReactionLegalNow("Tleilaxu Ghola"));
}

TEST_CASE("Tleilaxu Ghola is also legal inside any other open window") {
	// AnytimeSafe is satisfied by any open window: per the rules, "play
	// at any time" means at any safe checkpoint, including window
	// boundaries that are themselves reaction windows.
	ReactionEngine engine;
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeStormMove);
		CHECK(engine.isReactionLegalNow("Tleilaxu Ghola"));
	}
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::AfterBattleResolution);
		CHECK(engine.isReactionLegalNow("Tleilaxu Ghola"));
	}
}

TEST_CASE("nested windows restore the outer window on exit") {
	ReactionEngine engine;
	CHECK(engine.currentWindow() == ReactionWindow::None);
	{
		ReactionEngine::WindowGuard outer(engine, ReactionWindow::AfterBattleResolution);
		CHECK(engine.currentWindow() == ReactionWindow::AfterBattleResolution);
		{
			ReactionEngine::WindowGuard inner(engine, ReactionWindow::AnytimeSafe);
			CHECK(engine.currentWindow() == ReactionWindow::AnytimeSafe);
		}
		CHECK(engine.currentWindow() == ReactionWindow::AfterBattleResolution);
	}
	CHECK(engine.currentWindow() == ReactionWindow::None);
}

TEST_CASE("applyGholaLeaderRevive moves a dead leader back to alive and discards the card") {
	Player p(0, "Atreides");
	p.addTreacheryCard("Tleilaxu Ghola");
	p.addLeader(Leader("Thufir Hawat", 5));
	p.killLeader(0);
	REQUIRE(p.getDeadLeaders().size() == 1);
	REQUIRE(p.getAliveLeaders().empty());

	CHECK(ReactionEngine::applyGholaLeaderRevive(p, 0));
	CHECK(p.getDeadLeaders().empty());
	REQUIRE(p.getAliveLeaders().size() == 1);
	CHECK(p.getAliveLeaders()[0].name == "Thufir Hawat");
	// Card was consumed.
	CHECK(p.getTreacheryCards().empty());
}

TEST_CASE("applyGholaLeaderRevive refuses without the card and refuses out-of-range index") {
	Player p(0, "Atreides");
	p.addLeader(Leader("Gurney", 4));
	p.killLeader(0);

	// No Ghola card in hand.
	CHECK_FALSE(ReactionEngine::applyGholaLeaderRevive(p, 0));
	CHECK(p.getDeadLeaders().size() == 1);

	p.addTreacheryCard("Tleilaxu Ghola");
	// Index out of range — card must NOT be consumed.
	CHECK_FALSE(ReactionEngine::applyGholaLeaderRevive(p, 5));
	CHECK(p.getDeadLeaders().size() == 1);
	REQUIRE(p.getTreacheryCards().size() == 1);
	CHECK(p.getTreacheryCards()[0] == "Tleilaxu Ghola");
}

TEST_CASE("applyGholaForceRevive caps at 5, caps at total destroyed, normals first") {
	Player p(0, "Harkonnen");
	p.addTreacheryCard("Tleilaxu Ghola");
	p.setUnitsReserve(0);
	p.setEliteUnitsReserve(0);
	p.deployUnits(0); // no-op, just keep deployed at 0
	// Stage 7 destroyed normal + 2 destroyed elite by deploying then destroying.
	p.setUnitsReserve(7);
	p.setEliteUnitsReserve(2);
	p.destroyUnits(7);
	p.destroyEliteUnits(2);
	REQUIRE(p.getUnitsDestroyed() == 7);
	REQUIRE(p.getEliteUnitsDestroyed() == 2);

	// Request 7 — capped to 5. All five come from normals (normals first).
	CHECK(ReactionEngine::applyGholaForceRevive(p, 7) == 5);
	CHECK(p.getUnitsReserve() == 5);
	CHECK(p.getEliteUnitsReserve() == 0);
	CHECK(p.getUnitsDestroyed() == 2);
	CHECK(p.getEliteUnitsDestroyed() == 2);
	CHECK(p.getTreacheryCards().empty());
}

TEST_CASE("applyGholaForceRevive spills into elites when normals run out") {
	Player p(0, "Atreides");
	p.addTreacheryCard("Tleilaxu Ghola");
	p.setUnitsReserve(2);
	p.setEliteUnitsReserve(3);
	p.destroyUnits(2);
	p.destroyEliteUnits(3);
	REQUIRE(p.getUnitsDestroyed() == 2);
	REQUIRE(p.getEliteUnitsDestroyed() == 3);

	CHECK(ReactionEngine::applyGholaForceRevive(p, 4) == 4);
	CHECK(p.getUnitsReserve() == 2);
	CHECK(p.getEliteUnitsReserve() == 2);
	CHECK(p.getUnitsDestroyed() == 0);
	CHECK(p.getEliteUnitsDestroyed() == 1);
}

TEST_CASE("applyGholaForceRevive is a no-op when nothing is destroyed and keeps the card") {
	Player p(0, "Atreides");
	p.addTreacheryCard("Tleilaxu Ghola");
	REQUIRE(p.getUnitsDestroyed() == 0);
	REQUIRE(p.getEliteUnitsDestroyed() == 0);

	CHECK(ReactionEngine::applyGholaForceRevive(p, 3) == 0);
	REQUIRE(p.getTreacheryCards().size() == 1);
	CHECK(p.getTreacheryCards()[0] == "Tleilaxu Ghola");
}

TEST_CASE("Karama is legal only inside BeforeFactionAdvantage") {
	ReactionEngine engine;
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeFactionAdvantage);
		CHECK(engine.isReactionLegalNow("Karama"));
	}
	CHECK_FALSE(engine.isReactionLegalNow("Karama"));

	// Karama is not legal in unrelated windows.
	ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeBattlePlanReveal);
	CHECK_FALSE(engine.isReactionLegalNow("Karama"));
}

TEST_CASE("Tleilaxu Ghola is also legal during a BeforeFactionAdvantage window") {
	ReactionEngine engine;
	ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeFactionAdvantage);
	CHECK(engine.isReactionLegalNow("Tleilaxu Ghola"));
}

TEST_CASE("reactionWindowName covers BeforeFactionAdvantage") {
	CHECK(std::string(reactionWindowName(ReactionWindow::BeforeFactionAdvantage))
		== "BeforeFactionAdvantage");
}

TEST_CASE("findKaramaCard returns Karama when present") {
	Player p(0, "Atreides");
	p.addTreacheryCard("Karama");
	CHECK(ReactionEngine::findKaramaCard(p) == "Karama");
}

TEST_CASE("findKaramaCard returns empty for a non-BG hand without Karama") {
	Player p(0, "Atreides");
	p.addTreacheryCard("Crysknife");
	p.addTreacheryCard("Baliset");  // worthless, but Atreides cannot use as Karama
	CHECK(ReactionEngine::findKaramaCard(p) == "");
}

TEST_CASE("findKaramaCard returns first worthless when BG holds no Karama") {
	Player p(5, "Bene Gesserit");
	p.setFactionAbility(std::make_unique<WorthlessAsKaramaAbility>());
	p.addTreacheryCard("Crysknife");
	p.addTreacheryCard("Kulon");  // worthless — first one wins
	p.addTreacheryCard("Baliset");
	CHECK(ReactionEngine::findKaramaCard(p) == "Kulon");
}

TEST_CASE("findKaramaCard prefers a real Karama over worthless for BG") {
	Player p(5, "Bene Gesserit");
	p.setFactionAbility(std::make_unique<WorthlessAsKaramaAbility>());
	p.addTreacheryCard("Kulon");
	p.addTreacheryCard("Karama");
	CHECK(ReactionEngine::findKaramaCard(p) == "Karama");
}

TEST_CASE("applyKaramaPlay removes the card from the hand and pushes to the deck discard pile") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();
	Player p(0, "Atreides");
	p.addTreacheryCard("Karama");
	REQUIRE(deck.discardSize() == 0);

	CHECK(ReactionEngine::applyKaramaPlay(p, deck, "Karama"));
	CHECK(p.getTreacheryCards().empty());
	REQUIRE(deck.discardSize() == 1);
	CHECK(deck.getDiscardPile()[0] == "Karama");
}

TEST_CASE("applyKaramaPlay returns false when the named card is not in hand") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();
	Player p(0, "Atreides");
	CHECK_FALSE(ReactionEngine::applyKaramaPlay(p, deck, "Karama"));
	CHECK(deck.discardSize() == 0);
}

TEST_CASE("TreacheryDeck discard pile grows on discard and resets on initialize") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();
	deck.discard("Karama");
	deck.discard("Baliset");
	CHECK(deck.discardSize() == 2);
	REQUIRE(deck.getDiscardPile().size() == 2);
	CHECK(deck.getDiscardPile()[0] == "Karama");
	CHECK(deck.getDiscardPile()[1] == "Baliset");

	deck.initialize();
	CHECK(deck.discardSize() == 0);
}

TEST_CASE("applyGholaForceRevive without the card returns 0") {
	Player p(0, "Atreides");
	p.setUnitsReserve(3);
	p.destroyUnits(3);
	REQUIRE(p.getUnitsDestroyed() == 3);

	CHECK(ReactionEngine::applyGholaForceRevive(p, 2) == 0);
	CHECK(p.getUnitsDestroyed() == 3);
}

} // TEST_SUITE
