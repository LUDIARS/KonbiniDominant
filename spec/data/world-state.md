# World state

## 目的

店舗、施設、人口、支配領域、次元が増加しても、同種データを連続走査できる
runtime modelを定義する。これはsimulationの正本であり、Pictor object、
Vulkan handle、Figmentum meshを含めない。

## ID

全参照は世代付き整数IDを使う。

```cpp
struct EntityId {
  uint32_t index;
  uint32_t generation;
};
```

用途ごとに型安全なwrapperを持つ。

| ID | 対象 |
|---|---|
| `DimensionId` | 1つの生成世界 |
| `FacilityId` | Figmentum manifest上の施設・建物 |
| `LotId` | 配置可能敷地 |
| `StoreId` | プレイヤーまたはAIの店舗 |
| `ChainId` | ローサン / ファモマ / セバンイレバン / boss勢力 |
| `PopulationCellId` | 集約人口セル |
| `TriangleId` | 有効なDominant Triangle |
| `ModifierId` | 時限buff/debuff |

削除後に同じindexを再利用する場合はgenerationを増やし、古い参照を無効化する。

## 座標

```text
WorldPosition = { dimension, xMeters, zMeters, verticalSlot }
```

- Figmentum / Pictorの物理座標は `1 unit = 1 m`
- simulation上の次元は `DimensionId` で分離
- `verticalSlot=0` は施設基部、1以上はPhase 2の店舗stack
- 描画時のYは `facilityBaseY + verticalSlot * floorHeight`
- Phase 3の「同じ位置」は同じ `LotId` または同じ量子化XZセル +
  `verticalSlot` として判定する

同一位置の最終定義は `TBD-DIM-COORD-01`。別次元の都市planを同型にするか、
cross-dimension anchor mapを持つかを実装前に確定する。

## Dense tables

### `GameState`

低頻度のglobal state。

| field | 意味 |
|---|---|
| `tick` | fixed tick通算 |
| `phase` | Boot / Title / ChainSelect / Phase1 / Phase2 / Phase3 / Boss / Result |
| `phaseStartTick` | 現phaseの開始tick |
| `playerChain` | 選択chain |
| `worldSeed` | 全乱数streamのroot |
| `bossManifestationEndTick` | bossの30秒window終端 |
| `result` | None / Win / Lose |

### `DimensionTable`

| stream | 意味 |
|---|---|
| `id[]` | stable ID |
| `seed[]` | Figmentum生成seed |
| `recipeVersion[]` | CityManifest互換version |
| `state[]` | Active / Collapsing / Destroyed |
| `convenienceEnergy[]` | 自次元buff source |
| `inconvenienceEnergy[]` | 崩壊source |
| `isPlayerOrigin[]` | player開始次元 |

### `FacilityTable`

Figmentum `CityManifest`から初期化するimmutable寄りのtable。

| stream | 意味 |
|---|---|
| `id[]`, `dimensionId[]`, `lotId[]` | identity |
| `xMeters[]`, `baseYMeters[]`, `zMeters[]` | 位置 |
| `footprintX[]`, `footprintZ[]`, `heightMeters[]` | 形状bound |
| `kind[]` | Station / Residence / Commercial / Office / Utility / Other |
| `maxVerticalSlots[]` | stack可能数 |
| `populationCellId[]` | 集約人口との対応 |
| `generationRecipeKey[]` | Figmentum recipe |
| `destructionState[]` | Intact / Replaced / Destroyed |

### `StoreTable`

simulation hot pathの中心。各streamは同じdense indexを共有する。

| stream | 意味 |
|---|---|
| `id[]`, `chainId[]`, `dimensionId[]`, `lotId[]` | identity / ownership |
| `verticalSlot[]` | stack階 |
| `variantId[]` | chain内variant |
| `statusFlags[]` | Active / Encircled / AntiStore / Boss 等 |
| `zocRadius[]` | content設定とmodifier反映後の半径 |
| `faith[]` | Phase 2以降の信仰度 |
| `revenueRate[]` | 次tick収益計算用 |
| `quality[]` | セバンイレバン等の劣化対象 |
| `dirtyFlags[]` | spatial / render / economy更新 |

削除は走査中にswap-popせず、`StructuralCommandBuffer`へ積みtick終端で
全streamに同じ操作を適用する。ID→dense indexのsparse lookupも同時更新する。

### `ChainEconomyTable`

| stream | 意味 |
|---|---|
| `chainId[]` | chain |
| `cash[]` | 建設可能資金 |
| `storeCount[]` | active店舗数 |
| `customerShare[]` | 集約顧客数 |
| `dominanceScore[]` | phase/balance用支配指標 |
| `incomeThisTick[]`, `expenseThisTick[]` | 計測可能な内訳 |

### `PopulationCellTable`

DEC-DATA-01: 人口、需要、収益の正本は施設/街区単位で集約する。

| stream | 意味 |
|---|---|
| `id[]`, `dimensionId[]`, `lotId[]` | identity |
| `population[]` | 現人口 |
| `capacity[]` | 施設破壊前の収容力 |
| `preferredChain[]` | 現在最も強い影響 |
| `loyalty[]` | influenceのヒステリシス |
| `demand[]` | 収益へ渡す需要 |

個別住民の描画は`PopulationCellTable`、割当店舗、completed tickから決定的に派生する
presentation sampleであり、simulation entityにしない。sampleの歩行phase、発話、
Visia IDはcanonical snapshot / saveへ保存せず、人口・収益へ書き戻さない。
将来、個人行動がgame ruleに必要になった時だけ `CitizenTable` を追加する。

### Presentation-derived records

`RenderSnapshot`はauthoritative tableの参照を保持せず、次の一時recordを値で公開できる。

| record | source | lifetime |
|---|---|---|
| `ResidentPresentation` | population cell + assigned store + tick + counter RNG | snapshot |
| `RenderStorePlacementCue` | 成功した`PlacementResult::placedStore` | 成功tickのsnapshot |

placement cueは店舗配置eventの再通知であり、店舗の存在やpositionの正本ではない。

### `DominantTriangleTable`

| stream | 意味 |
|---|---|
| `id[]`, `dimensionId[]`, `chainId[]` | identity |
| `storeA[]`, `storeB[]`, `storeC[]` | 頂点店舗 |
| `area[]` | 退化判定 / score |
| `capturedPopulation[]` | 収益buff入力 |
| `statusFlags[]` | Active / Contested / Invalidated |

Triangleはderived stateでありsaveに必須ではない。同じstore配置から決定的に再構築する。

### `ModifierTable`

| stream | 意味 |
|---|---|
| `id[]`, `sourceKind[]`, `sourceId[]` | 発生源 |
| `targetKind[]`, `targetId[]` | 対象 |
| `stat[]`, `operation[]`, `value[]` | Add / Multiply / Override |
| `startTick[]`, `endTick[]` | fixed tick期間 |
| `stackRule[]` | Replace / Add / Max |

イメージ戦略、便利エネルギー、上げ底効果等を同じmodifier経路へ載せる。

## Spatial index

位置近傍処理は全件二重loopにしない。

- dimensionごとのuniform XZ gridをbaselineとする
- `LotId` / `StoreId` / `PopulationCellId`の別indexを持つ
- store placement / destruction後にdirty cellだけ更新する
- ZOC neighbor query、Triangle候補、population influence、AI候補生成が共有する
- 256階stackはXZ cell内のvertical slot rangeで索引する

Voronoi / Delaunayはindexの代替ではなく、indexから得た候補集合に対して使う。

## Commands

外部入力はdataとしてsimulationへ渡す。

```text
SelectChain
PlaceStore { chain, dimension, lot, verticalSlot, variant }
RunImageStrategy { chain, targetStoreOrRegion }
OpenDimension { sourceDimension, targetOrdinal }
InvertStore { sourceStore }
MoveDimensionView { dimension }
```

commandにはtarget tickとsequence numberを持たせる。同tick内は
`player/AI priority → sequence`で安定sortする。invalid commandは理由付きeventを返し、
資金やworld stateを部分変更しない。

## Structural commands

```text
SpawnStore
DestroyStore
ReplaceFacility
CreateDimension
DestroyDimension
CreateModifier
RemoveModifier
TransitionPhase
```

system scan中はqueueへ積み、tickの既定境界で一括適用する。

## Domain events

presentation、audio、telemetryは次のeventを購読する。

```text
StorePlaced / StoreRejected / StoreDestroyed
TriangleFormed / TriangleBroken / RegionDominated
PopulationChanged / RevenueCollected / FaithChanged
PhaseChanged / DimensionOpened / AntiStoreCreated
AnnihilationStarted / DimensionCollapsed
BossAppeared / MaxValueTriggered / GameEnded
```

eventはsimulation結果であり、event consumerが結果を逆変更してはならない。
