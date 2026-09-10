# Phase 2 — 3次元的侵略

> v4実装: ユーザーの判断委任とPhase 2–4実装指示に基づく具体値・暫定決定は
> [full-campaign-baseline](full-campaign-baseline.md)を正本とする。
> 下記の原案に残るTBDのうち実装済み項目は同baselineで解決し、
> 追加の11種スキルは[skill-upgrades](skill-upgrades.md)で定義する。

## 目的

平面支配に垂直stackと信仰度を加え、同じ施設座標を複数chainが奪い合う。
Phase 1のruleは累積して動作する。

## 垂直配置

REQ-P2-PLACE:

- 指定施設の上へ店舗を縦にstackする
- 高階ほど建設費が高い
- 他社店舗の上にも配置できる

`WorldPosition.verticalSlot`で階を表す。slotは0起点、画面上の階数表示は
`verticalSlot + 1`。

placement validation:

- facilityの`maxVerticalSlots`内
- 対象slotが空
- BASE-P2-STACK-01: slot `n>0` は直下slot `n-1`がoccupied
- 対象facilityがDestroyedでない
- chainに費用がある

他社上配置を許すため、stack全体のownerは持たず、slotごとにstore ownerを持つ。
下層storeが破壊された場合に上層が崩れるか浮くかは `TBD-STACK-SUPPORT-01`。

## 建設費

```text
verticalBuildCost =
  baseBuildCost
  × verticalCost(verticalSlot)
  × modifiers
```

`verticalCost(n)`は単調増加する。具体式は `TBD-ECON-VERTICAL-01`。

## 信仰度

REQ-P2-FAITH-01: storeごとに信仰度を持ち、近隣storeに対する支配性で
時間とともに増減する。

baseline rangeはcontentで定義する `faithMin..faithMax`。

tick更新:

1. 同dimensionのspatial neighborを取得
2. 同chain support、rival pressure、Triangle、vertical advantageを集計
3. natural driftとmodifierを加える
4. rangeへclamp
5. threshold crossing eventを生成

支配性式、vertical advantage、自然減衰は `TBD-FAITH-*`。

## イメージ戦略

REQ-P2-IMAGE-01: playerは「イメージ戦略」で信仰度を増やせる。

commandはchainまたはstore/regionをtargetに取る。費用、cooldown、範囲、増加量は
`TBD-IMAGE-*`。UIは未設定値を隠して実行可能にせず、content validationでfailする。

## 256階

BASE-P2-256-01: 原文「256回建」は「256階建て」と解釈し、
どれか1施設の連続occupied slot数が256へ到達した時に進行条件を満たす。

Figmentumの標準階高3.2mなら約819.2m。描画は次を必要とする。

- vertical partition / chunk
- camera far planeとdepth precisionの設計
- 表示高度に応じたLOD
- UIからの階層jump

256を同時描画数の意味に変えない。owner確認は `TBD-PHASE-256-01`。

## Phase進行

次のいずれかでPhase 3へ進む。

- 256階条件
- `phase2DurationTicks`経過

既存stack、faith、ZOC、Triangle、economyはPhase 3へ持ち越す。

## 不変条件

- 同じdimension / lot / verticalSlotにstoreは最大1つ
- faithはfiniteでrange内
- 高階costは直下階より低くならない
- slot構造変更はtick境界で一括適用
- 256階判定は描画上のmesh高さではなくsimulation slot数で行う
