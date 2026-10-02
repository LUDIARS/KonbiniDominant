---
task: kd-gate5-pictor-ergo-bridge
project: KonbiniDominant
kind: 実装
created: 2026-10-02
memory_links:
  - spec/plan/implementation-roadmap.md
  - spec/interface/pictor-rendering.md
  - spec/interface/ergo-runtime.md
  - spec/test/verification-strategy.md
  - spec/plan/tasks/first-playable.md
  - spec/tasks/2026-08-01-kd-npc-002-pictor-runtime-integration.md
---

# KD Gate 5 — Production Pictor / Ergo bridge

Actio: `actio:dfb6a3e7-fbf4-4ad5-861b-7b3696375788`

## 目的

[implementation-roadmap](../plan/implementation-roadmap.md) Gate 5 の
`GpuAssetStore` / `KonbiniBatchGpuSource : IBatchGpuSource` / `PictorFrameBridge` を
本作側に実装し、facility 描画を pinned Pictor (`02ea861c`、KD-MOB-002 で更新) の
`SceneRegistry` → `CullingSystem` → `BatchBuilder` → `CompiledBatchRecorder`
経路へ載せる。KD-NPC-002 はこの 3 つの review 済みを前提にしている。

## 正本

- [pictor-rendering.md](../interface/pictor-rendering.md) Required bridge / Object lifecycle
- [ergo-runtime.md](../interface/ergo-runtime.md) Render host
- [verification-strategy.md](../test/verification-strategy.md) §5 Production gate
- [first-playable.md](../plan/tasks/first-playable.md) Gate 5 の経路選択

## 完了条件

- [x] `GpuAssetStore` が asset key → Pictor `MeshHandle` / vertex・index buffer を所有し、
      参照数、未参照 asset の遅延破棄 (flight 数ぶんの frame 待ち)、shutdown 時の
      逆順解放を持つ
- [x] `KonbiniBatchGpuSource` が `MeshHandle` → buffer、shader key + pass type →
      pipeline を解決し、未解決を記録する。`PictorFrameBridge` は未解決を
      frame の明示 error にし、placeholder で隠さない
- [x] `PictorFrameBridge` が snapshot 由来の facility draw を Pictor object へ
      差分同期 (FacilityId → ObjectId) し、cull / batch / instance data / record を行う。
      snapshot tick と render frame 番号を分けて持つ
- [x] world pass の記録順 base facility → store → overlay facility → overlay mesh を保つ
- [x] resize (composer 再構築) で scene 同期状態を失わず、pipeline / instance buffer
      だけを作り直す。shutdown は layer → bridge → asset store → scene target の順
- [x] lifetime / resource resolution / 同 frame 差分 / bulk unregister / instance path
      の test を同じ変更単位で追加する
- [x] spec (pictor-rendering / ergo-runtime / verification-strategy / first-playable) へ
      選んだ経路と upstream gap を反映する
- [x] Windows x64 Debug build + `KONBINI_BUILD_TESTS=ON` で ctest 全件
- [ ] Revisor local PR 提出

## 検証記録 (2026-10-02, run ab7306a8)

- Pictor `02ea861c` の `CompiledPass::filter_mask` / `RenderBatch::transparency`
  への追従をコミット (`0ba0e1a`)。plan は shader key と透過区分の不一致を拒否し、
  各 world pass は対応する `filter_mask` を設定、filter で落ちた batch は frame error。
- `build-gate5-debug` (Visual Studio 17 2022, Debug, `KONBINI_BUILD_TESTS=ON`) の
  incremental build 成功、ctest 20/20 合格。clean 再 configure はしていない。
- 未実施: main `de87fed` (KD-MOB-003 docs) の取り込み。feature branch への
  merge 操作が実行環境の権限判定で拒否されたため、人間の判断待ち。
  de87fed は KD-MOB-003 の実装 (platform / city cache 等 45 files) を含むため、
  取り込み後の build / ctest は再実施が必要。
- 未実施: アプリ起動・実機 GPU 描画確認 (Cc policy により起動テストなし)。

## 範囲外 (残課題として報告する)

- STATIC pool 経路: pinned Pictor の `BatchBuilder` は STATIC batch の sorted index を
  公開せず、`CompiledBatchRecorder` の `firstInstance = startIndex` から instance data を
  引けない。facility は DYNAMIC pool に置く (BASE-GATE5-POOL-01)
- LOD 選択、GPU-driven pool、store / effect の Pictor object 化 (store は現行 CPU mesh 経路)
- draw / memory budget (`TBD-PERF-01` 未確定)
- 実機 (GPU) での描画確認・起動テスト
- Pictor `PipelineCompiler` / `CompiledGraph` による pass graph compile
  (pass graph は Ergo `FrameComposer` が正本、ergo-runtime.md Render host)

## スコープ (編集可ディレクトリ)

- `src/adapters/pictor/`、`include/konbini/adapters/pictor/`
- `src/adapters/ergo/world_frame_graph.cpp` と header
- `src/render/world_render_layer.cpp`、`world_pipelines.*`、`world_draw_list.*`
- `src/app/native_game_session.cpp`
- `shaders/`、`tests/`、`spec/`、`cc.acceptance.json`

上流 (Pictor / Ergo / Figmentum) は編集しない。
