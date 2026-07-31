---
task: kd-mob-003-mobile-runtime-assets
project: KonbiniDominant
kind: 実装
status: pending
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 673
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-mob-001-ergo-platform-render-contract.md
  - spec/tasks/2026-07-31-kd-mob-002-pictor-surface-recovery.md
  - spec/interface/mobile-platform.md
  - spec/setup/mobile-development.md
  - spec/data/save-format.md
---

# KD-MOB-003 — Mobile runtime and asset boundary

## 目的

KonbiniDominantへOS非依存のlifecycle、display、asset、writable path境界を追加し、
desktop / Android / iOS hostが同じapp ownerへeventを渡せるようにする。

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
