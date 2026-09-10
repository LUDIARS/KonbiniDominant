# Phase 1 game loop

2026-09-09: owner approved the rules below and delegated remaining implementation
decisions. This bounded match ends in Result; Phase 2 is not entered.

## Match contract

- Choose one of three chains. All three receive five stores' worth of starting
  cash; the other two are AI controlled. No free or preplaced stores.
- Place on an intact buildable facility or a destroyed store's lot, spending
  the chain's construction cost atomically. Protected facilities stay protected.
- Store influence is circular on the XZ plane. Most covering stores wins a cell;
  ties retain its previous owner if eligible, otherwise the nearest store wins
  (stable ID breaks equal distance). Each resident contributes revenue once.
- Same-chain Delaunay triangles form from active stores, with configurable
  maximum edge and minimum area. Collinear or coincident points cannot form one.
- A triangle adds influence inside it and increases participating stores'
  revenue. Overlapping triangles use the maximum bonus, never multiply it.
- An enemy store continuously enclosed for three seconds is destroyed
  automatically. Breaking the enclosing triangle cancels that threat. Mature
  captures resolve simultaneously against the same tick state; afterwards all
  triangles, customers and income are rebuilt before economy/result evaluation.
- Victory: no active rival stores AND at least 60 percent of current population
  belongs to the player. This is checked only after both rivals have opened.
- Defeat: zero player stores and less cash than the cheapest available store.
- After five minutes, the largest captured population wins. A tie for first
  involving the player is a draw; otherwise the player loses.
- Result freezes the simulation and rejects gameplay commands. Retry clears
  commands, cash, stores, population changes, capture timers and match counters,
  returns to chain selection, and reuses the deterministic city geometry.

## Adjustable baseline

Content version 3 contains a required `phase1` object. Version 2 remains the
explicit historical first-playable profile; a missing v3 field is an error.
Match length, capture delay, domination threshold, triangle limits/bonus,
AI cadence and population loss/recovery are content values, not UI constants.
Population lost when replacing/destroying a facility recovers up to its original
capacity when served. These baseline values are tuning decisions, not final balance.

## Tick order and ownership

Process player commands, activate rivals on chain selection, commit placement,
choose/commit AI placements through the same validator, advance the match clock,
derive triangles, update capture timers, apply mature destructions simultaneously,
derive surviving triangles, update population, assign customers, collect income,
resolve defeat/victory/timeout, and publish immutable snapshots.

Store/cell storage remains flat and ID based. Triangle/capture/outcome/AI systems
are separate from input, rendering and the simulation composition root.
Only internal AI generation may submit commands with AI source priority.

## Delivery and validation

Tasks: content/state; three-chain placement; competitive influence; triangles;
capture lifecycle; AI; result/retry; HUD and world overlays.
Compile without tests; do not run unit/integration/startup tests unless separately
authorized. Native startup remains Excubitor-only in the project main directory.
