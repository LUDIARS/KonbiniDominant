---
task: kd-npc-003-figmentum-pedestrian-path
project: KonbiniDominant
kind: 実装
status: in_review
created: 2026-08-01
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 684
actio_task_id: eef1109b-acf4-4de3-91ff-ff87970a8373
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

## Upstream status

Figmentum #2241 が main `d0437cd5cbf8721faec5267cc1e4dd2ce55d6fd0`
(feat(city): derive a versioned pedestrian path network from CityPlan) で merge 済み。
KD 側の consumer は同 revision を exact pin する。

## 完了条件

- review済みFigmentum revisionをexact pinする
- CityManifestへpedestrian path contractを投影する
- home entranceからassigned store entranceまでstable routeを選ぶ
- 同距離routeのtie-breakをstable edge keyで固定する
- resident snapshotがsegment上のpositionとyawを派生する
- 到達不能時は明示状態を返し、直線へsilent fallbackしない
- canonical gameplay stateと人口／収益を変更しない

## KD consumer の実装位置

| 完了条件 | 実装 | test |
|---|---|---|
| exact pin | `src/adapters/figmentum/CMakeLists.txt`、`city::kFigmentumRevision` | `tests/adapters/figmentum_pedestrian_path_test.cpp` |
| CityManifest 投影 | `src/adapters/figmentum/figmentum_pedestrian_projection.cpp`、`src/city/pedestrian_path_contract.cpp` | 同上 |
| stable route / edge key tie-break | `src/sim/snapshots/pedestrian_route.cpp` | `tests/sim/pedestrian_path_walking_test.cpp` |
| segment 上の position / yaw | `src/sim/snapshots/resident_presentation.cpp` | 同上 |
| 到達不能の明示状態、gameplay 不変 | 同上、`src/sim/first_playable_simulation.cpp` | 同上 |

`src/app/simulation_host.cpp` は manifest から path table を投影して simulation へ
渡す配線だけを持つ。

## 検証 (2026-10-02)

- 引継ぎ委託: Actio `1f0d75a4-5a80-40f1-b1c6-44975f9e8fe8` (前任 run 9c330374 の続き)
- Windows x64 / VS2022 / Debug / `KONBINI_BUILD_TESTS=ON` で全体ビルド成功、ctest 18/18 pass
- `figmentum_pedestrian_path_test`: pin した Figmentum は params の格子拡大を
  検出しない (施設 cell が格子内に収まり station anchor が一致すれば通る)。
  stationAnchor のずれ (PlanMismatch) と偶数 blocks (InvalidParams) が
  `std::runtime_error` の load 失敗になることを確認する
- `pedestrian_path_walking_test`: phase1 の 60 tick で canonical bytes / hash、
  人口、現金、予測収入が path 有無で一致し、`PedestrianPath` の resident が現れる
- 未実施: アプリ起動、Release / mobile / web ビルド

## スコープ (編集可ディレクトリ)

- Figmentum: `include/figmentum/`、`src/`、`spec/`、`tests/`
- KonbiniDominant: `include/konbini/city/`、`include/konbini/sim/`
- KonbiniDominant: `src/city/`、`src/sim/`、`src/adapters/figmentum/`
- KonbiniDominant: `spec/`、`tests/`

## Delivery

- Figmentum変更とKonbini consumer変更を別PRにする
- 各PRはtask-workflow 2.1 taskへlinkする
- 起動確認はruntime接続完了後にExcubitor / TestWorkflow経由で行う
