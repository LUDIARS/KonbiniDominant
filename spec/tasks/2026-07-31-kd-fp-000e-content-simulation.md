---
task: kd-fp-000e-content-simulation
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000d-simulation-systems.md
  - spec/data/content-schema.md
  - spec/data/world-state.md
  - spec/plan/tasks/first-playable.md
---

# KD-FP-000E — Content and transactional simulation

## 目的

versioned first-playable content、canonical snapshot、immutable render snapshotを
既存DoD systemへ接続し、commandからsnapshot生成までを一つの決定的tickとして
実行するcomposition rootを作る。

## 完了条件

- strict JSON loaderがfirst-playable content v1を検証する
- JSON整数fieldを浮動小数点へ丸めずcanonicalな10進lexemeから検証する
- programmaticに構築したcontentもauthoritative simulationへ入る前に検証する
- command queue、全mutable table、economy、ID poolをstaged copy上で処理する
- canonical snapshotとimmutable render snapshotが両方成功してからtickをcommitする
- 例外時にcommand消失、部分的なcash/ownership変更、tick進行を残さない
- `konbini_review` が追加sourceを含む `konbini_sim` をbuild対象にする
- unit / integration / startup testはこのtaskでは実行しない

## スコープ (編集可ディレクトリ)

- `data/content/`
- `include/konbini/sim/`
- `src/sim/`
- `spec/data/`
- `spec/plan/tasks/`
- `spec/tasks/`

Figmentum city adapter、Pictor / Ergo render、native app、Excubitor起動確認は
後続taskに分離する。

## 実装結果 (2026-07-31)

- content v1 loaderとstrict validationを実装済み
- canonical snapshotとfirst-playable simulationを実装済み
- whole-tick strong exception guaranteeを実装済み
- Revisorのconfigure / build結果はPR reviewで記録する
- unit / integration / startup testは未実行
