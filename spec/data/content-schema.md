# Content schema

## 目的

chain差、経済式、phase条件、boss設定をコード分岐ではなくversion付きcontent dataで
定義する。未決の数値はplaceholder値を入れず、required keyとして未解決を検出する。

## Root

```jsonc
{
  "schemaVersion": 1,
  "simulation": {},
  "chains": [],
  "economy": {},
  "phases": {},
  "triangle": {},
  "dimensions": {},
  "boss": {}
}
```

必須key欠落時は起動時にfail-fastする。黙ってhard-coded defaultやstubへ落とさない。

## Simulation

| key | 意味 |
|---|---|
| `ticksPerSecond` | fixed tick rate |
| `spatialCellMeters` | uniform grid cell幅 |
| `populationAggregation` | `lot` または `block` |
| `randomAlgorithm` | 再現性のためのalgorithm ID |

具体値は `TBD-PERF-01` の規模・性能予算決定後に固定する。

## Chain

```jsonc
{
  "id": "losan",
  "displayName": "ローサン",
  "baseBuildCost": "required-number",
  "baseZocRadiusMeters": "required-number",
  "baseRevenueMultiplier": "required-number",
  "variants": [],
  "passives": []
}
```

確定content:

| chain | 確定特性 | 未決 |
|---|---|---|
| ローサン | 複数価格帯variantを展開。他2社より固有特性は弱い | variant別cost/収益/品質 |
| ファモマ | 怪音波。ZOCとDominant areaが広い。建設単価が高い | 半径倍率、音波演出とruleの境界 |
| セバンイレバン | 建設単価が低い。支配拡大で上げ底効果によりstat低下 | 対象stat、曲線、「7社以上」「イレバン」の意味 |

牛乳・チキン等は現時点ではflavor tagであり、商品在庫systemを追加しない。

## Economy

| key | 意味 |
|---|---|
| `startingStoreEquivalent` | 初期資金を何店舗分とするか。REQで `5` |
| `buildCostByVerticalSlot` | 高階ほど増える費用式 |
| `revenueTickInterval` | 収益確定周期 |
| `populationDemandWeight` | 人口→需要 |
| `triangleRevenueMultiplier` | Triangle内収益buff |
| `destructionRefundRule` | 破壊時返金 |

`startingCash = 5 × selectedChain.baseBuildCost` とする。
その他の式・係数は `TBD-ECON-*`。

## Phase conditions

```jsonc
{
  "phase1": {
    "durationTicks": "required-number",
    "advanceOnAllRivalsDestroyedAndCityDominated": true
  },
  "phase2": {
    "durationTicks": "required-number",
    "targetVerticalSlots": 256
  },
  "phase3": {
    "foreignDimensionsToDestroy": 2
  }
}
```

`targetVerticalSlots=256` は原文「256回建」を「256階建て」の誤記とみなす
BASE-P2-256-01。owner確定まではsource annotationを残す。

## Triangle

| key | 意味 |
|---|---|
| `maxEdgeMeters` | 「近く」の最大辺 |
| `minAreaSquareMeters` | 退化Triangle排除 |
| `boundaryRule` | 点が辺上にある場合 |
| `overlapStackRule` | 複数Triangle buff |
| `captureRule` | rival store破壊が自動かcommandか |

Delaunayは候補列挙法であり、これらgame ruleの値を決めない。

## Dimensions

| key | 意味 |
|---|---|
| `seedDerivationVersion` | ordinalからseedを作る規則 |
| `coordinateMapping` | 同位置判定方式 |
| `convenienceEnergyModifier` | 自次元buff |
| `collapseDelayTicks` | 対消滅→次元消滅まで |

## Boss

| key | 確定 / baseline |
|---|---|
| `manifestationSeconds` | REQ: 30 |
| `maxValueDamage` | REQ: 9223372036854775807 |
| `minimumSurvivingStores` | BASE: 1 |
| `winOnManifestationTimeout` | BASE: true |
| `spawnCadenceTicks` | TBD |
| `timeCutPasteRule` | TBD |

`MaxValue`値は演出上の符号付き64bit最大値として保持するが、通常HP damage式へ
無理に接続しない。effectは「対象次元を崩壊状態へする」domain commandとして定義する。

## Validation

- IDはunique
- 数値はfiniteかつ許容範囲内
- tick / cost / radius等のrequired値が未解決なら起動失敗
- modifier参照先statが存在する
- chain variantが少なくとも1つ存在する
- phase遷移先が閉じたgraphを作る
- schema version不一致は明示error
