#include "doctest/doctest.h"
#include "reactions/reaction_engine.hpp"
#include "reactions/reaction_window.hpp"
#include "player.hpp"
#include "leader.hpp"
#include "cards/treachery_deck.hpp"
#include "factions/faction_ability.hpp"
#include <algorithm>
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

TEST_CASE("Karama legality covers all four windows used by basic and advanced uses") {
	ReactionEngine engine;
	CHECK_FALSE(engine.isReactionLegalNow("Karama"));

	// Basic block-an-advantage use.
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeFactionAdvantage);
		CHECK(engine.isReactionLegalNow("Karama"));
	}
	// Advanced AnytimeSafe powers (Emperor / Fremen / Harkonnen).
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::AnytimeSafe);
		CHECK(engine.isReactionLegalNow("Karama"));
	}
	// Advanced Guild stop-shipment.
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeShipment);
		CHECK(engine.isReactionLegalNow("Karama"));
	}
	// Advanced Atreides peek-everything.
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeBattlePlanReveal);
		CHECK(engine.isReactionLegalNow("Karama"));
	}

	// Unrelated windows reject Karama.
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::AfterMovement);
		CHECK_FALSE(engine.isReactionLegalNow("Karama"));
	}
	{
		ReactionEngine::WindowGuard g(engine, ReactionWindow::BeforeStormMove);
		CHECK_FALSE(engine.isReactionLegalNow("Karama"));
	}

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

TEST_CASE("applyEmperorKaramaLeaderRevive revives a dead leader and discards Karama") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();
	Player p(2, "Emperor");
	p.addTreacheryCard("Karama");
	p.addLeader(Leader("Hasimir Fenring", 6));
	p.killLeader(0);
	REQUIRE(p.getDeadLeaders().size() == 1);

	CHECK(ReactionEngine::applyEmperorKaramaLeaderRevive(p, deck, 0));
	CHECK(p.getDeadLeaders().empty());
	REQUIRE(p.getAliveLeaders().size() == 1);
	CHECK(p.getAliveLeaders()[0].name == "Hasimir Fenring");
	CHECK(p.getTreacheryCards().empty());
	REQUIRE(deck.discardSize() == 1);
	CHECK(deck.getDiscardPile()[0] == "Karama");
}

TEST_CASE("applyEmperorKaramaLeaderRevive refuses without Karama and on bad index") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();
	Player p(2, "Emperor");
	p.addLeader(Leader("Hasimir Fenring", 6));
	p.killLeader(0);

	// No Karama in hand.
	CHECK_FALSE(ReactionEngine::applyEmperorKaramaLeaderRevive(p, deck, 0));
	CHECK(p.getDeadLeaders().size() == 1);
	CHECK(deck.discardSize() == 0);

	p.addTreacheryCard("Karama");
	// Out-of-range index — Karama must NOT be discarded.
	CHECK_FALSE(ReactionEngine::applyEmperorKaramaLeaderRevive(p, deck, 9));
	REQUIRE(p.getTreacheryCards().size() == 1);
	CHECK(p.getTreacheryCards()[0] == "Karama");
	CHECK(deck.discardSize() == 0);
}

TEST_CASE("applyEmperorKaramaForceRevive caps at 3 and at total destroyed") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();
	Player p(2, "Emperor");
	p.addTreacheryCard("Karama");
	p.setUnitsReserve(7);
	p.setEliteUnitsReserve(2);
	p.destroyUnits(7);
	p.destroyEliteUnits(2);
	REQUIRE(p.getUnitsDestroyed() == 7);
	REQUIRE(p.getEliteUnitsDestroyed() == 2);

	// Request 5 — capped to 3 (Imperial revival hard cap, not Ghola's 5).
	CHECK(ReactionEngine::applyEmperorKaramaForceRevive(p, deck, 5) == 3);
	CHECK(p.getUnitsReserve() == 3);  // all 3 came from normals
	CHECK(p.getEliteUnitsReserve() == 0);
	CHECK(p.getUnitsDestroyed() == 4);
	CHECK(p.getEliteUnitsDestroyed() == 2);
	CHECK(p.getTreacheryCards().empty());
	REQUIRE(deck.discardSize() == 1);
	CHECK(deck.getDiscardPile()[0] == "Karama");
}

TEST_CASE("applyEmperorKaramaForceRevive is a no-op without Karama or with nothing destroyed") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();
	Player p(2, "Emperor");
	p.setUnitsReserve(3);
	p.destroyUnits(3);

	// No Karama in hand.
	CHECK(ReactionEngine::applyEmperorKaramaForceRevive(p, deck, 2) == 0);
	CHECK(p.getUnitsDestroyed() == 3);
	CHECK(deck.discardSize() == 0);

	// With Karama but nothing destroyed — card kept, nothing discarded.
	Player q(2, "Emperor");
	q.addTreacheryCard("Karama");
	CHECK(ReactionEngine::applyEmperorKaramaForceRevive(q, deck, 2) == 0);
	REQUIRE(q.getTreacheryCards().size() == 1);
	CHECK(q.getTreacheryCards()[0] == "Karama");
	CHECK(deck.discardSize() == 0);
}

TEST_CASE("applyHarkonnenKaramaHandSwap performs a 1-for-1 blind swap and discards Karama") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();

	Player h(3, "Harkonnen");
	h.addTreacheryCard("Karama");
	h.addTreacheryCard("Crysknife");
	h.addTreacheryCard("Shield");

	Player t(0, "Atreides");
	t.addTreacheryCard("Lasgun");
	t.addTreacheryCard("Snooper");

	// Take target index 0 ("Lasgun"), give "Crysknife".
	int swapped = ReactionEngine::applyHarkonnenKaramaHandSwap(h, t, deck,
		{0}, {"Crysknife"});
	CHECK(swapped == 1);

	// Harkonnen lost Karama + Crysknife, gained Lasgun. Has Shield + Lasgun.
	REQUIRE(h.getTreacheryCards().size() == 2);
	const auto& hHand = h.getTreacheryCards();
	CHECK(std::find(hHand.begin(), hHand.end(), "Karama") == hHand.end());
	CHECK(std::find(hHand.begin(), hHand.end(), "Crysknife") == hHand.end());
	CHECK(std::find(hHand.begin(), hHand.end(), "Lasgun") != hHand.end());
	CHECK(std::find(hHand.begin(), hHand.end(), "Shield") != hHand.end());

	// Target lost Lasgun, gained Crysknife. Has Snooper + Crysknife.
	REQUIRE(t.getTreacheryCards().size() == 2);
	const auto& tHand = t.getTreacheryCards();
	CHECK(std::find(tHand.begin(), tHand.end(), "Lasgun") == tHand.end());
	CHECK(std::find(tHand.begin(), tHand.end(), "Snooper") != tHand.end());
	CHECK(std::find(tHand.begin(), tHand.end(), "Crysknife") != tHand.end());

	REQUIRE(deck.discardSize() == 1);
	CHECK(deck.getDiscardPile()[0] == "Karama");
}

TEST_CASE("applyHarkonnenKaramaHandSwap can swap multiple cards with stable target hand size") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();

	Player h(3, "Harkonnen");
	h.addTreacheryCard("Karama");
	h.addTreacheryCard("Weapon1");
	h.addTreacheryCard("Weapon2");
	h.addTreacheryCard("Weapon3");

	Player t(0, "Atreides");
	t.addTreacheryCard("Defense1");
	t.addTreacheryCard("Defense2");
	t.addTreacheryCard("Defense3");

	// Two pairs: take target[0] ("Defense1") give "Weapon1";
	//            take target[2] ("Weapon1" — just-given!) give "Weapon2".
	// After first pair, target is [Defense2, Defense3, Weapon1]. So
	// target[2] is "Weapon1", which is what we just gave. The blind swap
	// happens to take it back. That's allowed by the rules.
	int swapped = ReactionEngine::applyHarkonnenKaramaHandSwap(h, t, deck,
		{0, 2}, {"Weapon1", "Weapon2"});
	CHECK(swapped == 2);

	// Harkonnen lost Karama + Weapon1 + Weapon2, gained Defense1 + Weapon1.
	// Net hand: Weapon3, Defense1, Weapon1.
	REQUIRE(h.getTreacheryCards().size() == 3);
	const auto& hHand = h.getTreacheryCards();
	CHECK(std::find(hHand.begin(), hHand.end(), "Karama") == hHand.end());
	CHECK(std::find(hHand.begin(), hHand.end(), "Weapon3") != hHand.end());
	CHECK(std::find(hHand.begin(), hHand.end(), "Defense1") != hHand.end());
	CHECK(std::find(hHand.begin(), hHand.end(), "Weapon1") != hHand.end());

	// Target lost Defense1 + Weapon1, gained Weapon1 + Weapon2.
	// Net hand: Defense2, Defense3, Weapon2 (Weapon1 came in then went out).
	REQUIRE(t.getTreacheryCards().size() == 3);
	const auto& tHand = t.getTreacheryCards();
	CHECK(std::find(tHand.begin(), tHand.end(), "Defense2") != tHand.end());
	CHECK(std::find(tHand.begin(), tHand.end(), "Defense3") != tHand.end());
	CHECK(std::find(tHand.begin(), tHand.end(), "Weapon2") != tHand.end());

	CHECK(deck.discardSize() == 1);
}

TEST_CASE("applyHarkonnenKaramaHandSwap refuses without Karama and never gives away the Karama itself") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();

	// No Karama in hand.
	{
		Player h(3, "Harkonnen");
		h.addTreacheryCard("Crysknife");
		Player t(0, "Atreides");
		t.addTreacheryCard("Lasgun");
		CHECK(ReactionEngine::applyHarkonnenKaramaHandSwap(h, t, deck,
			{0}, {"Crysknife"}) == 0);
		REQUIRE(h.getTreacheryCards().size() == 1);
		CHECK(h.getTreacheryCards()[0] == "Crysknife");
		CHECK(deck.discardSize() == 0);
	}

	// giveBack contains "Karama" — must refuse without applying anything.
	{
		Player h(3, "Harkonnen");
		h.addTreacheryCard("Karama");
		h.addTreacheryCard("Crysknife");
		Player t(0, "Atreides");
		t.addTreacheryCard("Lasgun");
		CHECK(ReactionEngine::applyHarkonnenKaramaHandSwap(h, t, deck,
			{0}, {"Karama"}) == 0);
		// Harkonnen and target untouched.
		CHECK(h.getTreacheryCards().size() == 2);
		CHECK(t.getTreacheryCards().size() == 1);
		CHECK(deck.discardSize() == 0);
	}
}

TEST_CASE("applyHarkonnenKaramaHandSwap returns 0 on size mismatch") {
	std::mt19937 rng(42);
	TreacheryDeck deck(rng);
	deck.initialize();
	Player h(3, "Harkonnen");
	h.addTreacheryCard("Karama");
	h.addTreacheryCard("Crysknife");
	Player t(0, "Atreides");
	t.addTreacheryCard("Lasgun");

	CHECK(ReactionEngine::applyHarkonnenKaramaHandSwap(h, t, deck,
		{0, 1}, {"Crysknife"}) == 0);
	// Karama still in hand — no apply happened.
	const auto& hHand = h.getTreacheryCards();
	CHECK(std::find(hHand.begin(), hHand.end(), "Karama") != hHand.end());
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
