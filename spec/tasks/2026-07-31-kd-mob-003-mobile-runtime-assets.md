---
task: kd-mob-003-mobile-runtime-assets
project: KonbiniDominant
kind: 実装
status: in_review
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 673
actio_task_id: "actio:cce62463-f179-48be-acaa-8fe93e26c8a4"
memory_links:
  - spec/tasks/2026-07-31-kd-mob-001-ergo-platform-render-contract.md
  - spec/tasks/2026-07-31-kd-mob-002-pictor-surface-recovery.md
  - spec/interface/mobile-platform.md
  - spec/setup/mobile-development.md
  - spec/data/save-format.md
value_ids:
  - UX-KD-W4
---

# KD-MOB-003 — Mobile runtime and asset boundary

## 目的

KonbiniDominantへOS非依存のlifecycle、display、asset、writable path境界を追加し、
desktop / Android / iOS hostが同じapp ownerへeventを渡せるようにする。

> **iOS は Metal へ変更 (2026-10-03)**: neco決定によりiOSはMoltenVKを使わずPictor
> Metal backendでMetal直描画する ([mobile-platform](../interface/mobile-platform.md#ios描画方針-2026-10-03-決定)、
> [KD-MOB-006](2026-07-31-kd-mob-006-ios-package-integration.md#前提-上流タスク))。
> 本taskのlifecycle / display / asset / writable path境界はgraphics APIに依存せず、iOS Metalでもそのまま使う。iOS bundle版`IAssetReader`の接続はKD-MOB-006。完了済みの内容は変えない。

## 前提

- KD-MOB-001 / KD-MOB-002のreview済みAPIを固定revisionで利用する
- Windows first playableのapp owner / shutdown順が確定している

## 完了条件

- lifecycle eventをqueueし、app owner threadで順序付けて適用する
- pause / suspend中にfixed tickと新規game commandを進めない
- surface loss中もauthoritative simulation stateを保持する
- safe checkpointをtick境界のimmutable stateから要求できる
- `DisplayMetrics` / safe area / orientation changeをrender / UIへ通知する
- packaged read-only assetをfilesystem絶対pathなしで読める
- save / replay / settings / cacheのwritable rootをplatform hostから注入する
- required asset / writable root欠落をfail-fastする
- Figmentum geometry cache keyへrevision / recipe / LOD / vertex formatを含める
- memory pressureで再生成可能cacheだけをevictできる
- `konbini_sim`へOS / Pictor / Ergo / Vulkan headerを導入しない

## 責務分割

- lifecycle queue
- display metrics value
- read-only asset reader
- writable path provider
- derived geometry cache policy

各責務は別class / fileとし、汎用`PlatformManager`へ集約しない。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / PRを作成して停止する
- unit / integration / behavior / startup testは明示指示なしに実行しない

## 実装結果 (2026-10-02)

Actio: `actio:cce62463-f179-48be-acaa-8fe93e26c8a4`。価値ID: `UX-KD-W4`。
KD-MOB-002 (KD #2249) はmainへmerge済みで、その上に実装した。
責務と型の対応は[mobile platform](../interface/mobile-platform.md#runtime-boundary-owners)を正本とする。

### 受け入れ条件

- C-1 LifecycleEventQueue::drain(): OS threadからのeventをpush順でowner threadへ渡し、owner以外のdrainとoverflowはfail-fastする
- C-2 planLifecycleTicks(state, driver, dt) / admitGameCommand(state, command): pause / background / surface loss中はtick 0、accumulatorを捨て、新規game commandを拒否する
- C-3 LifecycleState::apply(SurfaceLost): GPU submissionを止め、simulation stateを保持する。`SurfaceAvailable`で再構築を要求する
- C-4 SafeCheckpointLedger::request(): 最後のtick境界のimmutable checkpointを返す
- C-5 DisplayMetricsChannel::publish(metrics): 値が変わった時だけ購読順にextent / density / safe area / orientationの変化を通知する
- C-6 IAssetReader::read(name): package相対名だけで読み、絶対path / traversalを拒否する。`requireAssets`は欠落を全件列挙して失敗する
- C-7 WritablePathProvider(roots): save / replay / settings / cache / diagnostic rootをhostから受け、欠落・相対・非directoryをfail-fastする
- C-8 FacilityGeometryCacheKey: revision / recipe / LOD / vertex formatを含む (schema 3)
- C-9 FacilityGeometryCache::evict(policy, scope): packaged low LODを残し、再生成可能geometryだけを段階的にevictする
- C-10 konbini_sim: OS / Pictor / Ergo / Vulkan / Figmentum header・linkを含まない

C-1〜C-4は`tests/app/platform/lifecycle_runtime_test.cpp`、C-5は
`display_metrics_test.cpp`、C-6 / C-7は`asset_boundary_test.cpp`、C-8 / C-9は
`tests/city/derived_geometry_cache_policy_test.cpp`、C-10は
`tests/cmake/sim_platform_isolation_test.cmake`で確認した。

### 検証

実施:

- Windows x64 / MSVC 19.39 / Visual Studio 17 2022、Debug、`KONBINI_BUILD_TESTS=ON`で
  新しいbuild directoryからconfigure + 全target build (render / Figmentum adapter有効)。
  初回の並列buildはhostのpaging file不足 (CL.exe) で止まったため、同じdirectoryで
  `--parallel 2`により継続し成功した
- `ctest -C Debug`: 24/24 pass

未実施:

- desktop / mobile appの起動 (指示により起動テストなし)
- Android NDK / iOS build、実機lifecycle (KD-MOB-005 / 006 / 007)

### 残り

- `NativeMobileRuntime`とAndroid / iOS hostから`LifecycleEventQueue`へeventを流す接続、
  `AAssetManager` / iOS bundle版`IAssetReader`はpackage統合 (KD-MOB-005 / 006) で行う
