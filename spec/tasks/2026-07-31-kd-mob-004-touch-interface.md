---
task: kd-mob-004-touch-interface
project: KonbiniDominant
kind: 実装
status: pending
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 674
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-mob-003-mobile-runtime-assets.md
  - spec/interface/mobile-platform.md
  - spec/feature/ui-ux.md
  - spec/interface/ergo-runtime.md
---

# KD-MOB-004 — Touch input and responsive HUD

## 目的

hover、mouse、keyboardに依存せず、touchから既存のsemantic action /
`PlayerCommand`を生成してfirst playableを操作できるようにする。

## 完了条件

- native contactがfinger ID、phase、position、timestampを失わず正規化される
- tap / drag / pinch / cancelを責務別gesture recognizerで判定する
- drag開始後のcontactをtapとして確定しない
- app pause / contact cancel後にstuck pointerを残さない
- UI capture済みcontactからworld commandを生成しない
- tapでfacilityを選択し、明示`Place` actionでstore配置を確定する
- dragでcamera pan、pinchでzoomできる
- hover情報をselection stateから表示できる
- HUDがsafe area、display density、UI scaleへ追従する
- orientation / surface resize後もselectionとsimulation stateを保持する
- raw touch sample countやframe rateをgame resultへ使わない

## スコープ

- game-owned touch / gesture values
- Ergo input injection adapter
- input action mapping
- mobile HUD layout / placement confirmation
- interface / feature spec

OS Activity / ViewController、renderer surface、package、実機起動は含めない。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / PRを作成して停止する
- unit / integration / behavior / startup testは明示指示なしに実行しない
