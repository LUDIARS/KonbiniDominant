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
- `VisiaDefinition` / `VisiaInstance` — resident / effectのgame-owned visual recipe
- `SpeechBubbleRequest` — world anchor、dummy text、distance visibility

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
  residents[]
  placementCues[]
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

`residents[]`は`PopulationCellTable`から派生する非権威sampleで、position、yaw、
trip phase、任意のtarget store、来店中の任意speechを値で持つ。
`placementCues[]`はそのtickで成功した配置だけを持ち、target position、first playableの
最終yaw 0度、completed tickからrendererのwall-clock animationを開始する。
どちらもcanonical stateではない。

resident primitive、speech bubble glyph、landing effectのCPU geometry契約は
[visia-presentation.md](visia-presentation.md)を正本とする。Pictor adapterは
VisiaをVisus／text atlas／effect resourceへ解決し、未解決resourceを別種primitiveへ
silent fallbackしない。

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

## Offscreen world composition

depth付きworld描画をPictor既定swapchain passへ直接記録しない。first playableは
次の二段passを固定する。

```text
pass 0: per-flight RGBA16F + D32 framebuffer
        WorldRenderLayer
barrier: active-flight color write → fragment shader read
pass 1: Pictor default swapchain framebuffer
        WorldCompositeLayer → HudOverlayLayer
```

pass 0のframebufferはswapchain image indexではなく記録中の
`VulkanContext::current_frame()`で選ぶ。`FrameComposer`の
framebuffer providerとpass間hookがそれぞれflight framebufferとbarrierを供給する。

`HudOverlayLayer`はPictor既定render pass以外を拒否する。offscreen world passへ
HUDを記録するとcomposite前のHDR targetへ焼き込まれ、画面には出ないまま「描いた
つもり」になる。HUD geometryはpixel空間で組み、NDC変換はpush constantの
viewport sizeを読むshaderが行う。vertex layoutはworld passと同じ`WorldVertex`
で、per-flight bufferは`WorldOverlayBuffers`を共有する。

HUD passのshaderは`konbini_hud.vert` / `konbini_hud.frag`で、world passの
`konbini_world.vert` / `konbini_world.frag`、composite passの
`konbini_composite.vert` / `konbini_composite.frag`と同じ生成targetが出す。

BASE-FP-HUD-ASCII-01: 5x7 bitmap fontはASCII (A-Z / 0-9 / 空白 / `-:/.+`) だけを
持つ。HUDのchain表記はcontentの日本語`displayName`ではなくslug由来のASCII
ラベルを使い、日本語表示はtext atlasを持つまで扱わない。

`WorldSceneTargets`はflightごとのcolor image/view、depth image/view、
framebufferを所有する。`FrameComposer`から渡されるswapchain image indexを
flight indexとして使わず、記録中の`VulkanContext::current_frame()`で選ぶ。
Pictor `AttachmentRegistry`の固定上限に合わせ、flight countは1以上4以下を
初期化時に検証する。

color attachmentは`R16G16B16A16_SFLOAT`、最終layoutは
`SHADER_READ_ONLY_OPTIMAL`、depth attachmentは`D32_SFLOAT`とする。Pictorの
registryが生成する依存はexternal→subpassのみなので、pass 0終了後に同一layoutの
image barrierで`COLOR_ATTACHMENT_WRITE / COLOR_ATTACHMENT_OUTPUT`から
`SHADER_READ / FRAGMENT_SHADER`へのmemory dependencyを補う。

composite layerはflightごとにscene color viewを指す
`COMBINED_IMAGE_SAMPLER` descriptorを持つ。samplerはnormalized coordinate、
nearest、clamp-to-edgeとし、vertex/index bufferを持たないfullscreen triangleを
既定render passへ1回drawする。pipelineはPictorの現在の既定render passと
一致しなければならない。HDR colorはlinearのままsamplingするため、swapchain
formatはsRGBであることを初期化時に検証し、UNORM fallbackを暗黙に許可しない。

resizeではdevice idle後、scene viewを参照するcomposite descriptor / pipelineを
先に破棄する。hostがframebuffer extentの不一致を検出した場合は、
Pictor swapchainを明示再生成してそのframeをskipする。scene targetは新しい
attachment / render pass / framebufferを
一時bundleへすべて生成し、成功後に入れ替えてから旧bundleを逆順破棄する。
生成失敗時は旧bundleの所有を保つ。最終shutdownもcomposer / layer、
scene target、`VulkanContext`の順を守る。zero extent、flight count変化、
null handle、resource再生成失敗をsilent fallbackしない。

`WorldSceneTargets`はimage/view bundleのgenerationを公開する。initialize /
resizeの成功ごとにgenerationは単調増加し、未初期化は0とする。scene viewを
cacheするborrower (composite descriptor等) は初期化時のgenerationを保持し、
記録前に一致を検証する。extentとflight countが変わらないresizeでは他のguardが
すべて通過するため、破棄済みviewのsamplingはこのgeneration比較だけがfail-fastで
捕捉できる。

## World pass recording

pass 0の記録は`WorldRenderLayer`が行う。`set_render_pass()`は
`WorldSceneTargets::renderPass()`と一致しないrender passを拒否し、Pictor既定
swapchain render passを渡された場合も失敗させる。既定passはdepth attachmentを
持たないため、記録できてしまうとdepth無しで都市が描かれる。

pipelineは2本で、layoutとshaderを共有する。

| pipeline | depth test | depth write | blend | cull |
|---|---|---|---|---|
| base | 有効 | 有効 | 無効 | none |
| overlay | 有効 | 無効 | src alpha | none |

Figmentum marching-cubesのwindingを仮定しないので双方cull noneとする。
overlayがdepth writeを行うとoverlay同士が互いを消すため、depthはbaseだけが
書く。Pictorの`build_graphics_pipeline()`はblend非対応かつdepth testと
depth writeが連動するので、この2本はgame側で組む。

記録順は`baseFacilities` → `overlayFacilities` → `overlayMesh`で固定する。
`overlayMesh`はZOC → store marker → selectionの順に結合し、snapshot内の順序を
そのまま使う。

push constantはvertex stageのみが読み、`viewProjection` (mat4) と`tint` (vec4)
の80 byte。`viewProjection`の正本は`IsometricCamera::viewProjection`。

BASE-FP-DESTROYED-01: base passはblendを持たないので、palette alphaが1未満に
なるfacility (現状は`Destroyed`のみ) をbaseへ入れると半透明指定が無視される。
alpha < 1のfacilityだけを`overlayFacilities`へ回し、depth testあり /
depth writeなしで合成する。破壊済みfacilityのdraw policyはこの分類が正本で、
alpha値自体は`facilityColor()`が持つ。

### Geometry ownership

`WorldGeometryCache`はFigmentum stable keyからGPU常駐geometryを引く。keyは
geometryの同一性だけを表し、facility stateやchain色は含まない。色はdraw単位の
tintで与えるので、state変化でupload をやり直さない。未登録keyは例外とし、
missing geometryをsilentに飛ばさない。

`WorldGeometryBuffer`はvertex / index bufferをHOST_VISIBLE | HOST_COHERENTで
所有し、capacity超過とvertex範囲外indexをupload前に弾く。解放はindex →
vertexの逆順。pinned Pictorの`VertexDataUploader`はstaging copyが未実装なので、
first playableはこのhost-driven uploadで代替する。

overlay geometryは毎フレーム作り直すため`WorldOverlayBuffers`がflightごとに
1本持つ。書き込んでよいのは`acquire_next_image()`が該当flightのfenceを待った
後の`current_frame()`だけ。

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
