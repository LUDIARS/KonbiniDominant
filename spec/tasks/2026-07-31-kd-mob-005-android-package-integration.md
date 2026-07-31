---
task: kd-mob-005-android-package-integration
project: KonbiniDominant
kind: 実装
status: pending
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 675
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-mob-001-ergo-platform-render-contract.md
  - spec/tasks/2026-07-31-kd-mob-002-pictor-surface-recovery.md
  - spec/tasks/2026-07-31-kd-mob-003-mobile-runtime-assets.md
  - spec/tasks/2026-07-31-kd-mob-004-touch-interface.md
  - spec/interface/mobile-platform.md
  - spec/setup/mobile-development.md
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
