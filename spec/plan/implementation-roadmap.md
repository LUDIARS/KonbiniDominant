# Implementation roadmap

## Goal

Notion原案に含まれるPhase 1〜Bossまでを、Pictor / Ergo / Figmentumと
DoD simulationで実装・配線し、Windows / Android / iOSで共有する
決定的save/replayと検証を完成させる。
途中gateは品質確認点であり、部分版を完成扱いしない。

初回の実装委託では、複数gateを薄く縦断する
[tasks/first-playable.md](tasks/first-playable.md) を先に完成させる。この実行単位は
操作可能な技術・ゲームプレイ基線であり、Phase 1全体や製品完成を意味しない。

## Proposed source layout

```text
include/konbini/
  app/
  sim/
    commands/
    events/
    storage/
    systems/
    spatial/
  city/
  render/
  ui/
  adapters/
    ergo/
    figmentum/
    pictor/

src/
  app/
  sim/...
  city/
  render/
  ui/
  adapters/...

data/
  content/
  shaders/
  ui/

tools/
  ergo-plugins/

tests/
  sim/
  city/
  adapters/
  integration/
  replay/
```

1 file / module / class = 1 responsibility。`GameManager`へ集約しない。

## Gate 0 — Decisions and budgets

実装前に [faq/open-questions.md](../faq/open-questions.md) のP0を確定する。

- boss勝敗
- phase累積
- single-player + 2 AI
- Figmentum semantic city contract
- store / lot / dimension / population規模
- Triangle capture rule
- dependency revisions

性能budgetを決めずにGPU-drivenやresident個体化を先行しない。

## Gate 1 — Repository / build harness

- C++20 / CMake skeleton
- Pictor → Ergo順のdependency wiring
- Figmentum core wiring
- content schema validator
- headless simulation targetとnative app targetを分離
- CI build / test jobs
- required shader / Vulkan capabilityのfail-fast

Acceptance:

- headless targetがPictor/Vulkanをlinkしない
- native targetだけがadapterをlink
- required content欠落で明示失敗
- dependency revisionが再現可能

## Gate 2 — Figmentum semantic plan

別repository作業:

- Figmentumへ`CityPlan` / stable facility ID / station anchor contractを追加
- seedから同じplanを再生成
- interactive facilityを個別recipeへ分解
- N-KXi profileをversion化

KonbiniDominant:

- `ICityGenerator`
- manifest validation
- recipe / CPU mesh cache
- save manifest hash

Acceptance:

- 同じrecipe+seedでcanonical serialization / hashが一致
- stationが中央anchor
- facility IDがLOD / mesh順と独立
- facility単位で生成・選択・破壊可能

## Gate 3 — DoD simulation foundation

- generational IDs / dense tables / sparse lookup
- fixed-step clock
- command / structural command / domain event buffers
- deterministic random streams
- dimension-aware spatial index
- canonical snapshot / replay hash
- save/load foundation

Acceptance:

- object allocationなしのstore hot scan
- iteration orderを変えてもcanonical stateが一致
- invalid commandがstateを部分変更しない
- removed IDのstale handleを検出

## Gate 4 — Phase 1 full rules

- chain select / content passives
- facility replacement / store placement
- economy / population aggregation
- ZOC influence
- Delaunay candidate / Triangle lifecycle
- capture / destruction
- 2 opponent AI
- phase transition
- HUD / placement preview

Acceptanceは [phase-1-dominant-triangle.md](../feature/phase-1-dominant-triangle.md)
の全不変条件とfuture tests。

## Gate 5 — Production Pictor / Ergo bridge

- GLFW→Ergo input adapter
- `GpuAssetStore`
- `IBatchGpuSource`
- `PictorFrameBridge`
- facility/store object sync
- vertex normal / material conversion
- LOD / static / dynamic / instance path
- UI / overlay / screenshot
- resize / shutdown

単棟StageRenderer確認はこのgateの診断であり、完成条件ではない。

Acceptance:

- Figmentum施設をGPUへuploadし表示
- click picking→FacilityId
- placementでfacility object解除→store object登録
- repeated create/destroyでresource leakなし
- missing shader / bufferをplaceholderで隠さない
- target規模でdraw/memory budget内

## Gate 5M — Smartphone first playable

現行Windows first playableを基線に、Androidを先行して共通mobile contractを
実装し、その境界をiOSへ接続する。mobile対応を既存KD-FP-001へ混ぜず、
[mobile-platform contract](../interface/mobile-platform.md)と
[KD-MOB-000](../tasks/2026-07-31-kd-mob-000-smartphone-contract.md)から始まる
task-workflowを作業単位とする。

- Ergo render contextをGLFW具象型からPictor `ISurfaceProvider`へ一般化
- Pictor / Ergo / gameのVulkan build contractをdesktop SDKとmobile runtimeへ分離
- app lifecycle、surface loss、asset reader、writable pathをplatform adapter化
- touch / safe area / density-aware HUD
- Android native host、package、Figmentum geometry cache
- iOS native host、MoltenVK portability、package
- actual deviceでresume、memory / thermal pressure、性能を検証

Acceptance:

- Windows first playableのruleとcanonical stateを変更しない
- 同じseedと正規化command列でplatform間のcanonical snapshotが一致
- Android / iOSともPictor経由でFigmentum都市を描画する
- background中はfixed tickとGPU submissionを進めない
- surface再生成でsimulation stateを失わない
- capability profile変更は観測可能で、silent fallbackしない
- 実機証跡をTestWorkflowへ記録する

## Gate 6 — Phase 2

- vertical slot placement / pricing
- multi-chain stack
- faith system
- image strategy
- 256階world / camera / partition / UI
- timer / threshold transition

Acceptance:

- 256 slotをsimulation正本で判定
- 高度chunk / LODで描画budgetを守る
- 支持関係ruleを決定的に処理

## Gate 7 — Phase 3

- lazy dimension generation
- dimension UI / off-screen simulation
- cross-dimension anchor map
- faith max inversion
- anti-store matching / annihilation
- energy modifier / collapse
- 2 dimension objective

Acceptance:

- save/load後も同じdimension / anchor
- float近似だけに依存しないmatching
- collapse後にstale entity / GPU objectなし

## Gate 8 — Boss / Result

- Aion roster / spawn command
- MaxValue
- time cut/paste（owner決定後）
- 30秒manifestation
- infinite lazy escape
- win / lose / Result statistics
- accessibility / content / IP review

## Gate 9 — Full validation

- all unit / property / integration / replay / save migration tests
- native render path
- long deterministic soak
- performance budgets
- packaged build
- spec↔implementation linkage review

ユーザが明示しないセッションで実行・起動testを勝手に開始しない。

## Cross-repository changes

Figmentum、Pictor、Ergoのgapを修正する場合は各repositoryのbranch+PRで行う。
KonbiniDominant側にupstream実装をcopyして一時的に隠さない。

## Rollback

- content schema / recipeはversionを戻せる
- new rendering pathは旧production bridgeが存在する場合だけfeature flag化
- save migrationはoriginalを上書きしない
- upstream API変更はconsumer adaptationと同時にversion pinを更新
