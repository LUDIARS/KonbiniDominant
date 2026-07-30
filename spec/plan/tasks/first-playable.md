---
task_id: KD-FP-001
title: First playable vertical slice
status: in_progress
blocked_by: []
figmentum_revision: 3ee998f487d984f54003c4ec3c4f7ba00b53eec3
---

# KD-FP-001 — First playable vertical slice

## Outcome

Figmentum が決定的に形成した N-KXi の一区画で、chainを選び、施設をクリックして
コンビニへ置換し、支出・定期収益・支配表示が変化する Windows native app の
実装PRを作る。実際の起動確認は
[KD-FP-002](first-playable-validation.md) で行う。

これは Phase 1 の最初の操作可能版である。Phase 1全rule、Phase 2、Phase 3、
Boss、production規模の最適化はこのtaskに含めない。

## Required reading

実装前に次を読む。

1. [../../design.md](../../design.md)
2. [../../data/world-state.md](../../data/world-state.md)
3. [../../data/content-schema.md](../../data/content-schema.md)
4. [../../feature/chain-selection.md](../../feature/chain-selection.md)
5. [../../feature/phase-1-dominant-triangle.md](../../feature/phase-1-dominant-triangle.md)
6. [../../interface/figmentum-city-generation.md](../../interface/figmentum-city-generation.md)
7. [../../interface/pictor-rendering.md](../../interface/pictor-rendering.md)
8. [../../interface/ergo-runtime.md](../../interface/ergo-runtime.md)
9. [../implementation-roadmap.md](../implementation-roadmap.md)

## Prerequisite

[KD-FG-001](figmentum-city-plan.md) はFigmentumへmerge済みで、
KonbiniDominantは revision
`3ee998f487d984f54003c4ec3c4f7ba00b53eec3` を固定する。

## In scope

### Build and process boundary

- C++20 / CMake の repository skeleton
- headless `konbini_sim` と native `konbini_dominant` のtarget分離
- Pictor、Ergo、Figmentum のrevision / source pathをconfigure時に明示
- native targetへFigmentumの`figmentum_core`を直接linkし、CityPlanと施設meshを
  app process内で生成
- 必須dependency、shader、content欠落時のfail-fast
- Windows executableの起動経路と明示的な正常終了経路を実装

### City

- 固定 `WorldSeed` から同じ一区画を再生成
- 中央 station anchor と、クリック可能な複数の `FacilityId`
- Figmentum の建物recipe / SDF / polygonizeを施設単位で利用
- mesh生成はstartup時に行い、frame loopでは行わない
- game側の `CityManifest` がsemantic stateの正本

semantic placement、station anchor、stable facility identityは、merge済みの
Figmentum `CityPlan` を正本にする。game側はplanを `CityManifest` へ変換するが、
placement algorithmやID生成を複製しない。
Figmentum CLIのoffline出力や、手書き済みmeshをゲームへ読み込むだけの構成は
「組み込み」とみなさない。

### Simulation

- 世代付き `EntityId`
- `FacilityTable`、`StoreTable`、`PopulationCellTable` のdense data
- fixed tick、command validation、`StructuralCommandBuffer`
- chain選択
- 初期資金は baseline の5店舗分
- 空き施設の選択、費用検証、施設破壊、店舗配置
- 人口集約と定期収益
- 最小のZOC表示用 influence値
- 同じ seed と command stream から同じ canonical snapshot

店舗ごとの継承object、個別住民object、全責務を持つ `GameManager` は禁止する。

### Task-scoped content baseline

BASE-FP-CONTENT-01として、次を
`data/content/first-playable.json` へ隔離する。製品balanceのowner確定値ではない。

| key | ローサン | ファモマ | セバンイレバン |
|---|---:|---:|---:|
| `buildCostCredits` | 1000 | 1250 | 800 |
| `zocRadiusMeters` | 18 | 24 | 18 |
| `revenueMilliCreditsPerPerson` | 500 | 450 | 400 |

- `ticksPerSecond = 10`
- `economyPeriodTicks = 10`
- 選択chainの初期cash = `buildCostCredits * 5`
- 各population cellの人口 =
  `50 + Random(FP_POPULATION, tick=0, facilityId, ordinal=0) % 101`
- 同一次元でZOC内にあるpopulation cellを最短storeへ割当し、距離tieは
  `StoreId`昇順で解決
- economy periodごとの収益 =
  `floor(capturedPopulation * revenueMilliCreditsPerPerson / 1000)`
- 配置失敗時はcash / facility / populationを一切変更せず、refundは発生しない

このbaselineを変更する場合は値だけでなく、canonical snapshot期待値と
画面表示の説明を同じchangeで更新する。

### Input and rendering

- GLFW eventを Ergo input injectionへ渡すadapter
- simulationのread-only `RenderSnapshot`
- Figmentum meshをPictorで描画するために必要なgame-owned GPU bridge
- mouse pickingを stable `FacilityId` へ解決
- クリック候補、選択施設、店舗、ZOCの視覚差
- chain選択、cash、store count、次tick収益、操作ヒントを確認できるHUD
- window resizeとshutdownでresourceを解放
- repo直下の `excubitor.catalog.yaml` にservice
  `konbini-dominant-app` を登録

catalogは少なくとも次を固定する。

```yaml
runtime: app
app_kind: native
exec: ${ARS_ROOT}/KonbiniDominant/build/Release/konbini_dominant.exe
cwd: ${ARS_ROOT}/KonbiniDominant
build_command: 'cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DKONBINI_BUILD_TESTS=OFF && cmake --build build --config Release --target konbini_dominant'
process_match: konbini_dominant.exe
autostart: false
restart_policy: no
health:
  type: process
```

現行 Pictor の未実装 upload を「利用済み」と偽装しない。
`GpuAssetStore` / `IBatchGpuSource` / `PictorFrameBridge` の全部が必要か、
より小さい実経路で成立するかを現行APIから判断し、選んだ経路を
`spec/interface/` と実装コメントへ反映する。

## Minimal controls

| input | action |
|---|---|
| `1` / `2` / `3` | ローサン / ファモマ / セバンイレバンを選択 |
| left click | facility選択、同じ有効候補を再clickして配置確定 |
| right click / `Esc` | 選択解除 |
| `WASD` / drag | camera移動 |
| wheel | zoom |
| `F1` | 操作表示のtoggle |
| window close | 正常終了 |

## Out of scope

- 3店舗Triangleの完成ruleと敵店舗破壊
- opponent AI
- Phase遷移
- vertical stack / faith / 256階
- 他次元 / anti-store / energy
- Aion
- save migration、packaging、production performance tuning
- Figmentum / Pictor / Ergo repositoryそのものの変更
- 単体・統合testの実行

必要なupstream変更を発見した場合はconsumer側へcopyせず、gapと最小再現条件を
記録してこのtaskを停止する。

## PR acceptance

- repository構成が [../implementation-roadmap.md](../implementation-roadmap.md)
  の責務分割に従う
- `konbini_sim` は Pictor / Vulkan をlinkしない
- Release native appをclean configureからbuildできる
- warning / dependency failureを成功扱いしない
- build commandとdependency revisionをREADMEへ記録
- `excubitor.catalog.yaml` のexec / process matchは
  `build/Release/konbini_dominant.exe` と一致
- catalogのcwd / Release `build_command` / process healthが上記契約と一致
- unit / integration testを実行せず、実行していないことをPRへ明記
- runtime acceptanceはこのPRの完了条件に混ぜず、KD-FP-002へhandoff

## Delivery

- task専用branchを使用し、別taskを混ぜない
- commitしてpushし、1本のPRを作成する
- PR本文へbuild結果、未実行test、既知gap、KD-FP-002未実施を明記する
- merge前に親sessionがdiffとbuild結果を確認する
