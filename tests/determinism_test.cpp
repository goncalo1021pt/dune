// AI-mode determinism tests (Phase 5 commit 2). Locks in: with a fixed
// seed and no adapter (AI fallback paths), the engine produces an
// identical event stream across independent runs.
//
// This is the single-seed half of acceptance criterion 6 ("Deterministic
// replay passes for fixed seed + decision trace"). The decision-trace
// half lands in commit 3's replay test.
//
// Trap that motivated commit 0: the AI battle-strength roll used libc
// rand() — process-global state. Two Game instances inside one test
// process drew from different rand() positions and produced different
// outcomes. Routing through ctx.rng made this test possible.

#include "doctest/doctest.h"
#include "events/event_recorder.hpp"
#include "events/event.hpp"
#include "game.hpp"

#include <string>
#include <vector>

namespace {

// Capture an event stream by running a fresh Game(playerCount, seed)
// to completion with no adapter (AI mode) and dumping the recorder.
std::string runAndDump(unsigned int seed, int playerCount = 6) {
	Game game(playerCount, seed, /*interactive=*/false);
	EventRecorder rec;
	rec.attach(game.getEventBus());
	game.runGame();
	std::string dump = rec.dumpJsonLines();
	rec.detach();
	return dump;
}

} // namespace

TEST_SUITE("Determinism") {

TEST_CASE("seed=42 produces an identical event stream on every run") {
	// Three independent runs of the same seed in the same process.
	// If anything anywhere uses libc rand(), std::random_device,
	// chrono::now(), unordered_map iteration order, or process-global
	// state, the streams diverge and this fails.
	std::string a = runAndDump(42);
	std::string b = runAndDump(42);
	std::string c = runAndDump(42);

	CHECK(a.size() > 0);
	CHECK(a == b);
	CHECK(a == c);
}

TEST_CASE("multiple seeds each produce identical streams across two runs") {
	// Per-seed determinism — picks a few seeds spaced across the uint32
	// space to surface anything that's sensitive to particular bit
	// patterns of the seed (e.g. a hash collision in a downstream lookup).
	for (unsigned int seed : {7u, 42u, 99u, 1024u, 65537u}) {
		std::string a = runAndDump(seed);
		std::string b = runAndDump(seed);
		CHECK_MESSAGE(a == b, "seed " << seed << " event stream differed across runs");
		CHECK_MESSAGE(a.size() > 0, "seed " << seed << " produced an empty stream");
	}
}

TEST_CASE("different seeds produce different event streams") {
	// Determinism without sensitivity to the seed would mean the engine
	// is broken in a different way (everything's already pinned somehow).
	// This pins the inverse: changing the seed must change the stream.
	std::string a = runAndDump(42);
	std::string b = runAndDump(43);
	CHECK(a != b);
}

TEST_CASE("smaller player counts also stay deterministic") {
	// 2-player and 4-player AI runs both use the same code paths but
	// exercise different branches in turn order / shipment / battle.
	for (int n : {2, 4}) {
		std::string a = runAndDump(42, n);
		std::string b = runAndDump(42, n);
		CHECK_MESSAGE(a == b, "playerCount " << n << " event stream differed across runs");
	}
}

} // TEST_SUITE
