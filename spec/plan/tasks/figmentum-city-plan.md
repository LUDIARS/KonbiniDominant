---
task_id: KD-FG-001
title: Figmentum semantic CityPlan
status: done
target_repository: LUDIARS/Figmentum
merged_revision: 3ee998f487d984f54003c4ec3c4f7ba00b53eec3
---

# KD-FG-001 — Figmentum semantic CityPlan

## Outcome

都市のsemantic placementをFigmentumの責務として生成し、station anchor、
stable facility identity、施設単位の建物recipeをconsumerへ返す最小 `CityPlan`
APIをFigmentumへ追加する。

KonbiniDominantにcity placement algorithmを複製せず、
[KD-FP-001](first-playable.md) が一区画を操作可能にするためのupstream task。

## Required reading

- Figmentum `CLAUDE.md`
- Figmentum `include/figmentum/gen/city.h`
- Figmentum `include/figmentum/gen/building.h`
- Figmentum `spec/feature/scene-gen.md`
- Figmentum `spec/feature/cityplan-roads.md`
- [../../interface/figmentum-city-generation.md](../../interface/figmentum-city-generation.md)
- [../implementation-roadmap.md](../implementation-roadmap.md)

## Contract

実際のC++命名はFigmentum規約へ合わせるが、公開契約は次を満たす。

```text
CityPlanParams + seed
  → CityPlan
       schemaVersion
       seed
       stationAnchor
       facilities[] (FacilityPlan)

FacilityPlan
  stable FacilityId
  canonical lot / cell coordinate
  semantic role
  BuildingParams
  conservative bounds
```

- `CityPlan`生成そのものがFigmentum内部にある
- stationはplanの中央anchorとして識別可能
- station以外に複数のinteractive facilityを返す
- `FacilityId` はmesh term順、vector挿入順、LOD sort順から独立
- facilitiesは`FacilityId`昇順のcanonical order
- 同じversion / params / seedから全fieldが同一のplanを返す
- facilityごとに既存 `generateBuilding()` / `buildingBounds()` へ渡せる
- 座標単位は `1 unit = 1 m`
- algorithm変更時にconsumerが不一致を検出できるschema / recipe versionを持つ

乱数は明示seedから局所的に導出し、global RNGや呼出順へ依存しない。
新APIは既存の`figmentum_core` targetから公開し、consumerが同targetをlinkして
app process内から直接利用できる。

## Compatibility

- 既存 `generateCity()` / `buildCity()` の公開動作を壊さない
- Pictor、Ergo、Vulkan、KonbiniDominantの型へ依存しない
- game固有のchain、人口、所有、破壊状態をFigmentumへ入れない
- meshからfacilityを逆推定するAPIにしない

## Acceptance

- public headerと実装を責務別fileへ分離
- Figmentum specへcontract、決定性、versioning、consumer例を追記
- 既存build optionを変えず `figmentum_core` をbuildできる
- unit / integration testはこのsessionで実行しない
- 未実行testと、追加すべき将来test caseをPRへ明記

将来test case:

- same params / seed → canonical plan一致
- different seed → stableに異なるplan
- IDがLOD / mesh生成順から独立
- stationが中央anchor
- 各facility recipeをpolygonize可能

## Delivery

- Figmentum `origin/main` からtask専用branch / worktreeを作る
- 実checkout branchをCcへ登録してから編集
- KD-FG-001以外を実装しない
- commit / push / PR作成まで行う
- PR作成後は停止し、mergeは親sessionのreviewと明示許可へ委ねる
- mergeされた場合、そのcommitをKD-FP-001のdependency revisionへ固定する
