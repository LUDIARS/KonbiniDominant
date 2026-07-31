---
task: kd-npc-003-figmentum-pedestrian-path
project: KonbiniDominant
kind: 実装
status: pending
created: 2026-08-01
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 684
actio_task_id: null
memory_links:
  - spec/feature/npc-conversations-and-placement-feedback.md
  - spec/interface/figmentum-city-generation.md
  - spec/tasks/2026-08-01-kd-npc-001-visia-presentation.md
---

# KD-NPC-003 — Figmentum pedestrian-path integration

## 目的

first playableの施設間直線dummyを、Figmentumが所有するversion付きの道路／歩道
semantic pathへ置き換え、residentが都市形状に沿って店舗へ往復できるようにする。

## Upstream prerequisite

Figmentum専用branch / task / PRで、`CityPlan`から次を安定して公開する。

- pedestrian node / edgeのstable key
- facility entranceと最寄りnodeの対応
- meter座標とdimension非依存のcanonical順
- seed / recipe versionからの決定的再生成
- 到達不能、退化edge、非finite座標のerror contract

KonbiniDominant側へ独自の偽道路graphを正本として追加しない。

## 完了条件

- review済みFigmentum revisionをexact pinする
- CityManifestへpedestrian path contractを投影する
- home entranceからassigned store entranceまでstable routeを選ぶ
- 同距離routeのtie-breakをstable edge keyで固定する
- resident snapshotがsegment上のpositionとyawを派生する
- 到達不能時は明示状態を返し、直線へsilent fallbackしない
- canonical gameplay stateと人口／収益を変更しない

## スコープ (編集可ディレクトリ)

- Figmentum: `include/figmentum/`、`src/`、`spec/`、`tests/`
- KonbiniDominant: `include/konbini/city/`、`include/konbini/sim/`
- KonbiniDominant: `src/city/`、`src/sim/`、`src/adapters/figmentum/`
- KonbiniDominant: `spec/`、`tests/`

## Delivery

- Figmentum変更とKonbini consumer変更を別PRにする
- 各PRはtask-workflow 2.1 taskへlinkする
- 起動確認はruntime接続完了後にExcubitor / TestWorkflow経由で行う
