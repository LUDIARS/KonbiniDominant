# Figmentum city-generation contract

## 目的

REQ-TECH-FG-01: N-KXiと各dimensionの都市形成はFigmentumを正本とする。
KonbiniDominantが独立した別city generatorを持たない。

## Current upstream

調査対象: `LUDIARS/Figmentum@719af466c6f821fc2e518f652de162a8b9ebb3bd`

利用可能な主API:

- `fg::CityParams` → `fg::generateCity()` / `fg::buildCity()`
- `fg::BuildingParams` → `fg::generateBuilding()` / `fg::buildingBounds()`
- `fg::RoadNetwork + RoadCityParams` → `fg::generateRoadCity()`
- `SdfModel` → marching cubes `fg::Mesh`
- 単位: `1 unit = 1 m`

`gen-city`は抽象的な区画都市、`gen-roads`は道路network沿いの都市であり、
同じ用途として混同しない。

## Required game-side boundary

KonbiniDominantは次の抽象interfaceへ依存する。型名は概念契約。

```cpp
class ICityGenerator {
public:
  virtual CityManifest plan_city(const NkxiRecipe&) = 0;
  virtual FacilityGeometry build_facility(const FacilityRecipe&) = 0;
  virtual DistantChunkGeometry build_distant_chunk(const ChunkRecipe&) = 0;
};
```

具象`FigmentumCityGenerator`だけがFigmentumをincludeする。

## Input: `NkxiRecipe`

| field | 契約 |
|---|---|
| `schemaVersion` | recipe互換version |
| `seed` | deterministic 64bit seed |
| `dimensionOrdinal` | seed stream / variation |
| `boundsMeters` | gameplay area |
| `stationAnchor` | N-KXi駅。原点XZをbaseline |
| `roadProfile` | grid / road-network等のFigmentum profile |
| `densityBands` | station中心のdowntown / midtown / suburb |
| `terrainProfile` | amplitude / frequency / octaves |
| `facilityMix` | 用途categoryのweight |
| `verticalCapacityPolicy` | Phase 2のslot上限導出 |

REQ-CITY-01: 駅を都市中央へ固定してから他の道路・lot・facilityを生成する。

REQ-CITY-02: 西葛西の特徴を持つが、実在の西葛西ではない架空都市N-KXi。
具体的なfeature profileは `TBD-NKXI-PROFILE-01`。既定経路で実住所、緯度経度、
MLIT実データを読み込まない。

## Output: `CityManifest`

```text
CityManifest
  schemaVersion
  generatorRevision
  seed
  units = meters
  bounds
  station
  roads[]
  lots[]
  facilities[]
  populationCells[]
  distantChunks[]
```

### Lot

| field | 意味 |
|---|---|
| `LotId` | seedから決定的に生成するstable ID |
| `dimensionLocalKey` | cross-dimension対応用key |
| `center / bounds` | meters |
| `placementFlags` | buildable / protected / station等 |
| `verticalCapacity` | slot数 |

### Facility

| field | 意味 |
|---|---|
| `FacilityId`, `LotId` | stable identity |
| `kind` | Station / Residence / Commercial / Office / Utility / Other |
| `recipe` | Figmentum building kind / roof / dimensions / seed |
| `bounds` | selection / culling |
| `populationCapacity` | game側PopulationCell初期値 |
| `isDestructible` | placementで置換可能か |

IDはSDF term indexやmesh vertex indexから作らない。FigmentumのLOD並べ替えで
term順が変わってもstableでなければならない。

## Interactive / distant split

現行`buildCity()`は都市全体を1つの`SdfModel`とimportance列で返し、
建物IDやterm rangeを返さない。LOD sort後は複数棟のtermが交互になる。

したがって:

- **interactive facility**: manifestの施設ごとに`generateBuilding()`相当で生成
- **station**: protectedな個別recipe
- **road / ground**: spatial chunk単位
- **distant city**: 操作対象外に限り`buildCity()`の一括meshを許可

破壊可能施設を1都市1meshへ結合しない。

## Upstream gap: semantic planning

現行Figmentumには`CityManifest`、station anchor、facility stable IDを返す公開APIが無い。
REQ-CITY-GAP-01として、実装前にFigmentumへsemantic city-plan APIを追加する。

許容される解決:

1. Figmentumに`planCity()` / `CityPlan`を追加し、本作adapterが変換する
2. Figmentum repositoryが所有する再利用可能plan moduleを公開する

不許容:

- KonbiniDominant側へplacement algorithmを複製し、Figmentumは単棟meshだけにする
- SDF termを後解析して施設を推測する
- random meshの座標順にIDを付ける

このgap解消は別repository変更になるため、実装roadmapで独立gateとする。

## Geometry generation

```text
FacilityRecipe
  → Figmentum SdfModel
  → bounds + polygonize(resolution / LOD)
  → Mesh(position, index, vertexColor)
  → normal generation
  → Pictor asset conversion
```

制約:

- polygonizeは `(resolution+1)^3` gridを同期評価するためper-frame禁止
- startup / background job / disk cacheで生成
- cache keyはgenerator revision + recipe hash + LOD + vertex format
- `fg::Mesh`にnormalが無いためadapterで決定的に生成
- failed geometryをcubeへsilent fallbackしない。errorを返してworld loadを止める
- `BuildingParams`のyaw制約を確認し、表現できない向きはrecipeに嘘の値を持たせない

## N-KXi baseline

BASE-CITY-01:

- station anchor: `(0, 0, 0)`
- station周辺をdowntown、その外をmidtown / suburb
- Figmentumの実寸metricを維持
- gameplay lotはstationを除き置換可能
- road / terrainは都市seedに従い決定的

block数、都市半径、人口、facility mixは `TBD-NKXI-*`。

## Dimension生成

Phase 3 / Bossのdimensionも同じcontractを使う。

- ordinalを変えてseedを派生
- `dimensionLocalKey`の対応規則をversion管理
- 対消滅に必要なlot対応をmanifestへ明示
- Destroyed dimensionのgeometry cacheは参照解放後にevict可能
- saveはmeshでなくrecipeとdeltaを保存

## Error contract

`plan_city` / build APIは次を区別して失敗する。

- invalid recipe
- unsupported generator revision
- non-finite / out-of-range dimensions
- station anchor collision
- stable ID collision
- polygonize failure / empty mesh
- cache corruption

空manifestやplaceholderで続行しない。
