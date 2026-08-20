---
task: kd-fp-000i-depth-world-rendering
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000h-pictor-gpu-composition.md
  - spec/tasks/2026-07-31-kd-fp-000j-native-first-playable.md
  - spec/interface/pictor-rendering.md
  - spec/plan/tasks/first-playable.md
---

# KD-FP-000I — Depth-correct world rendering

## 目的

Figmentum facility meshとsimulation snapshotをgame-owned GPU resourceへ射影し、
`WorldSceneTargets`のdepth付きpassへ都市、店舗、ZOC、selectionを描画する。

## 完了条件

- Figmentum meshをgame-owned `render::WorldVertex`へ変換し、重複vertex型を作らない
- vertex / index bufferの所有、upload、範囲検証、逆順解放を実装する
- facility keyからGPU geometryへのcacheを持ち、missing resourceをsilentに飛ばさない
- world base pipelineはdepth test / writeを有効化する
- overlay pipelineはblendとdepth testを有効化し、depth writeを無効化する
- base facilityを先に、ZOC / store / selection overlayを後に記録する
- `WorldRenderLayer`がcustom offscreen render passだけを対象にする
- pipeline / shader / render pass不一致をfail-fastする
- `konbini_review`がworld shaderとrender targetをbuildする
- unit / integration / behavior / startup testはこのtaskでは実行しない

## スコープ候補

- `include/konbini/adapters/pictor/`
- `include/konbini/render/`
- `src/adapters/pictor/`
- `src/render/`
- `shaders/`
- `spec/interface/`
- `spec/tasks/`

native window、Ergo input、FrameComposer loop、HUD、Excubitor起動確認は
KD-FP-000Jへ分離する。

## 残作業メモ

- pinned Pictorのhost-driven upload責務をgame側buffer ownerで補う
- Figmentum marching-cubes windingを仮定せず、base pipelineはcull noneとする
- destroyed facilityのalpha / draw policyを明示する

## 実装結果 (2026-08-03)

- `render::WorldMesh`を追加し、`ZocOverlayGeometry` / `VisiaGeometry`をその
  aliasへ寄せてmesh型の重複を解消
- `buildFacilityWorldMesh()`でFigmentum geometryを`WorldVertex`へ射影。色は
  vertexへ焼かず中立色にし、facility stateはdraw単位のtintで表現
- `WorldGeometryBuffer` / `WorldGeometryCache` / `WorldOverlayBuffers`で
  vertex / index bufferの所有、host-driven upload、capacityとindex範囲の検証、
  逆順解放、facility key cacheを実装。未登録keyは例外
- store marker / selection overlay geometry builderを追加し、
  `buildWorldDrawList()`がbase / overlay / overlay meshを固定順で組む
- `WorldPipelines`がbase (depth write) とoverlay (blend / depth writeなし) の
  2本を生成。両方cull none、push constantはviewProjection + tint
- `WorldRenderLayer`を追加。`WorldSceneTargets`のoffscreen render pass以外を
  拒否し、Pictor既定render passも拒否する
- `konbini_shaders`へworld shaderを追加し、`konbini_review`が
  `konbini_pictor_world_geometry` / `konbini_world_render` / shaderを含むよう更新
- SPIR-V読み込みを`adapters/pictor/spirv_module`へ集約し、composite layerの
  重複実装を解消
- build / unit / integration / behavior / startup testは未実行
