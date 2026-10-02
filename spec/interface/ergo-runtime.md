# Ergo runtime contract

## 目的

Ergoから入力、frame clock、render host、UI/audio等の共通機能を利用し、
game-specific ruleやmass entity storageをErgoへ持ち込まない。

調査対象: `LUDIARS/Ergo@7f0d6bbd34dced4fc6664a5f04bce9910e893537`
(KD-MOB-001でplatform-neutral render contractを含むrevisionへ更新。
以前の調査対象は`771b027f0e5492015b27f54c3bab1fd5c1ae4790`)

## 利用候補module

| module | 用途 | 制約 |
|---|---|---|
| `ergo_input` | command入力 | OS pollは現状no-op、adapter必須 |
| `ergo_frame` | frame count / dt / FPS | simulation fixed tickとは分離 |
| `ergo_render` | Pictor frame orchestration | game固有GPU bridgeは本作側 |
| `ergo_ui` | headless UI bitmap / frame部品 | UI system全体ではない |
| `ergo_audio` / `ergo_sound` | SE / BGM候補 | backendを明示選択、silent dummy禁止 |
| `ergo_world_time` | presentation time scale候補 | simulation tickの正本にしない |
| `ergo_bind` | 開発時tuning | production gameplay stateの書換境界を限定 |
| `ergo_log` | runtime log | game event streamと混ぜない |

実装時は必要moduleだけenableする。

## Input adapter

現行`ergo_input`のplatform `poll()`はno-op。Pictorの
`GlfwSurfaceProvider::glfw_window()`へcallbackを登録し、Ergoのinject APIへ変換する。

```text
GLFW callback
  → ErgoInputAdapter
  → ergo_input double buffer
  → InputActionMap
  → PlayerCommand
```

adapterの責務:

- key / mouse button / pointer / scroll callback
- window座標→world rayはcamera/picking層へ委譲
- frame開始時のbuffer swap
- input action remap
- UI capture時にgame commandを抑止
- focus loss時にstuck keyを残さない

raw callbackからsimulation stateを直接変更しない。

first playableの実装 (`adapters/ergo/ergo_input_bridge`) は次を固定する。

- window user pointerはPictor `GlfwSurfaceProvider`が占有しているので、bridgeは
  自前のwindow→bridge登録表を使う。framebuffer size callbackは奪わない。
- `MouseDevice::injectPosition()`はdeltaを直前injectとの差で上書きするため、
  1 frame内の複数moveが畳まれる。pointerのposition / delta / scrollはbridgeが
  frame単位で累積した値を正本にし、Ergoへは真値として反映する。button / keyの
  状態とedge判定はErgo deviceが正本。
- `DoubleBuffer::swap()`はwrite bufferへ現在値を複製するので、scrollは
  frame終端で0をinjectして持ち越さない。
- focus lossでinject済みkey / buttonをすべてupへ戻し、そのframeの操作入力を
  捨てる。

## Frame / simulation

render frameの`dt`をsimulationへ直接積算せず、fixed-step accumulatorをapp層に置く。

```text
poll input
→ frame tick
→ accumulator += clamped render dt
→ while accumulator >= fixedDt:
     map commands
     simulation.tick()
→ publish/interpolate RenderSnapshot
→ render
```

catch-up上限とpause時の扱いは `TBD-RUNTIME-01`。tick dropが必要な場合はlogし、
結果をsilentに変えない。

first playableの `app::FixedStepDriver` はrender dtを0.25秒でclampし、1 frameで
消化するtickを `maxTicksPerFrame` (既定5) までに制限する。超過分はaccumulatorへ
残さず捨て、捨てた数をHUDとstderrへ出す。accumulatorへ残すと以後のframeが上限に
張り付いて復帰できない。最小化中はtickもGPU submissionも進めず、溜まった時間を
drainする。

## Render host

`ergo_render`はPictorの上でframe lifecycleを共通化するが、game-specificな
drawable変換、GPU asset store、batch sourceは本作側。

origin/main調査では`FrameComposer`のlayer initializationとrender pass設定順、
`StageRenderer`のpipeline作成順にcustom passでの整合リスクがある。
実装時に次をgateとする。

1. default passでlifecycleを確認
2. render passをpipeline作成前に確定できるcontractへ修正
3. PictorFrameBridgeのHDR / overlay passを統合

問題をadapterの呼出順偶然で隠さない。必要ならErgoへupstream fixをPRする。

### pinned Ergo / Pictorのgapに対するgame-owned owner

pinned Ergoのgapに対して、game側が次を所有する。いずれも回避策であり、
upstream修正で削除できるよう1箇所へ閉じ込める。

1. `LayerInitializationScope` + `TrackedRenderLayer` —
   `FrameComposer::initialize()`は全layer成功後にしか`initialized_`を立てず、
   途中例外ではdestructorの`shutdown()`がno-opになる。初期化済みlayerをscopeが
   登録順で覚え、失敗時に逆順で`shutdown()`する。
2. frame結果の取り出し — `run_frame()`はlegacyの`acquire_next_image()` /
   `present()`を呼び、skipでもtrueを返す。Pictor `02ea861c`以降はtyped
   `FrameResult`を`VulkanContext::last_frame_result()`で読めるので、frame前の
   `gate_frame()`とframe後の`last_frame_result()`から分類する
   ([Surface / device recovery](pictor-rendering.md#surface--device-recovery))。
   KD-MOB-002で旧`SwapchainIdentity`の同一性比較とdevice idle probeを廃止した。
3. `WorldFrameGraph::rebuild()` — `FrameComposer`は`add_pass()`時の
   `VkRenderPass`を差し替えられないので、swapchain再生成ではcomposerごと作り
   直す。順序はdevice idle → composer破棄 (layerが逆順にshutdown) →
   `WorldSceneTargets::resize()` → 同順で再初期化。extent 0のときは再構築を
   保留し、scene targetを作らない。window framebufferとswapchainの
   extent不一致をhostが検出した場合は、input / presentation publishより前に
   device idle → composer破棄 → `VulkanContext::recreate_swapchain()` →
   scene target / composer再構築を行い、そのframeはskipする。

## Render readiness

pinned Ergo `7f0d6bbd`の`ergo_render`はplatform-neutralなrender contractを持つ。
(contract自体は`b618de7a`で入り、`7f0d6bbd`はMSVCでreal render有効時の
`ERGO_RENDER_VULKAN_SOURCE`二重エスケープを直したrevision。これより前へは戻さない。)
KonbiniDominantはこのcontractを次のconsumer境界で使う。

| 境界 | contract | owner |
|---|---|---|
| surface | `RenderContext::surface`は`pictor::ISurfaceProvider*`を借用する。desktopは`GlfwSurfaceProvider`、Android / iOSはnative hostのproviderを同じ境界で渡す | `RenderDeviceHost` |
| configure | `ERGO_RENDER_REQUIRE_REAL=ON`で起動し、`ergo_render`の`ERGO_RENDER_HAS_VULKAN=1`と期待platform (`DESKTOP` / `ANDROID` / `IOS`) を検査する | `cmake/RequireErgoRealRender.cmake` |
| startup | `render_backend_contract()`が実描画有効かつ期待platformであること、`check_render_requirements(context)`が`None`であることを要求する | `requireRenderReady()` |
| composer | `FrameComposer::initialize()`の`RenderBackendError`を捨てない。`None`以外ならcomposerをshutdownして明示errorにする | `WorldFrameGraph` |

- 失敗は`RenderUnavailableError`として`RenderBackendError`を保持したまま
  起動経路へ投げる。Vulkan-free / headless成功へ縮退しない。
- 判定の正本はPictorの`PICTOR_HAS_VULKAN`をErgoが解決した結果であり、
  desktop専用`Vulkan::Vulkan`の有無ではない。mobile configureは
  `cmake/MobileVulkan.cmake`がtarget Vulkanを用意し、Pictorが
  `PICTOR_HAS_VULKAN`を公開する。
- surface lost中の`run_frame()`は`SurfaceNotReady`で`true`を返しframeを
  skipする。これは既存の`classifyFrameOutcome()`がsurface lostとして扱う。
- touch、native host lifecycle、packagingはKD-MOB-003〜006で扱う。

## Ergoを使わない領域

- `ergo_actor`: store / facility / population cellごとに作らない
- `ergo_scene`: runtime worldの正本にしない。look-dev document用途のみ
- `ergo_blackboard`: high-frequency store stateを置かない
- `ergo_physics2d`: ZOC / 256階 / dimension空間indexに使わない
- `ergo::math::ObjectPool<T>`: stable handleには利用可能だがcomponent SoAの代替にしない

DoD hot loopは本作のdense tablesを正本とする。

## UI

`ergo_ui`はRGBA bitmap生成等のbuilding blockとして使い、HUD stateは
simulationから生成した`HudViewModel`を入力にする。

- UI callback→PlayerCommand
- UIはcash / faith / phase等を直接書換えない
- animated bitmapを毎frameuploadする場合はGPU costを計測
- 256階 / dimension UIはgame-specific widgetとして本作側に置く

## Audio

game event→audio cue mappingをdata化する。backend欠落時に自動Dummyへ落ちて
「成功扱い」にしない。headless testだけは明示configでDummyを選べる。

Figmentumのformula audioを使う場合も、生成とruntime playbackのownershipを
明示し、frame threadで重い生成を行わない。

## Tools

game-specific editor / tuning surfaceはhost repositoryのplugin packに置き、
`ERGO_PLUGIN_DIR`で`tools/ergo`へ読み込ませる。

候補:

- chain / economy content editor
- N-KXi recipe inspector
- ZOC / Triangle debugger
- dimension / save inspector

toolはsimulation interface越しに操作し、内部tableのpointerを公開しない。

## Failure

- required input backend不在
- Pictor/Vulkan実描画経路不成立
- required Ergo module無効
- content / plugin schema mismatch

これらは起動時に明示error。capabilityが必須なのにno-op moduleへ自動縮退しない。
