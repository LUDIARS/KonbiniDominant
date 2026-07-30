---
task: kd-fp-000d-simulation-systems
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000c-model-city.md
  - spec/data/world-state.md
  - spec/feature/chain-selection.md
  - spec/feature/economy-and-population.md
  - spec/feature/phase-1-dominant-triangle.md
---

# KD-FP-000D — Deterministic commands and simulation systems

## 目的

DoD state へ順序付き command、chain 選択、店舗配置、population 初期化、
ZOC 割当、定期収益を適用する deterministic system 群を実装する。

## 完了条件

- command を tick、source priority、sequence、payload の順に決定的に処理する
- chain 選択と placement validation を game phase / ownership / cash で検証する
- structural placement を batch 単位の stage-and-swap で確定する
- population を counter RNG と stable Figmentum facility key から初期化する
- ZOC の距離 tie を StoreId 昇順で解決する
- ZOC、population、economy の例外時に authoritative state を部分更新しない
- milli-credit 収益を中間 overflow なしで計算し HUD と決算で共有する
- `konbini_sim` と review aggregate を source/link-complete にする
- unit / integration / startup test はこの task では実行しない

## スコープ

- `include/konbini/sim/`
- `src/sim/`
- `spec/tasks/`

content loader、fixed-tick composition root、Figmentum adapter、rendering、
native app は後続 task に分離する。

## 実装結果 (2026-07-31)

- command queue と simulation system 群を実装済み
- placement / population / ZOC / economy の強い例外境界を追加済み
- HUD と economy が共有する checked revenue math を追加済み
- unit / integration / startup test は未実行
