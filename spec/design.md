# 全体設計

## 1. 目的と重視点

ゲームの存在意義は「ゲームが現実のルールを壊して侵略してくる」感覚を、
コンビニの平面支配、垂直スタック、多次元対消滅、高次元存在からの逃走へ
段階的にエスカレートさせることにある。

設計では次を優先する。

1. 同じ seed と command stream から同じ結果を再現できること
2. 店舗、人口セル、影響領域が増えても data scan を一括処理できること
3. game rule と描画・都市メッシュ生成を分離すること
4. Phase が次元を増やしても、同じ rule/system をデータ追加で再利用できること
5. 原案の未決事項を実装上の偶然で確定しないこと
6. Windows / Android / iOSで同じsimulation、content、save契約を共有すること

## 2. 採用スタック

| 領域 | 採用 | 非採用 / 補足 |
|---|---|---|
| 言語 / build | C++20 / CMake | Unity / C# は使わない |
| 描画 | Pictor | Pictor の下で Vulkan を game domain が直叩きしない |
| 共通 runtime | Ergo | mass entity storage として `ergo_actor` は使わない |
| 都市形成 | Figmentum | 別の独立 city generator を正本にしない |
| simulation | KonbiniDominant 固有 DoD core | ECS 製品の採用自体は要件にしない |
| platform | Windows desktop、Android、iOS | platform host以外へOS APIを漏らさない |

実装で固定する参照点は次の commit。

- Pictor `c088e8d1b7b9e2625b7a8d923c89d4d684566c16`
- Ergo `771b027f0e5492015b27f54c3bab1fd5c1ae4790`
- Figmentum `3ee998f487d984f54003c4ec3c4f7ba00b53eec3`
- AIFormat `0cb32320e496c85576c5687786835127bd4c8609`

dependency は configure 時に exact revision を検証し、API drift をfail-fastする。

## 3. レイヤ

```text
App / Screen Flow
  ├─ Input Mapping (Ergo adapter)
  ├─ Game Commands
  └─ Fixed-step Game Simulation
       ├─ Dense DoD Tables
       ├─ Spatial Index
       ├─ Systems
       ├─ Deferred Structural Commands
       └─ Domain Events
            ├─ City Projection (Figmentum adapter)
            ├─ Read-only RenderSnapshot (Pictor adapter)
            ├─ HUD/View Model (Ergo UI adapter)
            └─ Save Snapshot
```

依存は上から下へ一方向とする。adapter は game domain interface を実装するが、
domain は adapter の具象型を知らない。

## 4. DOTS から DoD への読み替え

| Unity DOTS の概念 | 本作の設計 |
|---|---|
| Entity | 世代付き整数 `EntityId { index, generation }` |
| Component | 責務別の並列配列 / dense table |
| Archetype chunk | `StoreTable`、`PopulationCellTable`、`ModifierTable` 等 |
| System | dense range を処理する stateless system pass |
| EntityCommandBuffer | tick 中の配置・破壊・生成を遅延反映する `StructuralCommandBuffer` |
| Jobs / Burst | まず単純な contiguous loop。計測後に範囲分割 / SIMD |
| Blob / baking | Figmentum seed + recipe + immutable mesh cache |
| Transform sync | dirty ID の simulation transform を render snapshot へ射影 |

`std::vector<std::unique_ptr<Entity>>` のような entity ごとの所有モデルは hot path に
採用しない。公開境界では class/interface を使ってよいが、内部 storage layout を
consumer に漏らさない。

## 5. Tick と決定性

simulation は fixed tick で動かし、同一 tick 内の順序を固定する。

1. `PlayerCommand` / `AiCommand` の受理と正規化
2. 配置 validation、費用予約
3. structural command の適用
4. spatial index 更新
5. ZOC / Dominant Triangle 更新
6. 包囲・破壊・次元消滅の解決
7. 人口 / 顧客配分
8. 収益 / 費用 / modifier 更新
9. 信仰度 / 便利・不便エネルギー更新
10. phase / boss / result 遷移
11. domain event と render/save snapshot の生成

DEC-RNG-01: 乱数はstatefulな消費位置を持たず、
`(WorldSeed, RandomAlgorithmId, StreamId, Tick, StableId, Ordinal)` を入力する
counterless hashから生成する。同じtick / entityで複数値が必要な場合の
`Ordinal` はsystem仕様で固定する。wall clock、描画 frame rate、container
iteration orderをゲーム結果に使わない。

## 6. 都市と simulation の分離

Figmentum は `1 unit = 1 m` の都市 recipe と形状を生成する。
KonbiniDominant は stable `FacilityId`、所有、破壊、人口、縦スロット、次元座標を持つ。

- save の正本は mesh ではなく seed / recipe version / gameplay delta
- 破壊可能施設は施設単位で生成し、1都市1巨大meshに結合しない
- 操作対象外の遠景だけは Figmentum の都市一括生成を利用できる
- Figmentum の同期 polygonize は frame loop で実行しない

詳細は [interface/figmentum-city-generation.md](interface/figmentum-city-generation.md)。

## 7. Rendering 境界

Pictor は `RenderSnapshot` を読み、static / dynamic / GPU-driven pool へ射影する。
simulation table を直接変更しない。

現行 Pictor の mesh upload と host-driven draw には未実装・host責務が残るため、
本作側に `GpuAssetStore`、`IBatchGpuSource` 実装、`PictorFrameBridge` が必要。
`register_mesh_data()` だけで描画可能とはみなさない。

詳細は [interface/pictor-rendering.md](interface/pictor-rendering.md)。

## 8. Ergo 境界

Ergo は入力イベント、frame clock、render host、UI/audio等の共通機能を提供する。

- OS入力は現行 `ergo_input::poll()` が no-op のため GLFW callback adapter が必要
- `ergo_scene` は look-dev document であり runtime ECS ではない
- `ergo_actor` は開発用 tree identity に限定し、店舗・住民へ1個ずつ使わない
- `ergo_blackboard` は低頻度の global/UI state に限定する
- game-specific editor は host repo plugin pack として置く

詳細は [interface/ergo-runtime.md](interface/ergo-runtime.md)。

## 9. Smartphone platform 境界

REQ-PLATFORM-01: Windows版に加えてスマートフォンでも同じgameplayを
プレイ可能にする。Androidを最初のmobile実装対象とし、共通境界を確立した後に
iOSへ接続する。現行Windows first playableの完了条件へmobile packagingや実機確認を
混ぜず、後続taskとして進める。

mobile差分はsurface、input、app lifecycle、asset / writable path、packageに閉じ込める。
`konbini_sim`、Figmentum `CityPlan`、content schema、canonical save / replayは
platform間で共通とする。描画品質のcapability別変更は許容するが、game ruleや
canonical stateを変えない。

詳細は [interface/mobile-platform.md](interface/mobile-platform.md)。

## 10. 技術メモ中のアルゴリズム

| 候補 | 扱い |
|---|---|
| GPU Instancing | Unity APIではなく、Pictor batch / instance経路として性能計測後に適用 |
| WFC | Figmentum内部の都市制約生成が必要になった場合だけ採否を決める |
| Voronoi | ZOC表示・影響分割の候補。game ruleそのものとは分離 |
| Delaunay | 同一chainの近接3店舗からTriangle候補を得る baseline |
| 空間充填 | 目的が未定義のため採用保留 |
| minimax | 相手AIのaction/state/evaluationが定義されるまで採用保留 |

## 11. 非目標

- 現実のOS、位置情報、外部店舗データを侵略する機能
- 実在の西葛西をそのまま再現すること
- Unity互換APIやUnity asset pipelineの再現
- individual resident AI の先行実装
- 仕様未決の数値をコードの既定値で事実上固定すること
