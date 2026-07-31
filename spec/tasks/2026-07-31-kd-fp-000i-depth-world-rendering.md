---
task: kd-fp-000i-depth-world-rendering
project: KonbiniDominant
kind: 実装
status: pending
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
