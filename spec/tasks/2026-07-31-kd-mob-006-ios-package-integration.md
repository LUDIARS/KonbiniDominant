---
task: kd-mob-006-ios-package-integration
project: KonbiniDominant
kind: 実装
status: pending
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 676
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-mob-001-ergo-platform-render-contract.md
  - spec/tasks/2026-07-31-kd-mob-002-pictor-surface-recovery.md
  - spec/tasks/2026-07-31-kd-mob-003-mobile-runtime-assets.md
  - spec/tasks/2026-07-31-kd-mob-004-touch-interface.md
  - spec/tasks/2026-07-31-kd-mob-005-android-package-integration.md
  - spec/interface/mobile-platform.md
  - spec/setup/mobile-development.md
  - spec/setup/mobile-native.md
  - spec/interface/pictor-rendering.md
  - spec/interface/ergo-runtime.md
  - spec/faq/open-questions.md
---

# KD-MOB-006 — iOS package integration candidate (Metal)

## 方針変更 (2026-10-03)

neco決定 (2026-10-03):「iOS は Metal で描画するようにします」。このtaskは
旧方針「Pictor Vulkan + MoltenVK」からPictor Metal backend (`MetalContext`) による
Metal直描画へ書き直した。Androidは変えない。決定と理由の正本は
[mobile-platform](../interface/mobile-platform.md#ios描画方針-2026-10-03-決定)。
方針変更の記録task: `actio:c86334f3-2f4c-4b5c-82c8-696a565a8d11`。

## 目的

Androidで確立した共通mobile contractをiOS native hostへ接続し、
Pictor Metal backend経由で実機検証へ渡せるiOS package candidateを生成する。

## 前提 (上流タスク)

KDの描画は2026-10-03時点でPictorのVulkan pipelineとVulkan handleに直接乗っており、
Pictor `MetalContext`はmesh upload / drawと復旧APIしか持たない
(Pictor `PC-FEAT-PLAT-05`)。次の上流taskが完了・pin更新されるまで、このtaskは
着手しない。上流のfileはKonbiniDominantから編集せず、各repositoryのtaskとして起こす。

### (a) Pictor — Metal backendをKDが使う描画機能まで拡張する

Pictor repositoryのtaskとして起こす内容。KD現行コードが使う機能 (2026-10-03に
`src/adapters/pictor/`、`src/render/`、`src/adapters/ergo/`のinclude / 呼び出しから列挙):

| KDの機能 | KD側の実装 | 現在のPictor / Vulkan依存 | Metalで要るもの |
|---|---|---|---|
| Gate 5 facility batch | `GpuAssetStore`、`KonbiniBatchGpuSource`、`PictorFrameBridge`、`pictor_batch_plan`、`PictorSceneSync` | `SceneRegistry` → `CullingSystem` → `BatchBuilder` → `CompiledBatchRecorder`、`IBatchGpuSource`がVkBuffer / VkPipelineを返す | 同じ4段経路のMetal recorder、`IBatchGpuSource`相当のbackend中立なbuffer / pipeline解決 (MTLBuffer / MTLRenderPipelineState) |
| mesh upload / GPU asset所有 | `VulkanGpuMeshUploader`、`HostVisibleMemory`、`WorldGeometryBuffer`、`MemorySubsystem` | VkBuffer、host-visible memory、flight数ぶんの遅延破棄 | `MetalContext::upload_mesh`の参照数・遅延破棄対応、host-visible相当 (shared storage) buffer |
| presentation objects (KD-NPC-002) | `PresentationObjectSync`、`PresentationGeometryLoader`、`NpcPresentationDraws` (resident / speech bubble / landing effect) | `SceneRegistry`のDYNAMIC pool、半透明draw、per-object transform | DYNAMIC pool更新と半透明 (alpha blend、depth write off) draw |
| instanced world / overlay | `WorldInstancedPipelines`、`WorldInstanceBuffers`、`WorldOverlayBuffers`、`WorldPipelineFactory` | `GraphicsPipelineBuilder`、per-flight vertex / instance buffer、push constant | instanced draw、per-flight buffer、push constant相当 (`setVertexBytes`等) |
| offscreen world composition | `WorldSceneTargets`、`WorldRenderLayer`、`WorldCompositeLayer` | `AttachmentRegistry` / `FramebufferRegistry` / `RenderPassRegistry`、per-flight RGBA16F + D32、pass間barrier | offscreen RGBA16F color + Depth32Float target、2-pass構成 (world → composite)、pass間の依存 |
| HUD vector text / overlay | `HudOverlayLayer`、`HudPipelines`、`hud_text_geometry`、`vector_font`、`bitmap_font`、`speech_glyph_atlas` (三角形化済みglyph) | swapchain default pass上のpipeline、pixel空間→NDCのpush constant | default (drawable) pass上の2D overlay pipeline。glyphはgeometryなのでtexture atlas不要 |
| construction particles | `construction_particle_geometry` (Ergo particle → `WorldMesh`) | CPU生成geometryをworld passへ流す | 追加のGPU機能なし (world passが動けばよい) |
| shader | `konbini_world` / `konbini_world_instanced` / `konbini_composite` / `konbini_hud` (GLSL → SPIR-V、host `glslc`) | SPIR-V module (`SpirvModule`) | 同等のMetal shader library。生成方法は`TBD-IOS-METAL-SHADER-01` |
| frame / recovery | `RenderDeviceHost`、`FrameOutcome`、`RenderLifecycle` | `VulkanContext`、`FrameResult`、`FrameGate`、`set_presentation_suspended` | `MetalContext`の`acquire_frame` / `present_frame` / `recover_surface` / `recover_device`を同じ`FrameResult`分類で返す (既存) と、suspend中のsubmit抑止 |
| gallery frame export | `src/gallery/frame_export.cpp` | Vulkan readback | iOSでは不要 (desktop tool) |

Rive: KDのsource / CMake / specに参照は無い (2026-10-03 grep 0件)。iOS Metalの
前提に含めない。

### (b) Ergo — 描画契約にiOS Metalを加える

Ergo repositoryのtaskとして起こす内容。

- `ergo_render`の`RenderBackend`と`cmake/ErgoRenderBackend.cmake`は、iOSを
  「pictorのみ / MoltenVK、`ERGO_RENDER_PLATFORM_IOS=1`」としてVulkan前提で扱う
- `ERGO_RENDER_PLATFORM_IOS=1`がVulkan (`PICTOR_HAS_VULKAN`) を要求しない形にし、
  Pictor Metal backendをreal render pathとして判定できるようにする
- `FrameComposer` / `IRenderLayer` / `RenderContext`がVulkan型
  (command buffer、render pass、framebuffer provider、pass間hook) を公開しているなら、
  Metal backendでも同じlayer構成 (world → composite → HUD) を記録できる
  backend中立の形を用意する
- 実描画不可は従来どおり`RenderBackendError`の型付き値で返す

### (c) Pictor — iOS Metal validation

Pictor `spec/tasks/2026-09-10-ios-metal-validation.md`。`MetalContext`のiOS
build・実機確認が未了。(a) の拡張分も含めてiOS device / simulatorで検証済みの
Pictor revisionをKDがpinする。

### KD側の前提

- KD-MOB-001〜005がreview済み
- (a)〜(c) を満たすPictor / Ergo revisionへKDのpinを更新できる
- macOS / Xcode / iOS SDKを明示的に解決できる (MoltenVKは不要)
- signing identity / team IDをrepository外から注入できる
- `TBD-IOS-METAL-*` ([open questions](../faq/open-questions.md)) のうち
  `TBD-IOS-METAL-FEATURE-01`が決まっている

## 外す旧方針 (MoltenVK) 資産

2026-10-03の方針変更ではコードを変更していない。このtaskの実装で次を外す。

- `cmake/MobileVulkan.cmake`のiOS分岐 (`KONBINI_MOLTENVK_LIBRARY` /
  `KONBINI_VULKAN_HEADERS`の検査、MoltenVKを`Vulkan::Vulkan`としてimport、
  `KONBINI_PLATFORM_IOS=1`の定義位置はMetal経路側へ移す)
- iOSが`cmake/MobileVulkan.cmake`と`cmake/mobile/FindVulkan.cmake`を読む経路
  (`CMakeLists.txt`の`if(KONBINI_MOBILE)`で`MobileVulkan.cmake`をincludeする分岐を
  Android専用にする。host `glslc`の要否は`TBD-IOS-METAL-SHADER-01`に従う)
- `mobile/ios/vulkan_portability.cpp` (MoltenVK portability shim)
- `mobile/CMakeLists.txt`のiOS分岐の
  `target_compile_definitions(pictor PRIVATE vkCreateInstance=konbiniCreateInstance vkCreateDevice=konbiniCreateDevice)`
  と`Vulkan::Vulkan` link
- `mobile/ios/main.mm`の`pictor::IOSSurfaceProvider` (Vulkan surface) 利用。
  `CAMetalLayer`を`MetalContext`へ渡す形にする
- `cmake/RequireErgoRealRender.cmake`のiOS判定がVulkan前提なら、(b) の契約に合わせる
- `tests/cmake/ergo_real_render_contract_test.cmake`のiOS期待値 (同上)

## 完了条件

- Objective-C++ thin hostとC++ game ownerを分離する
- `UIView`の`CAMetalLayer`をPictor `MetalContext`へ渡し、Metalで直接描画する
- iOS buildがMoltenVK、Vulkan loader、Vulkan headerを要求しない
- 上記「外す旧方針資産」を撤去し、Android / desktopのVulkan経路を変えない
- Metal device / feature capabilityをfail-fastで検査する
- lifecycle、surface、touch、safe area、memory / thermal eventを共通境界へforwardする
- bundle assetとwritable Application Support / Cachesを分離する
- Figmentum都市、DoD simulation、Pictor world描画、Ergo frame / inputを接続する
- Android / Windowsと同じcontent schemaとcanonical saveを使う
- Apple SDK / signing secretをsourceへhard-codeしない
- Pictorを迂回してgame domainからMetalを直接描画しない
- Metal backend未実装の描画機能を黙って省略しない (`TBD-IOS-METAL-FEATURE-01`の決定に従う)
- build / archive失敗を成功扱いしない
- [mobile-native](../setup/mobile-native.md#xcode--ios)のiOS手順を実際のconfigure /
  Xcode手順で更新する

## Scope boundary

このtaskはiOS app build / archive経路まで。actual-device install、startup、
background / resume、performanceはKD-MOB-007へ分離する。`playable`の成立は
KD-MOB-007のiOS functional acceptance完了時とし、このtask単独では完成扱いしない。
上流 (a)〜(c) の実装はこのtaskに含めない。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / PRを作成して停止する
- build host、toolchain、signing有無、Pictor / Ergo pin、未実施runtime確認をPR本文へ明記する
- unit / integration / behavior / startup testは明示指示なしに実行しない
