# Figmentum city-generation contract

## 目的

REQ-TECH-FG-01: N-KXiと各dimensionの都市形成はFigmentumを正本とする。
KonbiniDominantが独立した別city generatorを持たない。

## Current upstream

利用revision:
`LUDIARS/Figmentum@d0437cd5cbf8721faec5267cc1e4dd2ce55d6fd0`
(Figmentum #2241、pedestrian path network を追加した main merge)

利用可能な主API:

- `fg::CityPlanParams + seed` → `fg::planCity()`
- `fg::CityPlan` → station anchor、stable facility identity、building recipe
- `fg::CityParams` → `fg::generateCity()` / `fg::buildCity()`
- `fg::BuildingParams` → `fg::generateBuilding()` / `fg::buildingBounds()`
- `fg::RoadNetwork + RoadCityParams` → `fg::generateRoadCity()`
- `SdfModel` → marching cubes `fg::Mesh`
- `fg::CityPlanParams + CityPlan` → `fg::planPedestrianNetwork()`
  (歩行者 node / edge / facility entrance。仕様は Figmentum
  `spec/feature/pedestrian-path.md`)
- 単位: `1 unit = 1 m`

`gen-city`は抽象的な区画都市、`gen-roads`は道路network沿いの都市であり、
同じ用途として混同しない。

## Required game-side boundary

first playable の KonbiniDominant は次の game-owned 抽象 interface へ依存する。
この境界に Figmentum / Pictor の型を公開しない。

```cpp
class ICityGenerator {
public:
  virtual CityManifest planFirstPlayableCity(
      GenerationalIdPool<FacilityId>& facilityIds) const = 0;
  virtual shared_ptr<const FacilityGeometry> buildFacility(
      const ManifestFacility& facility) const = 0;
  virtual GeneratedCity generateFirstPlayableCity(
      GenerationalIdPool<FacilityId>& facilityIds) const = 0;
};
```

具象 `FigmentumCityAdapter` と、その private projection / meshing units だけが
Figmentumをincludeする。将来の `NkxiRecipe`、distant chunk、dimension ordinal
はこの first-playable interface を暗黙に拡張せず、version付き契約として追加する。
world load composition は atomic な `generateFirstPlayableCity()` を使う。plan単体APIは
geometryを生成しない用途に限り、全geometry生成失敗時のID rollback契約は持たない。

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

## Semantic planning dependency

REQ-CITY-GAP-01 は Figmentum
`3ee998f487d984f54003c4ec3c4f7ba00b53eec3` の `planCity()` /
`CityPlan` で解消した (現在の pin は上記 Current upstream)。KonbiniDominant はこの exact revision を固定し、
`FigmentumCityAdapter` が plan を game-owned `CityManifest` へ変換する。

引き続き次は禁止する。

- KonbiniDominant側へplacement algorithmを複製し、Figmentumは単棟meshだけにする
- SDF termを後解析して施設を推測する
- random meshの座標順にIDを付ける

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

## Pedestrian path contract

KD-NPC-003。Figmentum `planPedestrianNetwork(params, plan)` の
`PedestrianNetwork` (schema 1 / recipe 1、CityPlan schema 1 / recipe 1) を
`CityManifest::pedestrianPaths` (`PedestrianPathContract`) へ投影する。
道路 graph の正本は Figmentum で、KD は値を写すだけで独自の道路 graph を持たない。

| 層 | 責務 |
|---|---|
| `adapters/figmentum` | network を導出し、version / seed / city version を plan と突き合わせて contract へ変換。`PedestrianPathError` は code 名付き `std::runtime_error` にして world load を止める |
| `city` | `validatePedestrianPathContract` (version、key 昇順・重複、edge endpoint / entrance node の存在、finite、正の edge 長、manifest facility と entrance の 1 対 1)。`projectPedestrianPathTable` で sim の read-only table へ投影 |
| `sim` | `PedestrianPathTable` (flat array + CSR) と `selectPedestrianRoute`。Figmentum 型に依存しない |

- Figmentum 由来の manifest は contract を必須とする。歩行者 network を持たない
  grid town は contract 無し (`projectPedestrianPathTable` は nullopt)
- contract は canonical bytes に含める (`kCityManifestCanonicalVersion = 2`)。
  network が変われば別都市として hash が変わる
- node / edge key は Figmentum の stable key をそのまま使い、bit layout を KD で
  解釈しない。昇順 = canonical 順だけを前提にする
- 連結性は contract 検証で要求しない。到達不能は経路選択が明示状態で返す
  ([Pedestrian path walking](../feature/npc-conversations-and-placement-feedback.md#pedestrian-path-walking))
- 空 network や施設間直線へ黙って落とさない

## Error contract

`planFirstPlayableCity()` / `generateFirstPlayableCity()` /
`buildFacility()` は次を区別して失敗する。

- invalid recipe
- unsupported generator revision
- non-finite / out-of-range dimensions
- station anchor collision
- stable ID collision
- polygonize failure / empty mesh
- cache corruption
- pedestrian network の version 不一致・plan との食い違い・Figmentum
  `PedestrianPathErrorCode` (InvalidParams / UnsupportedVersion / PlanMismatch /
  InvalidKey / NonCanonicalOrder / NonFiniteCoordinate / DegenerateEdge /
  UnknownNode / Unreachable)

空manifestやplaceholderで続行しない。
