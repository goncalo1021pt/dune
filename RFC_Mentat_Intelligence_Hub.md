# RFC: Mentat Intelligence Hub (Project "Arrakeen")

**Status:** Draft / Proposal  
**Target User:** House Atreides + 1 Designated Ally  
**Primary Goal:** To digitize the "Prescience" ability and provide a secure, real-time intelligence-sharing platform that remains discreet during physical tabletop play.

---

## 1. Objective
In the *Dune* board game, House Atreides possesses superior information (knowing the next spice blow, bidding cards, and opponent battle plans). Manually tracking this information while managing social dynamics is mentally taxing. 

**Project Arrakeen** aims to:
* Provide a centralized **Intelligence Ledger** for spice, cards, and traitors.
* Automate **Battle Probability** calculations based on known opponent hand data.
* Enable **Secret Coordination** with an ally through a low-latency, filtered data stream.

---

## 2. Feature Set

### A. The Intelligence Ledger
* **Treachery Tracker:** A grid-based UI to record cards acquired during the bidding phase. 
    * *Atreides View:* Full list of known cards across all factions.
    * *Prescience Mode:* A "peek" function to log the top card of the deck before bidding starts.
* **Spice Ledger:** A calculator that tracks the economy. It automatically attributes spice to the Emperor (bidding) or the Spacing Guild (shipping) based on inputs.
* **Traitor Records:** A secure list of known traitors and "safe" leaders to prevent accidental losses during combat.

### B. The Kwisatz Haderach Engine (Battle Simulator)
A combat calculator that factors in:
* **Known Cards:** Highlights if an opponent *cannot* defend against a specific weapon.
* **Tie-Breaker Rules:** Configurable logic (e.g., Attacker wins, Defender wins, or Physical Strength totals).
* **Risk Analysis:** Alerts the user to the probability of a "Traitor" card being held by the opponent.

### C. The "Secret Distrans" (Ally Sync)
* **Selective Sharing:** The Atreides player can "toggle" specific pieces of intel (a card, a spice count, or a traitor) to become visible on the Ally’s device.
* **Encrypted Chat:** A built-in, real-time chat for tactical planning without leaving the table or speaking aloud.

---

## 3. Technical Stack
The application is built on the **Cloudflare Unified Platform** for maximum speed and zero-infrastructure management.

| Component | Technology | Role |
| :--- | :--- | :--- |
| **Frontend** | **Svelte + Vite** | High-performance, reactive UI with a small footprint. |
| **Styling** | **Tailwind CSS** | Mobile-first "Stealth/Dark Mode" interface. |
| **Backend API** | **Hono.js** | Lightweight router running on Cloudflare Workers. |
| **Real-time Sync** | **Durable Objects** | Manages the "War Room" state and WebSocket connections. |
| **Static Hosting** | **Workers Assets** | Serves the Svelte frontend directly from the Worker. |
| **Persistence** | **Cloudflare D1** | SQL database for game history and session recovery. |

---

## 4. System Architecture & Information Flow
To ensure privacy and low latency, the app uses a "Single Source of Truth" model via Durable Objects.

1.  **Connection:** Players join a session via a unique Game ID.
2.  **State Management:** The Durable Object maintains the master JSON of all game data.
3.  **Filtering:** When the DO broadcasts updates, it checks the user’s role. 
    * *If Role == Atreides:* Receive all data.
    * *If Role == Ally:* Receive only data flagged as `shared: true`.
4.  **Stealth UI:** The frontend is designed to look like a system monitor (green/amber text on black) to remain inconspicuous to other players at the table.

---

## 5. Proposed House Rule Configurations
The app will allow the host to toggle common "House Rules" at the start of the session to ensure the Battle Simulator is accurate:
* **Tie-Breakers:** Attacker Wins (Standard) vs. Defender Wins vs. Highest Units on Territory.
* **Fremen Buffs:** Whether Fedaykin count as 2 strength in all conditions.
* **Harkonnen Mulligan:** Starting traitor count adjustments.

---

## 6. Security & Discretion
* **No Auth Required:** Quick entry via Room Code to avoid "fiddling" with accounts at the table.
* **Burn Command:** A quick-swipe action to clear the screen or wipe the session data.
* **Haptic Cues:** Using the Web Vibrations API to notify the Ally of new shared intel silently.

---

> **Next Steps:**
> 1. Finalize the JSON schema for the Treachery Card database.
> 2. Bootstrap the Svelte project with Workers Assets configuration.
> 3. Implement the WebSocket "Selective Broadcast" logic in the Durable Object.
