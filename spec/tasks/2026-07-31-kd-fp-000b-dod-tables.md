---
task: kd-fp-000b-dod-tables
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000-deterministic-primitives.md
  - spec/data/world-state.md
  - spec/design.md
---

# KD-FP-000B — DoD identity pools and SoA tables

## 目的

deterministic primitives の上へ generational entity identity と、
facility / population cell / store の dense SoA storage を実装する。

## 完了条件

- stale handle を generation で検出できる typed entity ID pool を実装する
- Figmentum facility identity と game entity identity を分離する
- facility / population / store row を dense columns へ保持する
- append 前に全 column capacity を確保し、partial row を残さない
- sparse lookup と row projection の境界を各 table に閉じ込める
- `konbini_sim` と review aggregate を source/link-complete にする
- test の build / 実行、game 起動はこの task では行わない

## スコープ

- `include/konbini/sim/`
- `src/sim/`
- `spec/tasks/`

economy state、render snapshot、city manifest、Figmentum adapter は後続 task に分離する。

## 実装結果 (2026-07-31)

- generational ID pool と3つの SoA table を実装済み
- geometric reserve helper で dense append の exception boundary を統一済み
- unit / integration / startup test は未実行
