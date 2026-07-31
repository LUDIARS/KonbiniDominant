---
task: kd-mob-000-smartphone-contract
project: KonbiniDominant
kind: 設計相談
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: null
actio_task_id: null
memory_links:
  - spec/design.md
  - spec/interface/mobile-platform.md
  - spec/setup/mobile-development.md
  - spec/feature/ui-ux.md
  - spec/plan/implementation-roadmap.md
---

# KD-MOB-000 — Smartphone platform contract

## 目的

necoの「スマホでも出来るようにしたい」という要求を、進行中のWindows
first playableを壊さない独立したplatform contractと実装順へ整理する。

## 完了条件

- Windows / Android / iOSで共有するgameplay / simulation / save境界を定義する
- Android先行、iOS後続のdelivery sequenceを定義する
- 固定Pictor / Ergo / Figmentumのmobile capabilityとgapを記録する
- surface、lifecycle、asset、touch、packageを責務別taskへ分解する
- Windows KD-FP-001 / KD-FP-002の完了条件を変更しない
- unit / integration / behavior / startup testを実行しない

## 設計結果 (2026-07-31)

- Pictor固定revisionにはAndroid / iOS surfaceとmobile lifecycleの足場がある
- Ergo固定revisionはGLFW具象型とdesktop Vulkan検出へ固定されている
- Figmentum `CityPlan`はplatform共通の都市形成正本として維持できる
- mobile hostはOS eventを正規化し、DoD simulationへsemantic commandだけを渡す
- Android / iOS実装とactual-device検証を後続taskへ分離した
- build、test、startup、service操作は未実施
