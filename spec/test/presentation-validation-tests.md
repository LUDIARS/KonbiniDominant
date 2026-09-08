# Presentation validation tests

resident presentation projection と Visia presentation
(`spec/interface/visia-presentation.md`、
`spec/feature/npc-conversations-and-placement-feedback.md`) の境界値・決定性を
固定する登録test。`verification-strategy.md` の
「1. Data / ID unit tests」「2. Deterministic simulation」に属する。

## 実行状態

このtestを追加したsessionはユーザからtest実行許可を得ていないため、
build / ctest / 起動をいずれも実行していない。**未実行のtestをgreenと
報告しない** (`verification-strategy.md` 原則) ので、初回実行は許可のある
sessionが行い、結果をtaskへ記録する。

## test binary

test frameworkはlinkしない。各fileが自分の `main` を持ち、`tests/check.h` の
assertion harnessだけを共有する。1 file = 1検証対象。

| binary | source | 対象 |
| --- | --- | --- |
| `konbini_sim_tests` | `tests/sim/deterministic_primitives_test.cpp` | ID / bounds / counter RNG |
| `konbini_resident_presentation_content_tests` | `tests/sim/resident_presentation_content_test.cpp` | content v2 profileと検証 |
| `konbini_resident_presentation_projection_tests` | `tests/sim/resident_presentation_projection_test.cpp` | resident射影の決定性と位相境界 |
| `konbini_placement_cue_projection_tests` | `tests/sim/placement_cue_projection_test.cpp` | placement cue射影 |
| `konbini_presentation_schema_isolation_tests` | `tests/sim/presentation_schema_isolation_test.cpp` | canonical schemaからのpresentation隔離 |
| `konbini_visia_primitive_geometry_tests` | `tests/render/visia_primitive_geometry_test.cpp` | resident box / landing annulus geometry |
| `konbini_speech_bubble_geometry_tests` | `tests/render/speech_bubble_geometry_test.cpp` | bitmap font / bubble geometry |
| `konbini_store_placement_animation_tests` | `tests/render/store_placement_animation_test.cpp` | store placement sampler |

`konbini_render_domain` はVulkanへ依存しないCPU geometryなので、render側test
binaryも `KONBINI_BUILD_RENDER=OFF` でbuild・実行できる。

## 固定している契約

### content v2

- pinされた正常profile (`data/content/first-playable.json` と同値) がparseでき、
  値が1つでもdriftすると失敗する
- `residentPresentation` のkey欠落・未知key、root側のkey欠落・未知key
- 非finite値をJSON reader (`1e999` / `-1e999` / `NaN`) と struct検証の両方で拒否
- 非正値 (`0` / 負) を各fieldで拒否
- `speechDurationTicks > storeDwellTicks` の期間不整合を拒否。等しい場合は許可
- remarksの本数・空文字・小文字・数字・空白のみ・25文字超を拒否

### resident射影の決定性

- 同じseed / cell / tick / tableで record と remark が完全一致する
- table を別instanceとして組み直しても一致する (挿入履歴に依存しない)
- 同じcycle位置ならcycle何周後でも一致する
- remark index は `FirstPlayableResidentRemark` stream由来で、tickで変わらない

### 位相境界

- unassigned cellはAtHomeのまま、target storeもspeechも持たない
- 距離0の割当でも各legが1 tick以上進み、yawは0のまま
- `homeDwellTicks` 直前 / 直後、歩行の初回tick (進捗0で自宅位置) と最終tick
- store到着tick、発話終了tick、滞在最終tick
- return開始tick (store位置から出発)、cycle最終tick、wrapで自宅へ戻ること
- record順は cell ID昇順 → ordinal昇順

### placement cue

- 成功placementだけがcueを生成し、target positionはstore行のコピー、
  yawは0度
- 全 `PlacementFailure` 値でcueが0件
- 失敗なのにstoreがある / 成功なのにstoreが無い / storeが引けない /
  stale generation / inactive / facility・chain不一致はすべてfail-fast

### CPU geometry

- resident: 48 vertex / 72 index、index範囲、quadごとのindexパターン、
  三角形のwindingが自身のnormalと一致、finite値、pose位置と原点回転
- annulus: `(segmentCount + 1) * 2` vertex / `segmentCount * 6` index、
  水平normal、高さoffset、半径の単調拡大、alphaのfade、
  segment数の下限3・上限4096、退化・逆転definitionの拒否
- instance dispatchが直接builderと同じgeometryを返し、resident instanceの
  `normalizedAge` 指定を拒否する

### speech bubble

- A-Z、0-9、space、HUD用punctuation (`-:/.+`) のglyphが全て存在し、
  行が5bit幅に収まる
- 小文字・未対応記号・制御文字を拒否する
- `isBitmapFontCharacter` と `bitmapGlyph5x7` の判定がASCII全域で一致する
  (非throwのprobeと実際のlookupが食い違わない)
- 出力は tail 3 vertex + 背景quad 4 vertex + 点灯pixelごとに4 vertex
- 距離境界は閉区間 (`hideDistance` ちょうどは描画、それを超えたら省略)。
  cullされたrequestも検証は受ける
- camera basisの単位長・直交・右手系・extentを検証する
- indexは常に自分のvertexを指し、対応文字数上限を超えるstyleを拒否する

### schema隔離

- canonical snapshotのbyte長が facility / store / population / chain economy
  レコードの合計と一致する (residentやcueが混ざれば一致しなくなる)
- `residentPresentation` を全面的に変えてもcanonical bytesとhashは不変で、
  同じ変更が `RenderSnapshot` 側には現れる
- placement cueはcanonical snapshotの入力ですらないこと

## 未検証として残るもの

- 32bit index空間のoverflow自体。到達には数十億vertexが要るため、
  代わりに「対応文字数の上限」と「emitされたindexが常に自分のvertexを指す」
  という不変条件を固定している
- 実GPU経路の目視証跡。runtime接続後に `verification-strategy.md`
  「5. Pictor / Ergo integration」としてExcubitor経由で記録する
