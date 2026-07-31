---
task: kd-mob-001-ergo-platform-render-contract
project: KonbiniDominant
kind: 実装
status: pending
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 671
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-mob-000-smartphone-contract.md
  - spec/interface/mobile-platform.md
  - spec/interface/ergo-runtime.md
  - spec/setup/mobile-development.md
---

# KD-MOB-001 — Ergo mobile dependency integration

## 目的

platform-neutral render contractを持つreview済みErgo revisionを
KonbiniDominantへ固定し、Windows / Android / iOS hostが同じconsumer境界を
利用できるようにする。

## Upstream prerequisite

Ergo側の実装は、着手時に`LUDIARS/Ergo/spec/tasks/`へtask-workflow 2.1形式で
保存し、Ergo専用branch / PRで行う。必要なupstream contract:

- `RenderContext::surface`が`pictor::ISurfaceProvider*`をborrowする
- Ergo公開render headerが`GlfwSurfaceProvider`具象型を要求しない
- desktopでは既存GLFW providerを同じinterface経由で利用する
- Android / iOSでPictor targetが提供するruntime Vulkan contractを認識する
- mobile configure時にdesktop `Vulkan::Vulkan`不在だけでrenderを無効化しない
- real render不可をVulkan-free成功へsilent fallbackしない
- FrameComposer / layerの所有・shutdown順を変更しない
- cross-platform build契約をErgo specへ記録する

## 完了条件

- upstream Ergo PRがreview済みでmergeされている
- merge revisionをexactかつclean source検証付きで固定する
- Konbiniのdesktop surface adapterが`ISurfaceProvider`境界でbuild可能
- Android / iOS configureがreal Ergo render pathを選択できる
- required render capability不在をheadless成功へsilent fallbackしない
- dependency revisionとconsumer contractをKonbini specへ反映する

## スコープ

- KonbiniDominant dependency pin / exact-source検証
- KonbiniDominant Ergo consumer adapter
- KonbiniDominant CMake / interface spec

Ergo repositoryの編集はこのtaskへ含めない。KonbiniDominantへErgo sourceを
copyしない。touch、native host、app packagingは後続taskへ分離する。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / KonbiniDominant PRを作成して停止する
- upstream未mergeならconsumer側へ仮実装せずblockedとして報告する
- unit / integration / behavior / startup testは明示指示なしに実行しない
