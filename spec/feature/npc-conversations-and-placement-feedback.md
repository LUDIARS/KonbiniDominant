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
- Figmentum都市では [Pedestrian path walking](#pedestrian-path-walking) に従い、
  home entranceからstore entranceまで歩行者pathに沿って往復する
- 歩行者networkを持たない都市 (grid town) だけは、BASE-NPC-PATH-01として
  population cellの施設位置からstore位置までXZ平面の直線往復とする。
  この場合も`ResidentRouteState::DirectLine`として明示する
- Konbini側に偽の道路正本を追加しない
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

上の英字列は表示文ではなく、content `residentPresentation.remarks`が持つ
localization keyである (BASE-NPC-LOCALE-01)。native frameは
[Japanese speech lines](#japanese-speech-lines) の日本語本文を表示する。
英字5x7 bitmap glyph (`buildSpeechBubbleGeometry`) はKD-NPC-001のCPU検証用helperとして
残るが、native frameの吹き出しには使わない。

## Store placement choreography

simulation上の店舗配置、施設置換、支払は同じtickで即時確定する。animationは
その結果を変更せず、成功した`PlacementResult::placedStore`から作る一時的な
`RenderStorePlacementCue`だけをconsumeする。

現行の演出は [Store construction effects](store-construction-effects.md) を正本とする。
360度回転しながら浮上し、短く滞空してから急落する。回転の軌跡と着地の土煙は
Ergo の particle モジュールで生成し、既存の Pictor 描画へ接続する。
入力成功・支払・店舗の収益開始を、演出完了まで待たせない。

## Visia dummy

REQ-VISIA-01: 人間と着地effectはgame-owned `VisiaDefinition`で意味を定義し、
初回版は次のprimitive dummyへ解決する。

- resident: yaw回転するbody box + head box
- store landing effect: 水平annulus ring

VisiaはPictorの`Visus`を置き換える型ではない。VisiaからVisus／text／effect
resourceへ解決する責務はadapter側に置く。詳細は
[Visia presentation contract](../interface/visia-presentation.md)を正本とする。

## Pedestrian path walking

KD-NPC-003。価値: UX-KD-W1 (都市俯瞰で住民と店舗支配の関係を読める)、
UX-KD-W2 (同じseedとcommand streamから同じ表示)。
道路graphの正本はFigmentumで、契約は
[Pedestrian path contract](../interface/figmentum-city-generation.md#pedestrian-path-contract)。

経路選択 (`selectPedestrianRoute`):

- home facilityのentranceからassigned storeのfacilityのentranceまで。
  facilityはFigmentum keyで引くので、dimension複製されたfacilityも同じentranceを使う
- costはedge長を整数mmにした和。最小costのrouteを選ぶ
- 同costの候補があるnodeでは、直前edgeのstable edge keyが小さい方を採る。
  edge costは正なので探索順に依存しない
- waypointは `home entrance → node列 → store entrance`。歩行距離はwaypoint間の
  XZ距離の和 (entranceとnodeの間も含む)
- 同じfacility同士は entrance 1点・距離0

resident snapshot:

- `AtHome`はhome entrance、`AtStore`はstore entranceに立つ。yはpopulation cellの高さ
- 歩行中の位置は経過tickに比例した歩行距離で、そのsegment上を線形補間する
- yawは今いるsegmentの進行方向。帰路は逆向き。`AtHome`は最初のsegment、
  `AtStore`は最後のsegmentの向き
- `ResidentPresentation::route`は `NoTrip` / `DirectLine` / `PedestrianPath` /
  `MissingEntrance` / `Unreachable`

到達不能:

- entranceが無い (`MissingEntrance`) か、graph上で非連結 (`Unreachable`) のときは
  その状態を返し、住民はhomeの施設位置に留まり発話しない
- 直線へfallbackしない
- 人口、店舗割当、収益、canonical snapshotは変えない。path tableは
  `FirstPlayableSimulation`が保持するread-only入力で、stageもsaveもしない

## Japanese speech lines

KD-NPC-002。価値: UX-KD-W1、UX-KD-W4。

- font: Noto Sans JP (SIL OFL 1.1)。取得元はgoogle/fonts
  (`ofl/notosansjp`、revision・SHA-256は`data/fonts/NotoSansJP/source.json`)。
  `OFL.txt`をリポジトリと配布物 (`licenses/` / `notices/`) に同梱する。TTF本体は
  コミットしない
- catalog: `data/locale/ja/resident_remarks.json`がlocalization key →
  日本語本文を持つ。BASE-NPC-LOCALE-01として、keyはcontentの`remarks`文字列
  そのもの (content v2 / v3 / v4 の値とcanonical bytesを変えないため)

  | key | ja |
  |---|---|
  | `NICE AND CLOSE` | 近くて便利！ |
  | `EASY TO REACH` | すぐ行けるね |
  | `HANDY LOCATION` | いい場所だな |

- atlas: `tools/bake_speech_glyphs.py`がcatalogに現れるcode pointだけを
  wght 500で平坦化し、`tools/bake_vector_font.py`と同じnonzero台形分割で
  三角形meshにする (`src/render/generated/noto_sans_jp_speech_mesh.inc`)。
  全CJKを同梱しない。生成物はfontとcatalogのSHA-256を記録する
- glyphは共有mesh 1つずつで、bubble本文ごとのmesh / texture生成はしない
- 未知のkey、atlas外のcode point、不正UTF-8は明示的な例外。raw keyや`?`へ
  fallbackしない。`GameSession`起動時にcontentの全remarkを検証する
  (`validateSpeechLineKeys`)
- 1 bubbleは最大16 glyph。超える本文はtruncateせず例外

## Runtime resident sync

KD-NPC-002。価値: UX-KD-W1。

`FramePresenter`はcompleted tickごとに`ResidentPoseTracker::observe`し、
frameごとに`advance`する。

- 新規residentはsnapshot poseで出す
- 同じstable resident IDの`targetStore`または`route`が変わった (ZOC再割当) ときは、
  直前frameに表示したposeから新しいsnapshot poseへ
  `retargetBlendSeconds` (BASE-NPC-RETARGET-01 = 0.6秒、smoothstep、yawは最短回転)
  で補間する。瞬間移動を見せない
- 同じ目的地の歩行はsnapshot poseをそのまま使う
- snapshotから消えたresidentは同じobserveで消え、Pictor objectとmesh参照も同frameで
  解放される
- tickが巻き戻った (retry / load) ときはblendを捨ててsnapshot poseから再開する
- trackerはsimulation / snapshotへ書き戻さない

placement cue: `ConstructionPresentation`がcueから店舗pose overrideを始め、着地後の
effect期間だけ`StorePlacementSample::landingEffect`を返す。その間だけ
`LandingEffect` objectを出し、期間後は同じframeでobjectと参照を解放する。
cueを取りこぼした店舗 (64件overflow、見えないdimension等) はvisualを持たず、
snapshotのtarget poseで描かれる。

## Bubble culling

KD-NPC-002。発話中residentのhead anchor (位置 + `bubbleHeightMeters`) を
camera view-projectionで投影し、次の順で残す。

1. camera eyeとの距離が`bubbleMaxDistanceMeters`以下
2. NDCが±(1 + 0.1) 内、depthが[0, 1] (off-screen cull)
3. 近い順 (同距離はresident ID順) に最大8件 (BASE-NPC-BUBBLE-LIMIT-01)

bubbleはcamera-facing planeに置き、寸法はpixel指定 (em 18px) をcameraの
meters-per-pixelで換算するので、zoomしても画面上の大きさが変わらない。tail先端が
head anchorに一致する。anchorがhead上端以下ならcontent errorとして例外。

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
- placement sampleが360度回転・浮上・滞空・急落を表現し、着地時に土煙が出る
- 同じauthoritative stateからcanonical snapshotは機能追加前と同じ規則で生成される
- Figmentum都市のresidentはpedestrian path上を歩き、同costのrouteはstable edge keyで
  決まる。到達不能は明示状態になり、直線に落ちない
- pedestrian pathの有無でcanonical snapshot、人口、収益が変わらない
- residentはstable IDごとに1つのPictor objectを持ち、共有resident meshの
  instanced drawで描かれる。ZOC再割当では直前poseから補間する
- 吹き出しは日本語本文をsubset glyph atlasから描き、距離・off-screen・同時8件で
  cullする
- 着地effectはeffect期間だけPictor objectを持ち、終了frameで解放される
