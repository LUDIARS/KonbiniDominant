---
task: kd-fp-000-deterministic-primitives
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/design.md
  - spec/data/world-state.md
  - spec/data/save-format.md
  - spec/feature/chain-selection.md
  - spec/interface/figmentum-city-generation.md
---

# KD-FP-000 — Deterministic simulation primitives

## 目的

first playable の DoD simulation が共有する chain identity、counter-based RNG、
Figmentum facility key、有限 world geometry の最小基盤を先行実装する。

## 完了条件

- save / content data と互換な `ChainId` と slug を定義する
- `kRandomAlgorithmId` で永続化契約を識別できる counter RNG を実装する
- Figmentum の stable facility identity を game 側の型へ隔離する
- meter 座標と finite / ordered bounds validation を定義する
- `konbini_sim` と review aggregate を source/link-complete にする
- test source は登録するが、この task では build / 実行しない
- local PR の登録 build を通し、Revisor review を完了する

## スコープ

- `.gitignore`
- `CMakeLists.txt`
- `include/konbini/sim/`
- `src/`
- `tests/`
- `README.md`
- `spec/setup/native-development.md`
- `spec/tasks/`

Pictor / Ergo / Figmentum の dependency 取得、game app、起動確認は後続 task に分離する。

## 実装結果 (2026-07-31)

- deterministic primitives と `konbini_sim` target を実装済み
- test source と opt-in CMake target を登録済み
- unit / integration / startup test は未実行
