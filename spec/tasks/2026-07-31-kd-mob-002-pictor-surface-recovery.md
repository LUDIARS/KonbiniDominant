---
task: kd-mob-002-pictor-surface-recovery
project: KonbiniDominant
kind: 実装
status: pending
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 672
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-mob-000-smartphone-contract.md
  - spec/interface/mobile-platform.md
  - spec/interface/pictor-rendering.md
  - spec/setup/mobile-development.md
---

# KD-MOB-002 — Pictor mobile recovery integration

## 目的

型付きmobile surface / device recovery contractを持つreview済みPictor revisionを
KonbiniDominantへ固定し、Android / iOSでsimulationを失わず描画を復旧できる
consumer境界へ接続する。

## Upstream prerequisite

Pictor側の実装は、着手時に`LUDIARS/Pictor/spec/tasks/`へtask-workflow 2.1形式で
保存し、Pictor専用branch / PRで行う。必要なupstream contract:

- acquire / present結果が少なくとも`Ready`、`RecreateSwapchain`、
  `SurfaceLost`、`DeviceLost`を区別する
- Android native window消失中はsurface / swapchain作成を試みない
- iOS MoltenVK portability extension / featureをcapability検査する
- required extension欠落を通常resizeやframe skipへ畳まない
- pause / suspend / surface lost中はGPU submissionを抑止する
- surface復帰時のswapchain依存resource再構築順をpublic contractへ記録する
- Android / iOS providerのownershipを変更せず、host所有を維持する
- desktop surface pathを同じtyped resultへ移行する

## 完了条件

- upstream Pictor PRがreview済みでmergeされている
- merge revisionをexactかつclean source検証付きで固定する
- typed resultをapp lifecycle / render resource ownerへ漏れなくmappingする
- `SurfaceLost`でsimulation stateを保持しGPU submissionを止める
- `RecreateSwapchain`でreplacement-before-retire順を守る
- `DeviceLost`を通常resizeとしてsilent retryしない
- dependency revisionとconsumer contractをKonbini specへ反映する

## スコープ

- KonbiniDominant dependency pin / exact-source検証
- KonbiniDominant Pictor lifecycle adapter
- KonbiniDominant CMake / interface spec

Pictor repositoryの編集はこのtaskへ含めない。Konbini固有world draw、touch、
APK / iOS appは後続taskへ分離する。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / KonbiniDominant PRを作成して停止する
- upstream未mergeならconsumer側へ仮実装せずblockedとして報告する
- unit / integration / behavior / startup testは明示指示なしに実行しない
