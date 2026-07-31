# Smartphone platform contract

## Purpose

REQ-PLATFORM-01を、Windows first playableのruleと決定性を壊さず
Android / iOSへ展開するためのplatform境界を定義する。

Androidを最初の実装・実機検証対象とし、その過程で確立した共通C++境界を
iOSへ接続する。Android専用game ruleやiOS専用simulationを作らない。

## Product scope

| platform | role | graphics host |
|---|---|---|
| Windows | 現行first playable / desktop基線 | GLFW + Pictor Vulkan |
| Android | first mobile target | native host + `AndroidSurfaceProvider` |
| iOS | Android後のmobile parity target | native host + `IOSSurfaceProvider` + MoltenVK |

mobile版はdesktop版と同じPhase、content、DoD simulation、Figmentum recipe、
save / replay contractを使う。画面密度、safe area、入力、描画profile、
package形式だけをplatform差分とする。

最低OS version、support device tier、portrait対応は
[open questions](../faq/open-questions.md)で確定する。

## Inspected dependency capability

固定Pictor
`c088e8d1b7b9e2625b7a8d923c89d4d684566c16`には次の足場がある。

- platform-neutral `ISurfaceProvider`
- `AndroidSurfaceProvider` / `IOSSurfaceProvider`
- Android Vulkan / iOS MoltenVK向けCMake分岐
- pause / resume / suspend / surface loss
- memory pressure / thermal state
- `MobileLow` / `MobileHigh` profile

これらはhostから接続するlibrary APIであり、KonbiniDominantのAPK / iOS app、
touch入力、asset packaging、actual-device成功を意味しない。

固定Ergo
`771b027f0e5492015b27f54c3bab1fd5c1ae4790`には次のgapがある。

- `ergo::render::RenderContext::surface`が
  `pictor::GlfwSurfaceProvider*`へ固定されている
- real render pathの判定がdesktop `find_package(Vulkan)` /
  `Vulkan::Vulkan`を前提にする
- UI pointerはsingle pointerで、finger ID / pinch / touch cancelを持たない
- asset pathは通常filesystem上のpathを前提にする

ErgoのgapはErgo repositoryのbranch / PRで直す。KonbiniDominantへ
Ergo実装をcopyしない。

Figmentum
`3ee998f487d984f54003c4ec3c4f7ba00b53eec3`の`CityPlan`はrenderer非依存で、
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

- game domainへ`ANativeWindow`、`CAMetalLayer`、GLFW、Vulkan handleを漏らさない
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

## Build and package contract

toolchainとpackage layoutは
[mobile development setup](../setup/mobile-development.md)を正本とする。

host shader compilerと端末runtime Vulkanを同じdependencyとして扱わない。
SPIR-Vはhost buildで生成または検証済みartifactをpackageし、端末上で`glslc`を
要求しない。

## Failure policy

次を成功扱いしない。

- required Vulkan / MoltenVK capability不在
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
