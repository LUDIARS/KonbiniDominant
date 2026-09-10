# Skill upgrades baseline

BASE-KD-SKILLS-01, requested 2026-09-09. This extends the v4 campaign.
The player retains the one selected chain. All eleven skills are available to every chain.

## Progression
- Start at level 1. Served customers generate XP every economy period while at least one player store survives.
- XP gain is 2 + floor(customers / 50), capped at 20. The next level needs 80 + 20 * (level - 1).
- On crossing the threshold, offer up to three distinct skills, sampled without replacement from a seeded pool.
- Each skill has five ranks. Up to six types can be equipped; after six, only owned non-maxed skills are offered.
- At maximum ranks no new offer is created. Overflow XP is retained.
- Press 1/2/3 or click a choice. All gameplay clocks, economy, AI, threats and periodic skills freeze while choosing.
- A choice transaction may advance the command sequence clock, but advances no gameplay time.
- Retry clears skills, XP, pending choices and timers. The same seed and decisions reproduce the same offers.
- No reroll, meta progression, item evolution or skill-combination evolution is included in this baseline.

## Eleven skills
Names in parentheses are the ASCII labels in the current renderer.
Values below are per rank unless otherwise noted and are data-configurable.

| Name | Effect |
|---|---|
| 多言語スタッフ (MULTILINGUAL STAFF) | +1 faith growth each economy period |
| 宅配便 (DELIVERY) | +8 percent store reach, applied to existing and future stores |
| カッフェマシン (COFFEE MACHINE) | Every 10 seconds, 20 credits per surviving store, up to 32 stores |
| パンとドーナツ (BREAD AND DONUTS) | +2 recovery for population served by the player's stores |
| ホットスナック (HOT SNACKS) | Every 10 seconds, +1 chain influence for 3 seconds inside store reach |
| こだわり素材 (QUALITY INGREDIENTS) | +10 percent store revenue |
| 居抜き出店 (REUSED SHOP) | -8 percent construction cost; escape's store cost is discounted too |
| 防犯カメラ (SECURITY CAMERA) | +1 second before encirclement destroys a player store |
| 深夜シフト (NIGHT SHIFT) | -8 percent interval for Coffee, Hot Snacks and Mind Wave |
| 上底テクニック (RAISED BOTTOM) | +20 percent revenue, -1 faith growth; faith remains clamped to 0..100 |
| 怪音波 (MIND WAVE) | Every 15 seconds, residents within player-store reach temporarily go to a player store for 2 + 0.5 * rank seconds |

Coffee and Hot Snacks share a base interval but have separate timers. Newly acquired periodic skills activate on the next gameplay tick.
Mind Wave selects one nearest live player store per cell, preserves dimension separation, never duplicates residents/income, and returns to normal competition when the pulse expires.
Security Camera does not delay Aion's dimension-wide MaxValue collapse.
Revenue bonuses from Quality and Raised Bottom add before multiplying triangle/convenience revenue; final store multiplier caps at 10x.
Faith updates start in Phase 1, while maximum-faith influence doubling unlocks in Phase 2.
Presentation includes XP, loadout ranks, active pulse labels, a purple wave/range pulse, and the choice overlay.

## Validation limits
Build and source/artifact checks only. No unit, integration, runtime, or balance playtests were authorized or executed.
