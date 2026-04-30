#include "doctest/doctest.h"
#include "reactions/reaction_engine.hpp"
#include "reactions/reaction_window.hpp"

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

} // TEST_SUITE
