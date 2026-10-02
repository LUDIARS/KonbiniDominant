---
task: kd-mob-005-android-package-integration
project: KonbiniDominant
kind: 実装
status: in_review
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 675
actio_task_id: "actio:7583d894-ef4a-409f-81cb-fa8c0cbc5eb4"
memory_links:
  - spec/tasks/2026-07-31-kd-mob-001-ergo-platform-render-contract.md
  - spec/tasks/2026-07-31-kd-mob-002-pictor-surface-recovery.md
  - spec/tasks/2026-07-31-kd-mob-003-mobile-runtime-assets.md
  - spec/tasks/2026-07-31-kd-mob-004-touch-interface.md
  - spec/interface/mobile-platform.md
  - spec/setup/mobile-development.md
value_ids:
  - UX-KD-W4
---

# KD-MOB-005 — Android package integration candidate

## 目的

Windows first playableと同じFigmentum都市 / DoD simulation / Pictor描画を
Android native hostへ接続し、実機検証へ渡せるinstallable package candidateを
生成する。

## 前提

- Windows KD-FP-001 implementationがreview済み
- KD-MOB-001〜004がreview済み
- dependency revisionがexactかつcleanと検証される

## 完了条件

- `arm64-v8a`向けNDK / CMake build targetを持つ
- thin Android host、manifest、Gradle packageを責務別に置く
- native windowをPictor `AndroidSurfaceProvider`へ渡す
- lifecycle、surface、touch、insets、memory / thermal eventを共通境界へforwardする
- host shader buildとNDK runtime Vulkan linkを分離する
- required content / SPIR-V / UI assetをread-only packageへ含める
- save / cacheをplatform writable rootへ置く
- Figmentum `planCity()`を端末process内で実行する
- polygonizeをframe loopで実行せず、low LOD / derived cache経路を使う
- explicit mobile graphics profileを選び、選択結果をdiagnosticへ記録する
- chain選択、facility選択、配置、cash、store count、revenue、ZOCを接続する
- package生成失敗やrequired capability不足を成功扱いしない

## Scope boundary

このtaskはpackage buildまで。install、startup、touch操作、background / resume、
actual-device性能確認はKD-MOB-007へ分離する。`playable`の成立はKD-MOB-007の
Android functional acceptance完了時とし、このtask単独では完成扱いしない。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / PRを作成して停止する
- build結果と未実施runtime確認をPR本文へ明記する
- unit / integration / behavior / startup testは明示指示なしに実行しない

## 実装内容

Actio: `actio:7583d894-ef4a-409f-81cb-fa8c0cbc5eb4`。価値ID: `UX-KD-W4`
(Windows / Android / iOSで同じsimulation・content・Figmentum cityを使い、
OS APIをplatform hostの外へ漏らさない)。KD-MOB-004 (`b279a3c`、KD #2280) の上に
実装し、PR提出前に`50602f0` (#2280再審査commit) をmergeした。型と契約の正本は
[mobile platform — Android package host](../interface/mobile-platform.md#android-package-host-kd-mob-005)、
toolchainは[mobile development setup](../setup/mobile-development.md#installed-android-toolchain-kd-mob-005-2026-10-02)。

- Android host を`native_main.cpp` (looper) / `AndroidHost` (lifecycle・window) /
  `android_touch_input` / `AndroidAssetReader` / `AndroidThermalMonitor`へ分割。
  `ANativeWindow`はPictor `AndroidSurfaceProvider`へ渡す
- lifecycle (pause / resume / start / stop)、surface (init / term / resize)、
  insets (`contentRect`)、memory (`LOW_MEMORY`)、thermal (`AThermal`) を
  KD-MOB-003の`LifecycleEventQueue` → `LifecycleState`へforwardし、touchは
  KD-MOB-004の`TouchSample`へ正規化
- required content / SPIR-V (HUD shaderがUI asset) をAPKから全件検査し、
  cacheへmirror。save / replay / settings / diagnosticsはfilesDir、
  geometry cacheはcacheDir (`androidWritableRoots`)
- `MobileGraphicsProfile` (`MobileHigh` / `MobileLow`) をboot時thermalで明示選択し、
  logcatと`diagnostics/graphics-profile.log`へ記録
- `FacilityMeshDetail`で`FigmentumCityAdapter`のpolygonize解像度 / LODを注入。
  `NativeMobileRuntime`は構築時 (frame前) に端末内で`planCity()`と1回の
  polygonizeを行い、memory pressureで派生CPU geometryを段階evictする
- chain選択、facility選択、配置、cash、store count、revenue、ZOCは
  desktopと同じ`GameSession` / HUD / `PointerInputController`経路をそのまま使う
  (Android専用game ruleは無い)
- package失敗の検出: `verifyKonbini<Variant>StagedAssets` (merge前) と
  `verifyKonbini<Variant>Apk` (assemble後) が`required-assets.txt`・
  `libkonbini_mobile.so`・ABIを検査し、欠落でbuild失敗。CMakeはarm64-v8a以外の
  ABIでconfigure失敗。Vulkan capability不足は`RenderInitError`でboot失敗
- host shader build (`glslc`、`konbini_shaders`) とNDK runtime Vulkan link
  (`vulkan`) は`MobileVulkan.cmake`の分離をそのまま使う
- Gradle 8.11.1 wrapperを追加 (checksum固定)
- NDK libc++ 18の浮動小数点`from_chars`欠落: KDのJSON parserはclassic locale
  streamへ切替、pinned Figmentum / Pictorにはforce-include shimを当てる
  (上流は編集しない)

### 判断 (BASE)

- BASE-MOB-005-PROFILE-01: profileはboot時thermalだけで選び、boot後のthermal変化は
  profileを切り替えず「held」として記録する。facility mesh levelはcity構築時に
  固定されるため。runtime切替はKD-MOB-007の実機計測後に判断
- BASE-MOB-005-LOD-01: `MobileHigh`=polygonize 16 / LOD 1、`MobileLow`=12 / LOD 2。
  world formatはdesktopと同じRGBA16F / D32 (composite passに他format経路が無いため)。
  packaged prebaked low LODは今回作らず、端末内polygonize + derived cacheを使う
- BASE-MOB-005-MIRROR-01: shader / content loaderはpathを受けるため、APKを正本に
  `<cacheDir>/konbini/package`へ毎bootでmirrorする (cacheはOSが消してよい)
- BASE-MOB-005-CHECKPOINT-01: background時の`checkpointRequested`はcanonical save
  形式が未決のためlogのみ。simulationはholdされresetしない
- BASE-MOB-005-CHARCONV-01: NDK libc++ < 20だけに効く`std::from_chars`浮動小数点
  overloadのshimをdependency targetへforce-includeする。上流修正は各repoの作業

### 再利用探索

- 採用: `LifecycleEventQueue` / `LifecycleState` / `DisplayMetrics` /
  `WritablePathProvider` / `IAssetReader` / `requireAssets` /
  `requiredPackagedAssets` (KD-MOB-003)、`TouchSample` / `TouchContacts` (KD-MOB-004)、
  `FacilityGeometryCache` / `DerivedGeometryCachePolicy` / `geometryEvictionScope`、
  `NativeMobileRuntime` / `GameSession`、`MobileVulkan.cmake` / `StageRuntimeAssets.cmake`
- 不採用: Pictor `PipelineProfile` (KDのworld passはPictor pipeline presetを使わず、
  名前だけ合わせた)、Pictor `MobileLifecycle` (KD側にlifecycle state machineが既にある)、
  GameActivity (Java/AGDK依存が増える。NativeActivityで足りる)

## 受け入れ条件

- C-1 konbini_mobile (mobile/CMakeLists.txt) / app/build.gradle: arm64-v8a専用のNDK / CMake targetで、他ABIはconfigure失敗、APKに他ABIがあればbuild失敗
- C-2 native_main / AndroidHost / android_touch_input / AndroidAssetReader / AndroidThermalMonitor: thin hostを責務別ファイルに置き、manifest・Gradle packageと分ける
- C-3 AndroidHost::attachWindow(): `ANativeWindow`をPictor `AndroidSurfaceProvider`へ渡す
- C-4 AndroidHost::onCommand() / drainLifecycle(): lifecycle、surface、insets、memory、thermalを`LifecycleEventQueue` → `LifecycleState`へforwardし、touchを`normalizeTouchSample`へ渡す
- C-5 MobileVulkan.cmake / konbini_mobile: host shader buildとNDK runtime Vulkan linkを分離する
- C-6 materializePackagedAssets(reader, requiredPackagedAssets(), root) / verifyKonbiniDebugApk: required content / SPIR-V / UI assetをread-only packageへ含め、欠落を全件報告して失敗する
- C-7 androidWritableRoots(dirs) / createWritableRoots(): save / cacheをplatform writable rootへ置き、packageのmirrorと分ける
- C-8 NativeMobileRuntime(assets, facilityMesh): Figmentum `planCity()`を端末process内で実行し、polygonizeはframe前に1回だけ、低LODで行う
- C-9 FigmentumCityAdapter::evictDerivedGeometry(scope): memory pressureで再生成可能geometryを段階evictする
- C-10 selectMobileGraphicsProfile(thermal) / GraphicsProfileDiagnostic: explicit mobile profileを選び、diagnosticへ記録する
- C-11 GameSession (desktopと共通): chain選択、facility選択、配置、cash、store count、revenue、ZOCを接続する
- C-12 verifyKonbini*StagedAssets / verifyKonbini*Apk / RenderInitError / WritableRootUnavailable: package生成失敗やrequired capability不足を成功扱いしない

Augur contract-wrapはTS/JS専用でC++のKDには仕込めないため、各契約は下記の
testとbuild検査を証跡にする。

| 契約 | 証跡 |
|---|---|
| C-1, C-2, C-3, C-5, C-12 (package) | `gradlew :app:assembleDebug` + `verifyKonbiniDebugApk` |
| C-4 (mapping), C-6, C-7, C-10 | `konbini_android_package_contract_tests` |
| C-8, C-9 | `konbini_figmentum_mesh_detail_tests` |
| C-11 | 既存`konbini_app_tests` / `konbini_phase1_tests` / `konbini_campaign_tests` / `konbini_touch_hud_layout_tests` (共通経路) |

## 検証

- 実施: Windows x64 Debug (MSVC, `KONBINI_BUILD_TESTS=ON`) build成功、
  `ctest -C Debug` 29/29 pass (2026-10-02、`b279a3c`上)。KD-MOB-004再審査commit
  `50602f0` (main `04b4b42` Gate 5取込済み) をmerge後に再build、30/30 pass
- 実施: `mobile/android/gradlew.bat --no-daemon :app:assembleDebug` 成功
  (NDK 27.2.12479018 / CMake 3.31.1 / AGP 8.9.2 / Gradle 8.11.1)。
  `verifyKonbiniDebugApk`がAPK内の7 assetと`lib/arm64-v8a/libkonbini_mobile.so`を確認。
  `app-debug.apk` 10,936,267 bytes (`50602f0` merge後、sha256
  `41553bba5c8947d0995c2db445fc64c593b038befe278000acf7b669dd84ba3d`)
- 実施 (main `e2dc9e2` merge後、2026-10-02): KD-MOB-004 squash `f2780ff` と
  KD-NPC-002 `e2dc9e2` を取込。`native_mobile_runtime` はmain側の変更
  (MOB-004の`displayMetrics` / `touch(TouchSample)` / safe area) が本branchに既に
  あるため本branch側を採用し、memory pressure / facility mesh経路を残した。
  NPC-002のresident / speech bubble / landing effect同期は`GameSession` /
  `FramePresenter` / Pictor `presentation_object_sync`側にあり、競合無くmerge済み。`cc.acceptance.json`は両側のimplementationsをsource単位で和集合にした。
  Windows x64 Debug再build成功、`ctest -C Debug` 33/33 pass。
  `assembleDebug`成功、`verifyKonbiniDebugApk`は7 asset + `libkonbini_mobile.so`を確認、
  `app-debug.apk` 11,296,163 bytes (sha256
  `7bbe59fe0055aa2eea358710cc30beb94def5588fd11dc326fd1e94fafcaccf6`)
- 判断 BASE-MOB-005-SPEECH-01: NPC-002の日本語speech glyph atlas
  (`src/render/generated/noto_sans_jp_speech_mesh.inc`) とremark catalogは
  binaryへbakeされ、runtimeにfont / locale fileを読まない。よって
  `required-assets.txt` / APK検査へは追加しない。Noto Sans JP OFL noticeの
  Android同梱はRoboto Mono同様に未対応で、release packaging (署名と同時) で扱う
- 未実施 (KD-MOB-007へ分離): install、起動、touch操作、background / resume、
  thermal / memory event実発火、実機性能。release署名、Android Studio IDE sync

## 復旧方法

save形式とsimulation ruleに変更は無い。desktopの`FigmentumCityAdapter`既定値は
従来のpolygonize 24 / LOD 0のまま。不具合時はこのPRのsquash commitをrevertすれば
KD-MOB-004時点のAndroid glue (wrapper無し) へ戻る。開発機に導入したSDK部品は
`sdkmanager --uninstall`で個別に外せる。
