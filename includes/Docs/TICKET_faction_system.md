# TICKET: Faction System Implementation
**Project:** Dune Board Game Engine (C++)  
**Goal:** Implement per-faction rules without over-engineering  
**Strategy:** Option B hooks first → EventBus only when ordering problems appear  

---

## Context for the AI assistant

This is a C++ console implementation of the Dune board game. The codebase uses:
- A **Phase strategy pattern** (`Phase` base class, one class per game phase)
- A **PhaseContext** struct passed to every phase as a dependency injection container
- An **EventLogger** interface (`logEvent`, `logDebug`, `logError`) already wired into PhaseContext
- A **GameMap** with territory structs, unit stacks, and neighbour pointer lists
- A **Player** class holding spice, units, leaders, and treachery cards
- A **TreacheryDeck** and **SpiceDeck** already initialized from CSV/hardcoded data

The full rulebook, all 33 treachery cards, all 6 faction sheets, and the complete map are available in context.

**Do not refactor existing phases unless a ticket step explicitly says to.**  
**Do not introduce the EventBus until Ticket 3.**  
**Each ticket is a self-contained PR.**

---

## Known bugs to fix BEFORE starting tickets (do these first)

These are silent failures in `srcs/map.cpp` `linkNeighbours()`:

1. `"hagga Basin"` → `"Hagga Basin"` (Carthag neighbour list, lowercase h)
2. `"Fasle Wall East"` → `"False Wall East"` (appears 3 times)
3. `"Fasle Wall West"` → `"False Wall West"` (appears 1 time)
4. Missing commas between adjacent string literals in initializer lists — the C++ compiler silently concatenates them into one invalid name. Audit every `linkTerritoryNeighbours` call for missing commas.

Fix, compile, verify no `getTerritory` calls return null for valid territory names.

---

## Ticket 1 — FactionAbility base class + wiring (zero game logic change)

**Goal:** Introduce the hook interface and connect it to PhaseContext. Game must play identically after this ticket — all hooks return defaults.

### 1.1 Create `includes/headers/factions/faction_ability.hpp`

```cpp
#pragma once
#include <string>
#include <vector>

// Forward declarations
class Player;
struct territory;
struct treacheryCard;
struct PhaseContext;

/**
 * FactionAbility: base class for all faction-specific rule overrides.
 * Default implementations encode the standard rules (no faction advantage).
 * Concrete subclasses override only what their faction changes.
 *
 * Hook naming convention:
 *   get*()   → pure query, returns a value the phase uses
 *   on*()    → notification + optional mutation, phase continues after
 *   can*()   → permission check, returns bool
 */
class FactionAbility {
public:
    virtual ~FactionAbility() = default;

    // --- Identity ---
    virtual std::string getFactionName() const = 0;

    // --- Revival Phase hooks ---
    // How many forces this faction revives for free each turn
    virtual int getFreeRevivalsPerTurn() const { return 1; }
    // Can this faction buy additional revivals beyond the free ones?
    virtual bool canBuyAdditionalRevivals() const { return true; }

    // --- Shipment Phase hooks ---
    // Cost in spice to ship N units to a given territory (default: 1 city, 2 other)
    virtual int getShipmentCost(const territory* terr, int unitCount) const;
    // Can this faction ship from one on-planet territory to another? (Guild only)
    virtual bool canCrossShip() const { return false; }
    // Can this faction ship units back to reserves? (Guild only)
    virtual bool canShipToReserves() const { return false; }
    // Called when ANY other faction completes a shipment (BG piggyback hook)
    virtual void onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex) {}

    // --- Movement Phase hooks ---
    // Base movement range (before ornithopter bonus is applied)
    virtual int getBaseMovementRange() const { return 1; }
    // Fremen: 2 territories base. Others: 1.
    // Note: ornithopter bonus (3 territories) is applied ON TOP by the phase if
    // the faction controls Arrakeen or Carthag, replacing base range with 3.

    // Can this faction take its shipment+move out of turn order? (Guild only)
    virtual bool canMoveOutOfTurnOrder() const { return false; }

    // --- Bidding Phase hooks ---
    // Maximum treachery cards this faction can hold (default 4, Harkonnen 8)
    virtual int getMaxTreacheryCards() const { return 4; }
    // How many cards does this faction receive at game start? (default 1, Harkonnen 2)
    virtual int getStartingTreacheryCardCount() const { return 1; }
    // Called when a card is dealt face-down for bidding, before any player bids.
    // Atreides overrides this to peek at the card.
    virtual void onCardDealtForBidding(const treacheryCard& card, PhaseContext& ctx) {}
    // Called when this faction wins a treachery card at auction.
    // Harkonnen overrides to draw a bonus card.
    virtual void onCardWonAtAuction(PhaseContext& ctx) {}
    // Called when another faction pays spice for a treachery card.
    // Emperor overrides to redirect payment to self instead of Spice Bank.
    virtual void onOtherFactionPaidForCard(PhaseContext& ctx, int payingFactionIndex, int amount) {}

    // --- CHOAM Charity Phase hooks ---
    // Does this faction always receive charity regardless of spice count? (BG only)
    virtual bool alwaysReceivesCharity() const { return false; }

    // --- Battle Phase hooks ---
    // Fighting strength of a normal unit (default 1, Sardaukar/Fedaykin 2 vs non-Fremen)
    virtual int getNormalUnitStrength() const { return 1; }
    // Does this faction need to spend spice to count units at full strength?
    // (Advanced game rule — default true, Fremen false)
    virtual bool requiresSpiceForFullUnitStrength() const { return true; }
    // Called before battle plans are revealed. BG Voice intercepts here.
    virtual void onBeforeBattlePlanReveal(PhaseContext& ctx, int opponentIndex) {}
    // Called when this faction's opponent reveals their battle plan.
    // Atreides prescience intercepts here.
    virtual void onOpponentBattlePlanRevealed(PhaseContext& ctx, int opponentIndex) {}

    // --- Traitor hooks ---
    // Does this faction keep ALL traitor cards drawn? (Harkonnen only)
    virtual bool keepsAllTraitorCards() const { return false; }

    // --- Storm/Spice Blow hooks ---
    // Does this faction's forces survive a sandworm? (Fremen only)
    virtual bool survivesWorm() const { return false; }
    // Does this faction lose only half forces in a storm? (Fremen only)
    virtual bool hasReducedStormLosses() const { return false; }

    // --- Victory condition hooks ---
    // Check faction-specific special victory condition.
    // Return true if this faction wins by special condition this turn.
    virtual bool checkSpecialVictory(PhaseContext& ctx) const { return false; }

    // --- Alliance hooks ---
    // Spice this faction provides per ally per revival phase (Emperor: pays for 3 extra)
    virtual int getAllyRevivalSupport() const { return 0; }
};
```

### 1.2 Create default implementations for `getShipmentCost` in `srcs/factions/faction_ability.cpp`

```cpp
#include "factions/faction_ability.hpp"
#include "map.hpp"

int FactionAbility::getShipmentCost(const territory* terr, int unitCount) const {
    if (terr == nullptr || unitCount <= 0) return 0;
    if (terr->terrain == terrainType::city) return unitCount * 1;
    return unitCount * 2;
}
```

### 1.3 Create one concrete stub per faction

Create `includes/headers/factions/` and `srcs/factions/` subdirectories.
Create these files — all empty stubs that just return the faction name, everything else inherited from base:

- `atreides_ability.hpp / .cpp` — `getFactionName()` returns `"Atreides"`
- `harkonnen_ability.hpp / .cpp` — `getFactionName()` returns `"Harkonnen"`
- `fremen_ability.hpp / .cpp` — `getFactionName()` returns `"Fremen"`
- `emperor_ability.hpp / .cpp` — `getFactionName()` returns `"Emperor"`
- `spacing_guild_ability.hpp / .cpp` — `getFactionName()` returns `"Spacing Guild"`
- `bene_gesserit_ability.hpp / .cpp` — `getFactionName()` returns `"Bene Gesserit"`

### 1.4 Wire into Player

In `includes/headers/player.hpp`, add:
```cpp
#include "factions/faction_ability.hpp"

class Player {
    // existing fields...
private:
    std::unique_ptr<FactionAbility> ability;
public:
    void setFactionAbility(std::unique_ptr<FactionAbility> a);
    FactionAbility* getFactionAbility() const;
};
```

### 1.5 Wire into PhaseContext

In `includes/headers/phases/phase_context.hpp`, add:
```cpp
// No new field needed — access via players[i]->getFactionAbility()
// Add one convenience helper:
FactionAbility* getAbility(int playerIndex) const {
    if (playerIndex >= 0 && playerIndex < (int)players.size())
        return players[playerIndex]->getFactionAbility();
    return nullptr;
}
```

### 1.6 Wire faction creation in Game constructor

In `srcs/game.cpp`, after creating each player, assign the correct ability:
```cpp
#include "factions/atreides_ability.hpp"
// ... etc

// In Game constructor, after players.push_back(...):
switch(i) {
    case 0: players[i]->setFactionAbility(std::make_unique<AtreidesAbility>()); break;
    case 1: players[i]->setFactionAbility(std::make_unique<HarkonnenAbility>()); break;
    case 2: players[i]->setFactionAbility(std::make_unique<FremenAbility>()); break;
    case 3: players[i]->setFactionAbility(std::make_unique<EmperorAbility>()); break;
    case 4: players[i]->setFactionAbility(std::make_unique<SpacingGuildAbility>()); break;
    case 5: players[i]->setFactionAbility(std::make_unique<BeneGesseritAbility>()); break;
}
```

### 1.7 Replace hardcoded values in phases with ability hook calls

These are the ONLY phase changes in Ticket 1:

**`revival_phase.cpp`:** Replace `getFreeRevivesPerTurn()` from Player with `player->getFactionAbility()->getFreeRevivalsPerTurn()`

**`ship_and_move_phase.cpp`:** Replace `calculateDeploymentCost()` with `player->getFactionAbility()->getShipmentCost(terr, unitCount)`

**`ship_and_move_phase.cpp`:** Replace movement range calculation with:
```cpp
int base = player->getFactionAbility()->getBaseMovementRange();
bool hasOrnithopter = map.isControlled("Arrakeen", idx) || map.isControlled("Carthag", idx);
int range = hasOrnithopter ? 3 : base;
```

**`bidding_phase.cpp`:** Replace `>= 4` hand limit with `>= player->getFactionAbility()->getMaxTreacheryCards()`

**`choam_charity_phase.cpp`:** Replace the hardcoded Bene Gesserit index check with `player->getFactionAbility()->alwaysReceivesCharity()`

### 1.8 Acceptance criteria

- Game compiles with no warnings
- Game plays identically to before (all hooks return defaults)
- No `if (faction == X)` strings anywhere in phase code
- All faction ability stubs compile

---

## Ticket 2 — Implement Emperor and Harkonnen (numbers-only factions)

**Goal:** First real faction logic. These two factions only change numbers and card counts — no timing or interception complexity.

**Do not start this ticket until Ticket 1 is merged and tested.**

### 2.1 Emperor

File: `srcs/factions/emperor_ability.cpp`

```cpp
// Emperor receives spice from other factions' treachery purchases
// instead of it going to the Spice Bank.

int EmperorAbility::getFreeRevivalsPerTurn() const { return 1; }

// Advanced: Sardaukar worth 2 vs non-Fremen (implement in Ticket 2.3)
// Basic game: no change to unit strength yet
```

**Bidding phase change** (in `bidding_phase.cpp`):

When a player pays spice for a card, find the Emperor player and give them the spice instead of discarding it:

```cpp
// After winner is determined and pays:
int emperorIndex = findFactionIndex(ctx, "Emperor"); // helper to add
if (emperorIndex >= 0 && emperorIndex != winner) {
    // Emperor receives the spice
    ctx.players[emperorIndex]->addSpice(currentBid);
    // log it
} else {
    // No Emperor in game, spice goes to bank (already handled — do nothing)
}
```

Add helper to PhaseContext or game utilities:
```cpp
// Returns player index of named faction, or -1 if not in game
int findFactionIndex(PhaseContext& ctx, const std::string& factionName);
```

Hook to add to `EmperorAbility`:
```cpp
// Override — Emperor receives payment when others buy cards
void onOtherFactionPaidForCard(PhaseContext& ctx, int payingFactionIndex, int amount) override;
```

**AT START setup** (in `game.cpp` `initializeGame()`):

Emperor starts with 20 forces in reserve, 10 spice.
Add a `setupFaction(Player* player)` call after player creation that each FactionAbility implements:

```cpp
virtual void setupAtStart(Player* player) {
    // default: set standard starting values per settings.hpp
    player->setSpice(10);  // add setSpice() to Player
}
```

Emperor override:
```cpp
void EmperorAbility::setupAtStart(Player* player) {
    player->setSpice(10);
    // 20 units already set by STARTING_UNITS constant
    // 5 Sardaukar elite units (advanced game — leave for later)
}
```

### 2.2 Harkonnen

File: `srcs/factions/harkonnen_ability.cpp`

```cpp
int HarkonnenAbility::getMaxTreacheryCards() const { return 8; }
int HarkonnenAbility::getStartingTreacheryCardCount() const { return 2; }
bool HarkonnenAbility::keepsAllTraitorCards() const { return true; }

void HarkonnenAbility::onCardWonAtAuction(PhaseContext& ctx) override {
    // Draw one extra card for free
    // Find this player in ctx, add card if below 8
    auto card = ctx.treacheryDeck.drawCard();
    // find Harkonnen player and add card
    // log the bonus draw
}
```

**AT START:** 10 forces in Carthag, 10 in reserve, 10 spice, dealt 2 cards instead of 1.

`setupAtStart` override:
```cpp
void HarkonnenAbility::setupAtStart(Player* player) {
    player->setSpice(10);
    // Map placement handled separately in initializeGame
}
```

In `initializeGame()`, add initial force placement:
```cpp
// Place starting forces on map for each faction
for (int i = 0; i < playerCount; ++i) {
    players[i]->getFactionAbility()->placeStartingForces(ctx);
}
```

Add to FactionAbility:
```cpp
virtual void placeStartingForces(PhaseContext& ctx) {} // default: no placement
```

Harkonnen override places 10 units on Carthag.

### 2.3 Fremen (numbers only — no sandworm or storm card logic yet)

```cpp
int FremenAbility::getFreeRevivalsPerTurn() const { return 3; }
bool FremenAbility::canBuyAdditionalRevivals() const { return false; }
int FremenAbility::getBaseMovementRange() const { return 2; }
bool FremenAbility::requiresSpiceForFullUnitStrength() const { return false; }
bool FremenAbility::survivesWorm() const { return true; }  // wired in Ticket 2.4
bool FremenAbility::hasReducedStormLosses() const { return true; } // wired in Ticket 2.4

void FremenAbility::setupAtStart(Player* player) {
    player->setSpice(3);
    // 10 forces placed on Sietch Tabr, False Wall South, False Wall West
    // 10 forces in reserve
}
```

**AT START:** 10 forces distributed on Sietch Tabr + False Wall South + False Wall West, 10 reserve, 3 spice.

### 2.4 Wire survivesWorm and hasReducedStormLosses into phases

**`spice_blow_phase.cpp`** in `resolveWormOnTerritory`:
```cpp
// Before clearing unitsPresent, check each stack's faction:
for (const auto& stack : terr->unitsPresent) {
    FactionAbility* ability = ctx.getAbility(stack.factionOwner);
    if (ability && ability->survivesWorm()) {
        // Keep this stack — do not remove these units
        // Fremen ride the worm (movement handled post-nexus, defer to later ticket)
        continue;
    }
    totalUnitsKilled += stack.normal_units + stack.elite_units;
}
// Only remove non-Fremen stacks
```

**`storm_phase.cpp`** when applying storm damage:
```cpp
// For each unit stack in a storm-hit territory:
FactionAbility* ability = ctx.getAbility(stack.factionOwner);
int unitsKilled = (ability && ability->hasReducedStormLosses())
    ? (stack.normal_units + 1) / 2   // round up half
    : stack.normal_units;
```

### 2.5 Spacing Guild (numbers only — no cross-ship or out-of-turn yet)

```cpp
int SpacingGuildAbility::getShipmentCost(const territory* terr, int unitCount) const {
    // Guild pays half normal cost, rounded up
    int normalCost = FactionAbility::getShipmentCost(terr, unitCount);
    return (normalCost + 1) / 2;
}

void SpacingGuildAbility::setupAtStart(Player* player) {
    player->setSpice(5);
    // 5 forces in Tuek's Sietch, 15 in reserve
}
```

### 2.6 Acceptance criteria for Ticket 2

- Emperor receives spice from all treachery card purchases
- Harkonnen starts with 2 cards, draws bonus card on every win, can hold 8
- Harkonnen keeps all 4 traitor cards at game start
- Fremen revive 3 free (cannot buy more), move 2 territories base
- Fremen units survive sandworms
- Fremen units take half losses from storm
- Guild pays half shipment cost
- All starting forces placed correctly on map for each faction in play
- All 6 factions have correct starting spice per their player sheets

---

## Ticket 3 — GameActionBus for Shipment (first interception pipeline)

**Goal:** Introduce the mutable action + bus pattern for shipment only. This is the smallest surface area that validates the architecture, and touches the most faction interactions (Guild, Emperor, Fremen, BG).

**Do not start this ticket until Ticket 2 is merged and tested.**

### 3.1 Create `includes/headers/actions/game_action.hpp`

```cpp
#pragma once
#include <string>
#include <functional>
#include <vector>

/**
 * GameAction: a mutable proposal for something about to happen.
 * Distinct from GameEvent (which records what already happened).
 *
 * Phases create actions, dispatch them through the bus,
 * then execute only if !cancelled.
 */
struct GameAction {
    bool cancelled = false;
    bool stopPropagation = false;  // Set by Karama to cancel faction advantages
};

struct ShipmentAction : public GameAction {
    int factionIndex;
    std::string targetTerritory;
    int normalUnits;
    int eliteUnits;
    int cost;              // Modifiable — Guild halves it
    bool isFreeShipment;   // Fremen free shipment flag
    bool isCrossShip;      // Guild cross-ship flag
};

struct BonusShipmentAction : public GameAction {
    int beneficiaryFactionIndex;  // Who gets the bonus ship
    std::string targetTerritory;
    int normalUnits;
};
```

### 3.2 Create `includes/headers/actions/game_action_bus.hpp`

```cpp
#pragma once
#include "game_action.hpp"
#include <functional>
#include <map>
#include <vector>
#include <typeindex>

enum class ActionType {
    SHIPMENT,
    BONUS_SHIPMENT,
    BATTLE_PLAN,        // Ticket 4
    CARD_DEALT,         // Ticket 4
    REVIVAL,            // Ticket 4
};

enum class InterceptPriority : int {
    KARAMA          = 200,
    BENE_GESSERIT   = 100,
    ATREIDES        = 90,
    EMPEROR         = 50,
    HARKONNEN       = 50,
    GUILD           = 50,
    FREMEN          = 50,
    DEFAULT         = 0
};

class FactionAbility;

class GameActionBus {
public:
    using Handler = std::function<void(GameAction&)>;

    void subscribe(ActionType type, Handler handler, int priority = 0);
    void dispatch(ActionType type, GameAction& action);
    void clear();

private:
    struct Subscription {
        Handler handler;
        int priority;
        bool operator<(const Subscription& o) const { return priority > o.priority; }
    };
    std::map<ActionType, std::vector<Subscription>> subscriptions;
};
```

### 3.3 Add bus to PhaseContext

```cpp
struct PhaseContext {
    // existing fields...
    GameActionBus* actionBus;  // nullable — phases check before dispatching
};
```

Wire it in `Game` constructor: create one `GameActionBus` instance, pass pointer to PhaseContext.

### 3.4 Subscribe faction handlers at game init

In `initializeGame()`, after factions are created:
```cpp
for (int i = 0; i < playerCount; ++i) {
    players[i]->getFactionAbility()->subscribeToActionBus(*actionBus, i, ctx);
}
```

Add to FactionAbility:
```cpp
virtual void subscribeToActionBus(GameActionBus& bus, int myIndex, PhaseContext& ctx) {}
```

Guild override example:
```cpp
void SpacingGuildAbility::subscribeToActionBus(GameActionBus& bus, int myIndex, PhaseContext& ctx) {
    bus.subscribe(ActionType::SHIPMENT, [this, myIndex, &ctx](GameAction& action) {
        auto& ship = static_cast<ShipmentAction&>(action);
        if (ship.factionIndex == myIndex) {
            ship.cost = (ship.cost + 1) / 2;  // Guild pays half
        } else {
            // Guild receives payment from other factions
            ctx.players[myIndex]->addSpice(ship.cost);
            // The shipping player still pays to bank — this is additional income
            // Actually: other factions pay TO Guild, not bank. Adjust cost flow here.
        }
    }, static_cast<int>(InterceptPriority::GUILD));
}
```

### 3.5 Replace shipment execution in `ship_and_move_phase.cpp`

```cpp
// Before:
player->removeSpice(cost);
map.addUnitsToTerritory(territory, faction, units, 0);

// After:
ShipmentAction action;
action.factionIndex = player->getFactionIndex();
action.targetTerritory = territoryName;
action.normalUnits = normalUnits;
action.eliteUnits = eliteUnits;
action.cost = calculateDeploymentCost(terr, totalUnits);
action.isFreeShipment = false;
action.isCrossShip = false;

if (ctx.actionBus) {
    ctx.actionBus->dispatch(ActionType::SHIPMENT, action);
}

if (!action.cancelled) {
    player->removeSpice(action.cost);
    map.addUnitsToTerritory(action.targetTerritory, action.factionIndex,
                            action.normalUnits, action.eliteUnits);
    // log event
}
```

### 3.6 Wire BG piggyback shipment

In `BeneGesseritAbility::subscribeToActionBus`:
```cpp
bus.subscribe(ActionType::SHIPMENT, [this, myIndex, &ctx](GameAction& baseAction) {
    auto& ship = static_cast<ShipmentAction&>(baseAction);
    if (ship.factionIndex == myIndex) return; // Not our shipment
    if (baseAction.cancelled) return;

    // BG may ship 1 free advisor to Polar Sink
    BonusShipmentAction bonus;
    bonus.beneficiaryFactionIndex = myIndex;
    bonus.targetTerritory = "Polar Sink";
    bonus.normalUnits = 1;

    if (ctx.actionBus) {
        ctx.actionBus->dispatch(ActionType::BONUS_SHIPMENT, bonus);
    }
    if (!bonus.cancelled && ctx.players[myIndex]->getUnitsReserve() > 0) {
        ctx.map.addUnitsToTerritory("Polar Sink", myIndex, 1, 0);
        ctx.players[myIndex]->deployUnits(1);
        // log BG advisor placed
    }
}, static_cast<int>(InterceptPriority::BENE_GESSERIT));
```

### 3.7 Acceptance criteria for Ticket 3

- Guild receives all shipment payments from other factions (not Spice Bank)
- Guild pays half cost for its own shipments
- BG automatically places 1 unit in Polar Sink whenever any other faction ships
- Karama card can set `stopPropagation = true` on any ShipmentAction, cancelling faction advantages
- If no `actionBus` is set (nullptr), phases fall back to direct execution — no crash

---

## Ticket 4 — BattlePlanAction + Atreides Prescience

**Goal:** Extend the action bus to battle plan resolution. Implements Atreides as the first "information" faction. BG Voice deferred to Ticket 5.

**Do not start this ticket until Ticket 3 is merged and tested.**

### 4.1 Add BattlePlanAction to `game_action.hpp`

```cpp
struct BattlePlanRevealAction : public GameAction {
    int revealingFactionIndex;
    int opponentFactionIndex;

    // The four elements of a battle plan
    int wheelValue;
    int leaderIndex;           // index into alive leaders, -1 = cheap hero
    int weaponCardIndex;       // index into hand, -1 = none
    int defenseCardIndex;      // index into hand, -1 = none

    // Which element Atreides asked to see (set by prescience handler)
    enum class PrescientElement { NONE, LEADER, WEAPON, DEFENSE, NUMBER };
    PrescientElement prescientElementRevealed = PrescientElement::NONE;

    // Which element BG Voiced (set by BG handler, Ticket 5)
    enum class VoicedConstraint { NONE, MUST_PLAY_WEAPON, MUST_NOT_PLAY_WEAPON,
                                   MUST_PLAY_DEFENSE, MUST_NOT_PLAY_DEFENSE };
    VoicedConstraint voiceConstraint = VoicedConstraint::NONE;
};
```

### 4.2 Atreides ability — prescience

```cpp
void AtreidesAbility::subscribeToActionBus(GameActionBus& bus, int myIndex, PhaseContext& ctx) {
    bus.subscribe(ActionType::BATTLE_PLAN, [this, myIndex, &ctx](GameAction& action) {
        auto& plan = static_cast<BattlePlanRevealAction&>(action);

        // Only intercept when WE are the opponent (opponent is revealing against us)
        if (plan.opponentFactionIndex != myIndex) return;

        // Ask Atreides player (interactive) or AI which element to reveal
        // Interactive: print menu "Choose element to reveal: 1.Leader 2.Weapon 3.Defense 4.Number"
        // AI: always ask for weapon (highest strategic value)
        plan.prescientElementRevealed = BattlePlanRevealAction::PrescientElement::WEAPON;

        // Log what was revealed to Atreides player
        // The battle_phase then uses plan.prescientElementRevealed
        // to show that element to the Atreides player before committing their own plan
    }, static_cast<int>(InterceptPriority::ATREIDES));
}
```

### 4.3 Wire into battle_phase.cpp

Before each player commits their battle plan:
```cpp
BattlePlanRevealAction action;
action.revealingFactionIndex = opponentIdx;
action.opponentFactionIndex = attackerIdx;
// ... fill in plan details

if (ctx.actionBus) {
    ctx.actionBus->dispatch(ActionType::BATTLE_PLAN, action);
}

// If Atreides prescience fired, show the revealed element to Atreides
if (action.prescientElementRevealed != BattlePlanRevealAction::PrescientElement::NONE) {
    // display revealed element to Atreides player
}
```

### 4.4 Atreides bidding prescience

In `AtreidesAbility`:
```cpp
void onCardDealtForBidding(const treacheryCard& card, PhaseContext& ctx) override {
    // Atreides sees the card before bidding starts
    // In interactive mode: display card name and type to Atreides player
    // In AI mode: store card info for AI decision-making
    // Log: "Atreides peeks at card: [name]"
}
```

Wire in `bidding_phase.cpp` after dealing each card:
```cpp
for (int i : eligiblePlayers) {
    FactionAbility* ability = ctx.getAbility(i);
    if (ability) ability->onCardDealtForBidding(card, ctx);
}
```

### 4.5 Acceptance criteria for Ticket 4

- Atreides player sees each treachery card before bidding starts
- Atreides can ask to reveal one element of opponent's battle plan
- If opponent has no weapon and Atreides asks for weapon, opponent says "none" and Atreides cannot ask again (per rulebook)
- Priority ordering: prescience fires AFTER BG Voice would (priority 90 < 100) — BG not yet implemented but ordering is already correct

---

## Ticket 5 — Bene Gesserit (most complex faction)

**Goal:** Implement BG completely. This is last because it depends on the mature hook interface.

**Do not start this ticket until Ticket 4 is merged and tested.**

### 5.1 BG has unique unit state: Advisor / Fighter

Add to `unitStack` struct in `map.hpp`:
```cpp
struct unitStack {
    int factionOwner;
    int normal_units;
    int elite_units;
    bool isAdvisor;  // BG-specific: advisors coexist, cannot fight/collect/block
};
```

### 5.2 BG Voice via BattlePlanRevealAction

```cpp
void BeneGesseritAbility::subscribeToActionBus(GameActionBus& bus, int myIndex, PhaseContext& ctx) {
    bus.subscribe(ActionType::BATTLE_PLAN, [this, myIndex, &ctx](GameAction& action) {
        auto& plan = static_cast<BattlePlanRevealAction&>(action);
        if (plan.opponentFactionIndex != myIndex) return;

        // BG fires at priority 100, BEFORE Atreides (90)
        // Voice: force opponent to play or not play a specific card type
        // Interactive: ask BG player what to voice
        // Set plan.voiceConstraint accordingly
        // If opponent cannot comply, they may do as they wish (rulebook)
    }, static_cast<int>(InterceptPriority::BENE_GESSERIT));
}
```

### 5.3 BG Prediction win condition

In `checkSpecialVictory()`:
```cpp
bool BeneGesseritAbility::checkSpecialVictory(PhaseContext& ctx) const {
    // If the faction BG predicted wins on the turn BG predicted
    // AND BG made the prediction — BG wins alone instead
    // Requires storing prediction (faction + turn) at game setup
    return (predictedFaction == actualWinnerFaction && predictedTurn == ctx.turnNumber);
}
```

Wire in `mentat_pause_phase.cpp` before standard victory check:
```cpp
for (int i = 0; i < ctx.playerCount; ++i) {
    FactionAbility* ability = ctx.getAbility(i);
    if (ability && ability->checkSpecialVictory(ctx)) {
        // BG wins alone — override any other winner
        ctx.gameEnded = true;
        return;
    }
}
```

### 5.4 BG CHOAM charity

Already wired via `alwaysReceivesCharity()` from Ticket 1.

### 5.5 BG Karama — worthless cards as Karama

```cpp
// BG can play any worthless card as a Karama card
// This is a battle phase / general phase hook
// When BG plays a card, check if it's worthless type
// If so, treat as Karama: set stopPropagation on the relevant action
```

### 5.6 Acceptance criteria for Ticket 5

- BG places 1 advisor in Polar Sink per opponent's shipment (from Ticket 3)
- Advisors coexist with other factions, cannot collect spice, cannot battle, cannot block strongholds
- Advisors flip to fighters when BG announces before battle (post-Nexus)
- BG Voice fires before Atreides prescience
- If BG Voice is complied with and Atreides still asks — prescience resolves normally on what was actually played
- BG prediction stored at game start, checked each Mentat Pause
- BG always receives 2 CHOAM charity regardless of spice
- BG worthless cards can be played as Karama

---

## Ticket 6 — Guild special mechanics

**Goal:** Complete Guild implementation (cross-ship, out-of-turn, special victory).

**Do not start this ticket until Ticket 5 is merged and tested.**

### 6.1 Cross-ship (territory to territory)

Add `CrossShipAction` to `game_action.hpp`. In `ship_and_move_phase.cpp`, if the active player is Guild, offer a third shipment option alongside normal deploy and skip.

### 6.2 Out-of-turn movement

Guild can declare "I go now" at any point during the ship-and-move loop. Implement by allowing Guild to interrupt the turn order loop:
```cpp
// In the ship-and-move loop, before each player's turn:
if (guildHasNotMovedThisTurn && guildWantsToGoNow()) {
    executeGuildTurn();
    guildHasNotMovedThisTurn = false;
}
```

### 6.3 Special victory condition

In `checkSpecialVictory()`:
```cpp
bool SpacingGuildAbility::checkSpecialVictory(PhaseContext& ctx) const {
    // If game reaches max turns with no winner, Guild wins
    return ctx.turnNumber >= MAX_TURNS;
}
```

Wire in mentat_pause alongside BG special victory check.

---

## Ticket 7 — Fremen advanced mechanics

**Goal:** Complete Fremen (sandworm ride, storm cards, Fedaykin).

**Do not start this ticket until Ticket 6 is merged and tested.**

### 7.1 Sandworm ride

After worm resolves and Nexus fires, Fremen player may move surviving forces to any territory. This is a movement outside the normal movement phase — add a `WormRideAction` and handle it in `spice_blow_phase.cpp` post-nexus.

### 7.2 Storm deck control (advanced game)

Fremen controls storm movement using their Storm Deck (6 cards, 1–6). Replace the random storm card draw with a lookup to the Fremen player's face-down Storm Card when Fremen is in the game.

### 7.3 Fedaykin elite units

Fedaykin (starred Fremen forces) worth 2 in battle. Wire via `getEliteUnitStrength()` hook in FactionAbility (add this hook). Only 1 Fedaykin can be revived per turn — add enforcement in revival phase.

### 7.4 Fremen free shipment

Fremen can bring any number of reserves for free to Great Flat or within 2 territories of Great Flat. Wire via a new `getFreeFactionShipmentTerritory()` hook that returns `"The Great Flat"` for Fremen. Ship-and-move phase checks this and sets `ShipmentAction.isFreeShipment = true`.

### 7.5 Fremen special victory

```cpp
bool FremenAbility::checkSpecialVictory(PhaseContext& ctx) const {
    // If game ends with no winner AND
    // (Fremen or no one) occupies Sietch Tabr AND Habbanya Sietch
    // AND neither Harkonnen, Atreides, nor Emperor occupies Tuek's Sietch
    // Fremen wins
}
```

---

## Ticket 8 — Advanced game (post-basic completion)

**Do not start until all basic game factions are complete and the game runs 10 turns correctly.**

- Double Spice Blow (two cards per turn, two discard piles A and B)
- Advanced combat (spice supports units at full strength, half-strength otherwise)
- Advanced Karama powers (unique per faction)
- Increased Spice Flow (Arrakeen +2, Carthag +2, Tuek's Sietch +1 per turn)
- BG Fighter/Advisor split unit rendering

---

## Architecture rules — never break these

1. `GameAction` (mutable, future) and `GameEvent` (immutable, past) are always two separate concepts. Never log a `GameAction` as if it already happened.
2. No `if (factionName == "Atreides")` in any phase file. All faction branching lives in FactionAbility subclasses.
3. Phases call hooks. Factions implement hooks. Phases never know faction names.
4. If `actionBus` is null, phases execute directly (no crash). The bus is an enhancement, not a requirement.
5. Each ticket is a single compilable, testable unit. No ticket should break existing tests.
6. BG is always last. It is the most complex faction and its Voice/Prescience interaction is the hardest edge case in the game.

---

## File structure after all tickets complete

```
includes/headers/
  factions/
    faction_ability.hpp
    atreides_ability.hpp
    harkonnen_ability.hpp
    fremen_ability.hpp
    emperor_ability.hpp
    spacing_guild_ability.hpp
    bene_gesserit_ability.hpp
  actions/
    game_action.hpp
    game_action_bus.hpp

srcs/
  factions/
    faction_ability.cpp
    atreides_ability.cpp
    harkonnen_ability.cpp
    fremen_ability.cpp
    emperor_ability.cpp
    spacing_guild_ability.cpp
    bene_gesserit_ability.cpp
  actions/
    game_action_bus.cpp
```

No other files need new directories. All phase files stay in their current locations.
