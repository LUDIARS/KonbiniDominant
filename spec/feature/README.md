# feature/ — ゲーム機能

- [game-flow.md](game-flow.md) — 起動からResultまで
- [chain-selection.md](chain-selection.md) — 3chainと固有特性
- [economy-and-population.md](economy-and-population.md) — 資金、人口、収益
- [phase-1-game-loop.md](phase-1-game-loop.md) — Phase 1 の勝敗、tick順序、競合ルール
- [phase-1-dominant-triangle.md](phase-1-dominant-triangle.md)
- [phase-2-vertical-invasion.md](phase-2-vertical-invasion.md)
- [phase-3-multiverse.md](phase-3-multiverse.md)
- [boss-aion.md](boss-aion.md)
- [full-campaign-baseline.md](full-campaign-baseline.md) — Phase 1–4 の実装 baseline
- [skill-upgrades.md](skill-upgrades.md) — 11種のスキルと進行
- [opponent-ai.md](opponent-ai.md)
- [grid-town-and-vector-ui.md](grid-town-and-vector-ui.md) — グリッド都市とベクタUI
- [pointer-controls.md](pointer-controls.md) — マウス／タッチ操作
- [store-construction-effects.md](store-construction-effects.md) — 出店演出
- [three-store-brands.md](three-store-brands.md) — 店舗外観
- [ui-ux.md](ui-ux.md)
- [npc-conversations-and-placement-feedback.md](npc-conversations-and-placement-feedback.md) — 住民の来店・発話と店舗着地feedback

各fileはプレイヤーから見た振る舞いを正本とし、data layoutや外部APIの詳細は
`data/`、`interface/`へ委譲する。
