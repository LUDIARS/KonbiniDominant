---
task: kd-fp-000h-pictor-gpu-composition
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000g-render-domain-foundation.md
  - spec/tasks/2026-07-31-kd-fp-000i-depth-world-rendering.md
  - spec/tasks/2026-07-31-kd-fp-000j-native-first-playable.md
  - spec/interface/pictor-rendering.md
  - spec/interface/ergo-runtime.md
  - spec/setup/native-development.md
  - spec/plan/tasks/first-playable.md
---

# KD-FP-000H — Pictor GPU composition foundation

## 目的

Pictor / Ergoを固定revisionで導入し、depth付きworld描画をswapchainと分離する
per-flight offscreen targetと、world colorを既定render passへ合成するlayerを
first playableのGPU境界として実装する。

## 完了条件

- Pictor / Ergo sourceがconfigure時にexact revisionかつcleanと検証される
- render buildを明示optionで無効化でき、headless domain targetはVulkan非依存を保つ
- flightごとにRGBA16F color、D32 depth、framebufferを所有する
- flight count、extent、Vulkan handleをfail-fastで検証する
- scene pass終了後のcolor writeからcomposite samplingへのmemory dependencyを明示する
- swapchain image indexとflight indexを混同しない
- composite layerがflight別descriptorでscene colorを既定render passへ1回drawする
- resize / shutdown時のresource破棄順を明示する
- shader sourceを`glslc`で再現可能にSPIR-Vへ変換する
- unit / integration / behavior / startup testはこのtaskでは実行しない

## スコープ (編集可ディレクトリ)

- `include/konbini/adapters/pictor/`
- `include/konbini/render/`
- `src/adapters/pictor/`
- `src/render/`
- `shaders/`
- `CMakeLists.txt`
- `README.md`
- `src/CMakeLists.txt`
- `spec/interface/`
- `spec/setup/`
- `spec/tasks/`

facility GPU upload、world graphics pipeline、`WorldRenderLayer`、Ergo
`FrameComposer`へのpass登録、native app、Excubitor起動確認は後続taskに分離する。

## 実装結果 (2026-07-31)

- Pictor / Ergoを固定revision・clean source検証付きで導入
- per-flight HDR color / depth / framebuffer所有境界を追加
- render-pass終了後のsame-layout color barrierを追加
- flight別sampled descriptorを持つfullscreen composite layerを追加
- shader buildとstaged review targetを追加
- unit / integration / behavior / startup testは未実行
