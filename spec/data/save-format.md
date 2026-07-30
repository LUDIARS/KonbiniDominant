# Save format

## 方針

mesh、Pictor ObjectId、GPU bufferを保存しない。保存の正本は
「Figmentumで同じ都市を再生成する情報」と「再生成後へ適用するgameplay delta」。

```jsonc
{
  "schemaVersion": 1,
  "gameVersion": "semver-or-commit",
  "contentVersion": "content-hash",
  "worldSeed": "uint64-as-string",
  "randomAlgorithm": "algorithm-version",
  "currentTick": 0,
  "phase": "phase1",
  "phaseStartTick": 0,
  "playerChain": "losan",
  "dimensions": [],
  "chainEconomy": [],
  "stores": [],
  "populationDeltas": [],
  "modifiers": [],
  "boss": null,
  "commandSequence": 0
}
```

## Dimension entry

```jsonc
{
  "dimensionId": 1,
  "ordinal": 0,
  "figmentumRecipeVersion": 1,
  "citySeed": "uint64-as-string",
  "manifestHash": "sha256",
  "state": "active",
  "facilityDeltas": [
    { "facilityId": 42, "destructionState": "replaced" }
  ]
}
```

load時は次の順に復元する。

1. recipe versionとseedからFigmentum `CityManifest`を再生成
2. `manifestHash`を検証
3. facility deltaをstable IDへ適用
4. store / economy / population / modifierを復元
5. derived spatial index / Triangle / RenderSnapshotを再構築

hash不一致時に座標で推測matchingして続行してはならない。migrationが無ければ
互換errorとして停止する。

## Store entry

storeはstable gameplay dataだけ保存する。

```jsonc
{
  "storeId": 1001,
  "chainId": "famoma",
  "dimensionId": 1,
  "lotId": 55,
  "verticalSlot": 3,
  "variantId": "default",
  "faith": 0,
  "quality": 1,
  "statusFlags": []
}
```

ZOC radius、revenue rate、Triangle membershipはcontentと配置から再計算する。

## 決定性

- 64bit integerはJSON number精度に依存せず10進stringで保存
- 浮動小数の派生値をsave正本にしない
- DEC-RNG-01に従い、乱数は
  `(worldSeed, randomAlgorithm, streamId, tick, stableId, ordinal)` の
  counterless hashで生成する
- stateful random counterは持たず、`randomAlgorithm`不一致は互換errorとする
- saveはtick境界でのみ作成し、`currentTick`は次に処理するtickを表す
- wall clock時刻をphase/buff残時間へ使わずtickで保存
- 配列順はstable ID昇順でcanonicalizeし、snapshot hashを再現可能にする

## Migration

- schema versionごとに純粋なforward migrationを用意
- original saveを上書きせず、新versionとして書き出す
- field欠落を無言defaultで補わない
- Figmentum recipeの破壊的変更はmanifest migrationまたは旧generator保持が必要
