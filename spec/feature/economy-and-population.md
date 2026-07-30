# Economy and population

## 初期資金

REQ-ECON-01: 初期資金は選択chainのbase店舗5個分。

```text
startingCash = selectedChain.baseBuildCost × 5
```

variant選択により5店舗ちょうど建てられない場合でも、starting cashの基準は
base variantとする。

## 建設費

- Phase 1: chain / variantのbase cost
- Phase 2以降: vertical slotが高いほど増える
- 他社店舗の上へ置く場合の追加費用は `TBD-ECON-02`
- command validation成功後、spawnと同じtick境界で費用確定
- placement失敗時は費用を消費しない

高階費用式は単調増加を必須とするが、linear / exponential等は未決。

## 人口

REQ-POP-01: 街の破壊とコンビニの発展で人口が変動する。

DEC-POP-01:

- 人口はlot / block単位の`PopulationCell`として集約
- 施設破壊でcapacityとpopulationへ負のdelta
- 店舗・支配領域・都市発展でdemand / attractionへ正または負のmodifier
- 人口は0未満、capacity上限超過にならない
- 個別住民modelはvisual sampleでありgameplay正本にしない

流入・流出式、破壊時損失率、回復速度は `TBD-POP-*`。

## 顧客化

各PopulationCellは周辺店舗のinfluenceを集計し、preferred chainとloyaltyを更新する。

1. spatial indexで影響候補店舗を取得
2. 距離、ZOC、Triangle、faith、modifierからchain別scoreを算出
3. hysteresisを通してpreferred chainを変更
4. chain別customer shareとdemandを集計

同点処理はstable `ChainId`順ではなく、前tickのpreferred chain維持を優先して
不自然な振動を避ける。

## 収益

```text
storeRevenue =
  baseRevenue(variant)
  × servedDemand
  × quality
  × triangleMultiplier
  × faithMultiplier
  × dimensionEnergyMultiplier
```

各項の値と加算/乗算順はcontentで固定する。float誤差がgame resultへ影響する場合は
fixed-point化を検討し、`TBD-NUMERIC-01`で決定する。

## 資金不足

建設可能なcommandが無い状態でも即game overにはしない。既存店舗の収益tickを待てる。
全店舗消滅かつ建設資金不足の場合の救済/敗北は `TBD-LOSE-02`。
