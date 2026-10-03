# Smartphone platform contract

## Purpose

REQ-PLATFORM-01を、Windows first playableのruleと決定性を壊さず
Android / iOSへ展開するためのplatform境界を定義する。

Androidを最初の実装・実機検証対象とし、その過程で確立した共通C++境界を
iOSへ接続する。Android専用game ruleやiOS専用simulationを作らない。

## iOS描画方針 (2026-10-03 決定)

neco決定 (2026-10-03):「iOS は Metal で描画するようにします」。iOSはPictorの
Metal backend (`pictor::MetalContext`) で直接描画し、MoltenVK (Vulkan
portability over Metal) は使わない。Androidは従来どおりVulkanのまま変えない。

理由と根拠:

- Pictor自身が同じ方針を持つ (Pictor `spec/feature/metal-backend-design.md`
  「iOS surface は Metal 直呼び。Android は既存 Vulkan を維持」)。KDだけが
  MoltenVKを抱えると、Pictor上流が保証しない経路をgame側が保守することになる
- Pictor main `502ba022` の`include/pictor/surface/metal_context.h`に
  `MetalContext` (`initialize(CAMetalLayer*)` / `acquire_frame` /
  `present_frame` (`FrameResult`) / `recover_surface` / `recover_device` /
  `upload_mesh`) がある

制約 (2026-10-03時点):

- Pictor `PC-FEAT-PLAT-05` (`spec/feature/platform-feature-matrix.md`) により
  `MetalContext`はmesh upload / drawと復旧APIだけで、各描画効果のMetal実装を
  意味しない。iOSのbuild・実機はPictor側でも未確認
  (Pictor `spec/tasks/2026-09-10-ios-metal-validation.md`)
- KDの描画 (Gate 5 bridge、presentation objects、offscreen world composition、
  HUD) はPictorのVulkan pipelineとVulkan handleに直接乗っており、Metalには無い
- Ergoの描画契約 (`ergo_render`の`RenderBackend`、
  `cmake/ErgoRenderBackend.cmake`) はiOSを「pictorのみ / MoltenVK、
  `ERGO_RENDER_PLATFORM_IOS=1`」としてVulkan前提で扱う

このため、iOS packageは上流 (Pictor Metal backendの拡張、Ergo描画契約への
iOS Metal追加、Pictor iOS Metal validation) の完了を前提とする。前提と
KD側で外すMoltenVK資産は
[KD-MOB-006](../tasks/2026-07-31-kd-mob-006-ios-package-integration.md#前提-上流タスク)
を正本とし、未決の判断は[open questions](../faq/open-questions.md)の
`TBD-IOS-METAL-*`に置く。

旧方針 (2026-07-31〜2026-10-02): iOSはPictor Vulkan + MoltenVK +
`CAMetalLayer`で描画し、game-owned portability adapter
(`mobile/ios/vulkan_portability.cpp`) でMoltenVKのportability extensionを
有効化する。KD-MOB-006の実装で撤去するまでコードには残る (下記
[Build and package contract](#build-and-package-contract))。

## Product scope

| platform | role | graphics host |
|---|---|---|
| Windows | 現行first playable / desktop基線 | GLFW + Pictor Vulkan |
| Android | first mobile target | native host + `AndroidSurfaceProvider` |
| iOS | Android後のmobile parity target | native host + `CAMetalLayer` + Pictor `MetalContext` (Metal直描画。2026-10-03に旧方針MoltenVKから変更) |

mobile版はdesktop版と同じPhase、content、DoD simulation、Figmentum recipe、
save / replay contractを使う。画面密度、safe area、入力、描画profile、
package形式だけをplatform差分とする。

最低OS version、support device tier、portrait対応は
[open questions](../faq/open-questions.md)で確定する。

## Inspected dependency capability

固定Pictor
`502ba022b66fbba0356b820fc17a064abe37435e` (KD-MOB-002のPictor #2243 にKD-MAC-001でPictor #2309の浮動小数点parseを加えた)
には次の足場がある。

- platform-neutral `ISurfaceProvider`
- `AndroidSurfaceProvider` / `IOSSurfaceProvider` (host所有のまま)
- Android Vulkan向けCMake分岐 (iOS MoltenVK分岐もあるが、KDのiOS描画経路には使わない)
- iOS `MetalContext` (mesh upload / drawと復旧APIのみ。`PC-FEAT-PLAT-05`)
- pause / resume / suspend / surface loss
- memory pressure / thermal state
- `MobileLow` / `MobileHigh` profile
- typed `FrameResult` (`Ready` / `RecreateSwapchain` / `SurfaceLost` /
  `DeviceLost` / `Suspended` / `NotInitialized` / `Error`) と
  `swapchain_recreated`
- `VulkanContext::set_presentation_suspended()`による停止中のGPU submission抑止
- `ContextInitStatus` (`SurfaceUnavailable` / `MissingInstanceExtension` /
  `MissingDeviceExtension` / `MissingCapability` ほか)。MoltenVK
  portabilityのcapability検査も持つが、KDのiOS経路では使わない (旧方針)

KDでの対応付けは
[Pictor rendering contract](pictor-rendering.md#surface--device-recovery)。

これらはhostから接続するlibrary APIであり、KonbiniDominantのAPK / iOS app、
touch入力、asset packaging、actual-device成功を意味しない。

固定Ergo
`7f0d6bbd34dced4fc6664a5f04bce9910e893537` (KD-MOB-001で更新) は次を満たす。

- `ergo::render::RenderContext::surface`は`pictor::ISurfaceProvider*`を借用し、
  公開render headerは`GlfwSurfaceProvider`具象型を要求しない
- real render pathの判定はPictorの`PICTOR_HAS_VULKAN`を正本とし、
  Android / iOSでdesktop `Vulkan::Vulkan`不在だけでrenderを無効化しない
  (iOSは`ERGO_RENDER_PLATFORM_IOS=1`でもVulkanを要求する。iOS Metalを描画契約へ
  加えるのはErgo側の前提task。[KD-MOB-006](../tasks/2026-07-31-kd-mob-006-ios-package-integration.md#前提-上流タスク))
- 実描画不可は`RenderBackendError`の型付き値で返し、mobile configureは
  real render不成立を構成errorにする

consumer側の検査は[Ergo runtime contract](ergo-runtime.md#render-readiness)。
残るgapは次の通りで、後続taskで扱う。

- UI pointerはsingle pointerで、finger ID / pinch / touch cancelを持たない
  (KD-MOB-004ではgame側のgesture recognizerで補い、Ergoへは主contactだけを
  mouse pointerとしてinjectする。Ergo touch deviceの追加はErgo側task)
- asset pathは通常filesystem上のpathを前提にする

ErgoのgapはErgo repositoryのbranch / PRで直す。KonbiniDominantへ
Ergo実装をcopyしない。

Figmentum
`ff09a65db1a1db6711537a7ce49f207ca068c8b5`の`CityPlan`はrenderer非依存で、
同じseed / paramsから端末上でも再生成できる。同期polygonizeをframe loopで
実行せず、geometry cacheは再生成可能な派生dataとして扱う。

## Required boundaries

```text
Android native host / iOS native host / Windows host
  ├─ NativeSurface
  ├─ AppLifecycle
  ├─ DisplayMetrics + SafeArea
  ├─ Touch / pointer events
  ├─ AssetReader + WritablePaths
  └─ Pictor ISurfaceProvider
        ↓
Ergo frame / input orchestration
        ↓
InputActionMap → PlayerCommand
        ↓
Fixed-step Konbini simulation
        ↓
immutable RenderSnapshot → Pictor
```

platform hostはOS callbackを正規化するだけで、cash、facility、store、
phase等のgame stateを直接変更しない。

### Surface and renderer

- game domainへ`ANativeWindow`、`CAMetalLayer`、GLFW、Vulkan handle、
  Metal object (`MTLDevice` / `MTLCommandBuffer` 等) を漏らさない
- iOSのMetal呼び出しはPictor `MetalContext`とその上流拡張だけが持つ。KDは
  Pictorを迂回してMetalを直接呼ばない
- Ergoは`ISurfaceProvider`をborrowし、具象providerを所有しない
- `SurfaceLost`、`RecreateSwapchain`、`DeviceLost`を異なる結果として扱う
- surface消失中はGPU workをsubmitしない
- surface復帰後はswapchain依存resourceを新規作成してから旧resourceを解放する
- device lostをresizeとしてsilent retryしない

desktopのRGBA16F + D32 world targetはmobileでも最初にcapability検査する。
未対応端末では、versioned mobile profileに宣言されたformat / render scaleだけを
利用できる。ad-hocなsilent fallbackは禁止し、選択profileをlog / diagnosticへ残す。

### Lifecycle

| event | required behavior |
|---|---|
| inactive / pause | 新規game command受付を止め、fixed tickをpause |
| background / suspend | GPU submission停止、safe checkpoint要求 |
| surface lost | swapchain依存resourceを無効化し、simulation stateを保持 |
| surface regained | extent / safe areaを再取得してrender resourceを再構築 |
| memory pressure | 再生成可能なmesh / texture cacheから段階的にevict |
| thermal pressure | 明示profile / render scaleへ変更。simulation ruleは不変 |

OS callback threadからrendererやsimulationを直接変更しない。event queueを介して
app owner threadで順序付ける。

### Input and UI

native touch contactはfinger ID、phase、position、timestampを保持して
gesture adapterへ渡す。gesture adapterはtap、drag、pinch、cancelを判定し、
Ergo input buffer / `InputActionMap`へsemantic actionをinjectする。

- UI capture中のcontactをworld placementへ流さない
- drag開始後に同じcontactをtapとして確定しない
- multi-touch終了 / app pause時にstuck pointerを残さない
- hoverだけで得られる情報を作らない
- placementはselectionと明示confirmを分ける
- safe areaとdisplay densityをlayout inputにする

具体的な操作は [UI / UX](../feature/ui-ux.md) を正本とする。

KD-MOB-004で次の責務別型を置いた。`konbini_app_domain`
(`include/konbini/app/input/` ほか) はOS、Ergo、GLFW headerを含まない。

| 責務 | 型 | 契約 |
|---|---|---|
| contact正規化 | `TouchSample` / `normalizeTouchSample` | finger ID、phase、framebuffer position、platform timestamp (秒) を保持する。非有限position / timestamp、負timestamp、非正scaleは`std::invalid_argument`。`Cancel`はpositionを検証しない |
| contact table | `TouchContacts` | 全host共通 (Windows `WM_TOUCH`、Android、iOS)。1 frame内のpress / release edgeを保持し、同じ2指のmotionだけをdrag deltaへ足す。timestamp逆行・容量超過はgestureを破棄してから例外。cancel後のMove / Upは無視する |
| tap判定 | `TapRecognizer` | 一度disqualifyされた (drag開始、2本目の指、cancel) sequenceはpress位置へ戻ってもtapにしない |
| drag判定 | `DragRecognizer` | press位置から`10 * uiScale` pixelを超えるか2本目の指でdragへlatchし、pan deltaを出す |
| pinch判定 | `PinchRecognizer` | 同じ2指の距離比だけを積む。指の増減でpairが変わったframeはzoomしない |
| cancel判定 | `recognizeGestureCancel` | contact cancel、focus喪失 / app pause、surface resize / orientation、phase等のcontext変更で進行中gestureを破棄する |
| UI routing | `PointerInputController` | 押下時にUI captureを決め、captureしたgestureからworld click / pan / pinchを作らない。touch tapは`primaryClickFromTouch` |
| Ergo injection | `adapters::ergo::planTouchPointerInjection` | 主contactをErgo mouse位置と左buttonへ写す。release / cancel後は左buttonを残さない。gestureの意味はErgoへ渡さない |
| HUD layout input | `HudLayoutMetrics` / `hudLayoutFromDisplay` | `DisplayMetrics`のextent、safe area、densityとplayer UI scale (0.75〜1.5) |

`NativeMobileRuntime::touch(TouchSample)`はpresentationがsubmit不可 (pause、
background、surface loss) の間contactを捨て、`pause` / `resize` /
`displayMetrics`はcontactとgestureを破棄する。selectionとsimulation stateは
触らない。

### Assets and generated geometry

read-only packaged assetとwritable dataを分離する。

```text
AssetReader
  read-only content / shader / UI / prebaked low LOD

WritablePaths
  save / replay / settings / generated geometry cache / diagnostic
```

通常filesystem pathへ展開されていることを前提にしない。shader / content loaderは
byte readerまたはplatformが保証したmaterialized pathを受け取る。

Figmentum `planCity()`は端末内で実行し、都市形成の正本を維持する。
geometryは次のkeyでcache可能にする。

```text
Figmentum revision
+ recipe/schema version
+ facility recipe hash
+ LOD
+ vertex format version
```

cache miss時のpolygonizeはframe loopで同期実行しない。低LOD同梱または
cancel可能なbackground generationを使い、memory / thermal pressureで
派生cacheを破棄できるようにする。

### Determinism

- platform clock、frame rate、touch sample countをgame resultへ使わない
- gestureから生成したcommandをfixed tickへ正規化する
- 同じworld seedと正規化済みcommand列はplatform間で同じcanonical snapshotになる
- graphics profile、render scale、safe areaはcanonical saveへ含めない
- autosave / checkpointはtick境界のimmutable stateだけを永続化する

### Runtime boundary owners

KD-MOB-003で上記境界をOS / GPU非依存のgame側型として置いた。すべて
`konbini_app_domain` (`include/konbini/app/platform/`) と`konbini_city`にあり、
OS、Pictor、Ergo、Vulkan headerを含まない。汎用`PlatformManager`へは集約しない。

| 責務 | 型 | 契約 |
|---|---|---|
| lifecycle queue | `LifecycleEventQueue` | OS callback threadから`push` (noexcept、事前確保の有界queue)。app owner threadだけが`drain`し、push順で返す。overflowはlatchし、次の`drain`で`LifecycleQueueOverflow` |
| lifecycle適用 | `LifecycleState` | eventを順に適用し`LifecycleEffects`を返す。simulation stateは所有もresetもしない |
| tick / command gate | `planLifecycleTicks` / `admitGameCommand` | pause / background / surface loss中はfixed tickを0にし、accumulatorを捨てる。新規game commandは拒否し、resume後へ持ち越さない |
| safe checkpoint | `SafeCheckpointLedger` | 完了tickごとのcanonical snapshotをimmutableな`SafeCheckpoint`として保持し、要求には最後のtick境界を返す |
| display metrics value | `DisplayMetrics` | extent、density、safe area、orientationの値。空extent、非正density、usable領域0は`std::invalid_argument` |
| display通知 | `DisplayMetricsChannel` | 値が変わった時だけ、購読順 (render → UI) に変更前後を通知する |
| read-only asset reader | `IAssetReader` / `DirectoryAssetReader` | package相対名でbytesを読む。絶対path、drive、`\`、`.` / `..`、空segmentは拒否。`requireAssets`は欠落を全件列挙して失敗する |
| writable path provider | `WritablePathProvider` | save / replay / settings / cache / diagnosticのrootをhostが注入する。空、相対、非directoryのrootは`WritableRootUnavailable`。再生成可能なのはcacheだけ |
| derived geometry cache policy | `DerivedGeometryCachePolicy` | packaged low LODは対象外。memory pressure moderateは未使用の再生成geometry、criticalは再生成geometry全件をevictする |

| event | state | `LifecycleEffects` |
|---|---|---|
| `Pause` / `Resume` | paused切替 | tickとcommandだけ止める。直前frameの表示は許す |
| `EnterBackground` | suspended | 遷移時だけ`checkpointRequested`と`gpuSubmissionStopped` |
| `EnterForeground` | suspended解除 | — |
| `SurfaceAvailable` | surfaceあり | metrics検証、`renderRebuildRequested`、`display` |
| `SurfaceLost` | surfaceなし | `gpuSubmissionStopped`。simulation stateは保持 |
| `DisplayChanged` | — | metrics検証、`display`。surfaceがある時だけ`renderRebuildRequested` |
| `MemoryPressure` | — | batch内で最も重いlevel |
| `ThermalPressure` | — | batch内で最後のlevel。simulation ruleは読まない |

fixed tickとgame commandは`!paused && !suspended && surfaceAvailable`の時だけ進む。
GPU submissionは`!suspended && surfaceAvailable`。

Figmentum geometry cache keyは`schemaVersion` (recipe/schema version、3)、
`generatorRevision`、`recipeHash`、`polygonizeResolution`、`lod`、
`vertexFormatVersion`を持つ。first playableのLODは0 (最精細) だけである。

### Android package host (KD-MOB-005)

`mobile/android/`のthin NativeActivity hostは責務別に分ける。OS headerを含むのは
このdirectoryだけで、game stateは変更しない。

| 責務 | 置き場所 | 契約 |
|---|---|---|
| looper entry | `native_main.cpp` | `android_main`。boot後にcallbackを`AndroidHost`へ渡すだけ |
| lifecycle / window owner | `AndroidHost` | `APP_CMD_*`を`LifecycleEventQueue`へ積み、owner threadで`LifecycleState`が決めた効果を`NativeMobileRuntime`へ適用する。`TERM_WINDOW`はcallback内で同期detachしてから`SurfaceLost`を積む |
| touch | `forwardMotionEvent` | pointer ID、phase、window pixel、event時刻 (CLOCK_MONOTONIC) を`normalizeTouchSample`へ渡す。`ACTION_CANCEL`は全pointerをcancel |
| insets | `safeAreaFromContentRect` | `android_app::contentRect`外のwindow端をsafe areaにする (window内へclamp) |
| memory | `APP_CMD_LOW_MEMORY` | `MemoryPressure(Critical)`。`NativeMobileRuntime::memoryPressure`が`FigmentumCityAdapter`の再生成可能CPU geometryを段階evictする。描画中GPU bufferとsimulationは保持 |
| thermal | `AndroidThermalMonitor` | API 30の`AThermal`をruntime解決し、binder threadから`ThermalPressure`をpushする。API 29ではstatus無し (`thermal=unavailable`) |
| package asset | `AndroidAssetReader` / `materializePackagedAssets` | APKが正本。`requiredPackagedAssets()`を全件検査してから`<cacheDir>/konbini/package`へmirrorする (`.partial`→rename) |
| writable root | `androidWritableRoots` | save / replay / settings / diagnosticsは`<filesDir>/konbini`、geometry cacheは`<cacheDir>/konbini/geometry`。作成失敗は`WritableRootUnavailable` |

Activity pause (`APP_CMD_PAUSE`) とfocus喪失はどちらも「inactive」で、
両者を合わせた変化だけを`Pause` / `Resume`として積む。`START` / `STOP`は
`EnterForeground` / `EnterBackground`。`EnterBackground`の`checkpointRequested`は
canonical save形式が未決のためlogだけ残す (simulationはholdされresetしない)。

#### Mobile graphics profile

`MobileGraphicsProfile` (version 1) はgameが宣言するmobile profileである。

| profile | world color / depth | render scale | facility polygonize / LOD |
|---|---|---|---|
| desktop (参考) | RGBA16F / D32 | 1.0 | 24 / 0 |
| `MobileHigh` | RGBA16F / D32 | 1.0 | 16 / 1 |
| `MobileLow` | RGBA16F / D32 | 1.0 | 12 / 2 |

`selectMobileGraphicsProfile`はboot時のthermal levelだけで選ぶ (serious /
criticalなら`MobileLow`、それ以外とstatus無しは`MobileHigh`)。端末名は読まない。
world formatは`WorldSceneTargets`がdevice init時にcapability検査し、未対応端末は
`RenderInitError`でboot失敗にする (別formatへのsilent fallbackはしない)。
選択結果は`GraphicsProfileDiagnostic`が`diagnostics/graphics-profile.log`へ書き、
logcatにも出す。boot後のthermal変化はprofileを切り替えず「held」として追記する
(facility mesh levelはboot時に固定。runtime切替はKD-MOB-007の実機計測後に判断)。

Figmentum `planCity()`とfacilityごとの`polygonize`は`NativeMobileRuntime`の
構築時 (最初のframe前) に端末process内で1回だけ実行し、frame loopでは
polygonizeしない。`FacilityMeshDetail`はcache keyの`polygonizeResolution` / `lod`へ
そのまま入る。

## Build and package contract

toolchainとpackage layoutは
[mobile development setup](../setup/mobile-development.md)を正本とする。

host shader compilerと端末runtime graphics APIを同じdependencyとして扱わない。
Android / desktopのSPIR-Vはhost buildで生成または検証済みartifactをpackageし、
端末上で`glslc`を要求しない。iOSのMetal shader (`.metallib`) の生成・同梱経路は
Pictor Metal backendの拡張に従い、KD-MOB-006で確定する (`TBD-IOS-METAL-SHADER-01`)。

現行コードには旧方針のiOS MoltenVK分岐 (`cmake/MobileVulkan.cmake`のiOS分岐、
`mobile/ios/vulkan_portability.cpp`、`mobile/CMakeLists.txt`のiOS
`vkCreateInstance` / `vkCreateDevice`置換) が残る。2026-10-03時点では変更せず、
KD-MOB-006の実装で外す。

## Failure policy

次を成功扱いしない。

- required Vulkan (Android / desktop) / Metal (iOS) capability不在
- iOSでMetal backendが未実装の描画機能を、黙って省略して起動成功に見せること
- providerとnative surface typeの不一致
- packaged shader / content不在
- writable save / cache rootを取得できない
- lifecycle event queue overflow
- unsupported mobile profileへのsilent変更
- surface / device lossを通常frame skipへ畳む

明示的なgraphics capability劣化だけは許容する。gameplayを削るfallbackや、
空のcity / no-op inputで起動成功に見せることは禁止する。

## Delivery sequence

設計判断は
[KD-MOB-000](../tasks/2026-07-31-kd-mob-000-smartphone-contract.md)へ記録し、
実装・検証を次の順に分離する。

1. [KD-MOB-001 — Ergo mobile dependency integration](../tasks/2026-07-31-kd-mob-001-ergo-platform-render-contract.md)
2. [KD-MOB-002 — Pictor mobile recovery integration](../tasks/2026-07-31-kd-mob-002-pictor-surface-recovery.md)
3. [KD-MOB-003 — mobile runtime / asset boundary](../tasks/2026-07-31-kd-mob-003-mobile-runtime-assets.md)
4. [KD-MOB-004 — touch input / responsive HUD](../tasks/2026-07-31-kd-mob-004-touch-interface.md)
5. [KD-MOB-005 — Android package integration](../tasks/2026-07-31-kd-mob-005-android-package-integration.md)
6. [KD-MOB-006 — iOS package integration](../tasks/2026-07-31-kd-mob-006-ios-package-integration.md)
7. [KD-MOB-007 — actual-device validation](../tasks/2026-07-31-kd-mob-007-device-validation.md)

## Out of scope

- web / browser版
- mobile専用simulation fork
- native UIへgame ruleを複製すること
- Figmentumをoffline meshだけへ置換すること
- Android task内でiOS署名や配布を同時実装すること
- 実機未確認のpackageをmobile完成扱いすること
