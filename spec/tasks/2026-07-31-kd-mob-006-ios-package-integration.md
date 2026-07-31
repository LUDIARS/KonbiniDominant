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
---

# KD-MOB-006 — iOS package integration candidate

## 目的

Androidで確立した共通mobile contractをiOS native hostへ接続し、
Pictor / MoltenVK経由で実機検証へ渡せるiOS package candidateを生成する。

## 前提

- KD-MOB-001〜005がreview済み
- macOS / Xcode / iOS SDK / MoltenVKを明示的に解決できる
- signing identity / team IDをrepository外から注入できる

## 完了条件

- Objective-C++ thin hostとC++ game ownerを分離する
- `UIView`の`CAMetalLayer`をPictor `IOSSurfaceProvider`へ渡す
- MoltenVK portability extension / featureをfail-fastで検査する
- lifecycle、surface、touch、safe area、memory / thermal eventを共通境界へforwardする
- bundle assetとwritable Application Support / Cachesを分離する
- Figmentum都市、DoD simulation、Pictor world描画、Ergo frame / inputを接続する
- Android / Windowsと同じcontent schemaとcanonical saveを使う
- Apple SDK / signing secretをsourceへhard-codeしない
- Pictorを迂回してgame domainからMetalを直接描画しない
- build / archive失敗を成功扱いしない

## Scope boundary

このtaskはiOS app build / archive経路まで。actual-device install、startup、
background / resume、performanceはKD-MOB-007へ分離する。`playable`の成立は
KD-MOB-007のiOS functional acceptance完了時とし、このtask単独では完成扱いしない。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / PRを作成して停止する
- build host、toolchain、signing有無、未実施runtime確認をPR本文へ明記する
- unit / integration / behavior / startup testは明示指示なしに実行しない
