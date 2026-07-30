---
task: kd-fp-000c-model-city
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000b-dod-tables.md
  - spec/data/world-state.md
  - spec/interface/figmentum-city-generation.md
  - spec/feature/economy-and-population.md
---

# KD-FP-000C — Economy snapshot and city model

## 目的

DoD table の上へ chain economy と read-only render snapshot を構築し、
Figmentum の semantic CityPlan を受け取る game-owned manifest、
canonical serialization、facility projection、geometry cache の境界を実装する。

## 完了条件

- chain ごとの cash、store count、customer share、tick 収支を SoA で保持する
- render thread へ mutable simulation state を渡さない immutable snapshot を作る
- predicted revenue を `int64_t` の範囲で正しく計算し、overflow を fail-fast する
- CityManifest が generator revision、seed、station、facility ordering を検証する
- manifest canonical bytes と hash が input ordering に依存しない
- Figmentum facility key と game entity ID を混同せず facility table へ投影する
- facility geometry cache を recipe/revision/resolution/version で識別する
- `konbini_sim`、`konbini_city`、review aggregate を source/link-complete にする
- unit / integration / startup test はこの task では実行しない

## スコープ

- `include/konbini/sim/`
- `include/konbini/city/`
- `src/sim/`
- `src/city/`
- `spec/tasks/`

command processing、fixed-tick simulation、Figmentum adapter、rendering、
native app は後続 task に分離する。

## 実装結果 (2026-07-31)

- economy model、render snapshot、city semantic/canonical model を実装済み
- Figmentum adapter を後段で差し込む `ICityGenerator` 境界を追加済み
- unit / integration / startup test は未実行
