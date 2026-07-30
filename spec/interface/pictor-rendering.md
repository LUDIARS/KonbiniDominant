# Pictor rendering contract

## 目的

Pictorへgame stateを漏らさず、読み取り専用snapshotから大量の都市施設・店舗・
effectを描画する。

調査対象: `LUDIARS/Pictor@c088e8d1b7b9e2625b7a8d923c89d4d684566c16`

## Ownership

| owner | data |
|---|---|
| simulation | StoreId、FacilityId、位置、owner、faith、phase、dimension |
| Figmentum adapter | CPU geometry / recipe cache |
| Pictor adapter | GPU buffer、MeshHandle、MaterialHandle、ObjectId |
| Pictor | SceneRegistry、culling、batch、render graph |

Pictor ObjectIdをsimulation/saveのIDとして使わない。

## Game-owned render domain

Pictor / Ergo / Vulkanの所有型はcamera、picking、overlay geometryへ漏らさない。
first playableは次のgame-owned valueを境界にする。

- `ViewportExtent` — pixel幅と高さ
- `WorldRay` — meter単位のoriginと正規化可能なdirection
- `WorldVertex` — position、deterministic normal、presentation color
- `FacilityPick` — stable Figmentum key、runtime FacilityId、ray distance

cameraとpickerは同じ`ViewportExtent`を使い、等距離pickはstable Figmentum keyで
決定する。ZOC geometryはsnapshot順と固定segment順から決定的に生成する。

camera行列はcolumn-major (index = `col * 4 + row`)、右手系view空間、
Vulkan clip空間 (depth 0..1、Y下向き) のorthographicとする。この規約はGPU側
shaderと共有する正本であり、game domainとPictor adapterの双方が従う。

BASE-FP-PALETTE-01: 原案はchain色とfacility state色を定めていない。first
playableの間はgame側のpresentation値として固定し、gameplay分岐には使わない。

## `RenderSnapshot`

tick終端にimmutable snapshotをpublishする。

```text
RenderSnapshot
  tick
  visibleDimension
  cameraHints
  facilities[]
  stores[]
  triangles[]
  populationVisuals[]
  effects[]
  hudViewModel
```

各record:

- stable gameplay ID
- mesh / material asset key
- transform
- bounds
- visibility / layer / shadow flags
- presentation parameter（chain色、faith、damage state等）

render threadはsnapshotをconsumeするだけでsimulation tableへ書き戻さない。

## Object lifecycle

adapterは次のmappingを持つ。

```text
FacilityId → Pictor ObjectId[]
StoreId    → Pictor ObjectId[]
EffectId   → Pictor ObjectId[]
```

snapshot diff:

- Added → asset解決後にregister
- Transform/Material dirty →対象streamだけupdate
- Removed → unregisterしmapping削除
- dimension非表示 → unregisterまたはvisibility off（budgetで選択）

facility破壊・store対消滅・dimension collapse後、次frame snapshotにstale objectを
残さない。

## Pictor pools

- distant static facility: `STATIC`
- interactive store / modified facility: `DYNAMIC`またはdirty解消後`STATIC`
- 大量同形store: instance / GPU-driven候補
- transient effect: 専用render layer / particle

現行classifierは件数だけで自動GPU-drivenへ移すとは限らない。必要flagsとcapabilityを
adapterが明示する。

## Geometry conversion

Figmentum meshから少なくとも次を生成する。

- position
- deterministic normal
- vertex colorまたはmaterial index
- 32bit index
- AABB

vertex layout / winding / coordinate handednessはPictor pipelineと一致させ、
境界testで固定する。

## Required bridge

現行upstreamでは`VertexDataUploader`のstaging allocation後の実copy処理に
未実装箇所があり、`register_mesh_data()`だけで描画完了とはならない。
`CompiledBatchRecorder`もhost-owned GPU resourceを解決する
`IBatchGpuSource`を要求する。

本作に必要な責務:

### `GpuAssetStore`

- VkBuffer / memoryの所有
- vertex / index upload
- asset key→Pictor MeshHandle / buffer range
- upload完了前objectのvisibility管理
- reference count / cache eviction
- device lost / shutdown時の逆順解放

### `KonbiniBatchGpuSource : IBatchGpuSource`

- MeshHandle→vertex/index buffer
- Material/Shader key→pipeline
- indirect / instance data
- missing resourceを明示error

### `PictorFrameBridge`

- attachment / RenderPass / Framebuffer registry
- render graph compile
- frame acquire / cull / batch / record / submit / present
- resize時の再構築
- snapshotのtickとrender frameの分離

これらが実装・統合されるまで「Pictor描画完了」としない。

## StageRendererの扱い

Ergo `StageRenderer`はFigmentum単棟のintegration probeに利用できるが、
本番大量描画経路ではない。

現状:

- position + normal中心
- per-object base color
- host-visible直接upload
- drawableごとdraw

さらにErgo origin/mainではrender pass設定とpipeline初期化順の整合確認が必要。
full gameは上記Pictor bridgeを完了してから進める。

## LOD / culling

- Figmentum recipeから複数LOD assetをcache可能
- Pictorの`lodLevel`保存だけに依存せず、mesh選択をadapterで実装
- visible dimensionだけrender snapshotへ出す
- vertical stackは高度chunkでpartition
- Triangle/ZOC overlayはgeometry object数を増やしすぎない専用passを検討

threshold、draw count、GPU memory、target FPSは `TBD-PERF-01`。

## Materials / presentation

- chain identityはmaterial / vertex color / sign等のdataで表現
- facility replacementはgeometry swapとsimulation transactionを分離
- reality collapseはpost-processやshader eventとしてphaseに応じて強化
- source parody名をtextureへ焼き込む前にIP review

## Failure

次をsilent fallbackしない。

- Vulkan / required extension不在
- shader / SPIR-V不在
- mesh upload失敗
- pipeline/material key未解決
- non-finite transform / bounds

debug placeholderは明示config `render.debugPlaceholder=true` の時だけ許可し、
production既定にしない。
