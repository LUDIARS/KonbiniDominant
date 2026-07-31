# NPC conversations and placement feedback

## 目的

REQ-NPC-01: 街の住民が店舗へ歩き、来店中に店舗について短く発話することで、
人口と店舗支配の関係を都市俯瞰から読み取れるようにする。

REQ-PLACE-FEEDBACK-01: 店舗配置の成功を、上空からの回転着地と着地effectで
即座に識別できるようにする。

この機能はpresentation feedbackであり、人口、収益、店舗配置成否を変更しない。

## Ambient resident baseline

first playableでは`PopulationCellTable`の各cellから1体の表示用sampleを派生する。
sampleは個別住民のsimulation entityではなく、同じworld seed、completed tick、
population cell、店舗割当から毎回同じ結果を得る非権威データである。

住民は次のcycleを繰り返す。

```text
AtHome
  -> WalkingToStore
  -> AtStore
  -> WalkingHome
  -> AtHome
```

- 行先はZOC systemが`PopulationCellRow::assignedStore`へ確定したactive store
- 未割当cellのsampleは自宅位置に留まり、発話しない
- 出発地はpopulation cellの施設位置、目的地はstore位置
- 現行Figmentum `CityPlan`には道路／歩道anchorが無いため、BASE-NPC-PATH-01は
  XZ平面の直線往復とする
- 道路沿い歩行はFigmentum側でsemantic pedestrian pathをversion付きで公開した後に
  別taskで置き換える。Konbini側に偽の道路正本を追加しない
- 配置済み店舗と同位置のcellなど距離0でも停止・来店状態を正しく扱う

BASE-NPC-CONTENT-01は`contentVersion = 2`の
`residentPresentation`を正本とする。

| key | baseline |
|---|---:|
| `samplesPerPopulationCell` | 1 |
| `walkingSpeedMetersPerSecond` | 1.5 |
| `homeDwellTicks` | 30 |
| `storeDwellTicks` | 40 |
| `speechDurationTicks` | 30 |
| `bubbleHeightMeters` | 2.2 |
| `bubbleMaxDistanceMeters` | 220 |

## Conversations

現時点のauthoritative modelには商品品質、接客、価格満足度の評価値が無い。
存在しない評価を捏造しないため、first playableの発話は「ZOC内の近い店舗へ
割り当てられている」という既存事実に基づく次の英字dummy lineに限定する。

- `NICE AND CLOSE`
- `EASY TO REACH`
- `HANDY LOCATION`

lineはresidentのstable keyから決定的に選ぶ。来店直後の
`speechDurationTicks`だけ頭上に小型吹き出しを出し、camera eyeから
`bubbleMaxDistanceMeters`を超えた時は生成しない。

英字5x7 bitmap glyphはasset未接続期間の明示的なdummyである。日本語会話へ
置き換える時は、ライセンス確認済みfont、文字atlas、localization keyを
Pictor adapterへ追加し、本文ごとのtexture生成は行わない。

## Store placement choreography

simulation上の店舗配置、施設置換、支払は同じtickで即時確定する。animationは
その結果を変更せず、成功した`PlacementResult::placedStore`から作る一時的な
`RenderStorePlacementCue`だけをconsumeする。

BASE-PLACE-ANIM-01:

| parameter | baseline |
|---|---:|
| 上空で溜める時間 | 0.20秒 |
| 落下時間 | 0.65秒 |
| 落下easing | `EaseInCubic` |
| 着地effect時間 | 0.40秒 |
| 出現高度 | target + 18m |
| Y軸総回転 | 270度 |
| first playable最終yaw | 0度 |

- hold中は上空で待機し、fall中は明示したeasing curveでtargetへ着地する
- fall完了時には必ずtarget位置、270度回転済みの最終yawになる
- effectは着地時から拡大・fadeするring primitive
- cueを保持できなかったrendererは最終店舗poseを表示してよいが、配置済み店舗を
  消したりsimulationを巻き戻してはならない
- camera shake、flash、強いparticleはaccessibility設定が定義されるまで追加しない

## Visia dummy

REQ-VISIA-01: 人間と着地effectはgame-owned `VisiaDefinition`で意味を定義し、
初回版は次のprimitive dummyへ解決する。

- resident: yaw回転するbody box + head box
- store landing effect: 水平annulus ring

VisiaはPictorの`Visus`を置き換える型ではない。VisiaからVisus／text／effect
resourceへ解決する責務はadapter側に置く。詳細は
[Visia presentation contract](../interface/visia-presentation.md)を正本とする。

## Determinism and ownership

- scheduleとremarkは専用counter RNG streamを使い、stateful RNGを使わない
- wall clockは店舗placement animationだけのpresentation時間に使える
- residentのgameplay phaseと位置はinteger completed tickから派生する
- resident sample、speech、placement cue、effectはcanonical snapshotとsaveへ入れない
- presentation consumerはsimulation tableへ書き戻さない
- ZOC再割当時は同じstable resident IDの目的地がsnapshot境界で変わり得る。
  KD-NPC-002のruntime object syncが直前poseから補間し、popを隠す

## Acceptance

- assigned residentがhome、store、homeを決定的に往復する
- store滞在中だけ本文付きspeech requestを公開する
- camera eyeから220mを超える吹き出しgeometryを生成しない
- residentと着地effectがVisia primitive geometryとして生成できる
- placement sampleが0.20秒hold、0.65秒fall、270度回転、着地ringを表現する
- 同じauthoritative stateからcanonical snapshotは機能追加前と同じ規則で生成される
