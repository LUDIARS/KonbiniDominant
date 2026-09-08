---
task: kd-npc-004-presentation-validation
project: KonbiniDominant
kind: テスト
status: done
created: 2026-08-01
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 685
actio_task_id: null
memory_links:
  - spec/tasks/2026-08-01-kd-npc-001-visia-presentation.md
  - spec/feature/npc-conversations-and-placement-feedback.md
  - spec/interface/visia-presentation.md
  - spec/test/verification-strategy.md
  - spec/test/presentation-validation-tests.md
---

# KD-NPC-004 — Resident / Visia presentation validation

## 目的

KD-NPC-001で追加したpure projection、content v2、CPU geometry、animation samplerの
境界値と決定性を登録testで固定する。

## 完了条件

- content v2の正常profile、missing / unknown key、非finite値、期間不整合を検証する
- 同じseed / cell / tickでresident recordとremarkが一致する
- unassigned、距離0、歩行境界、store滞在／発話終了、return境界を検証する
- 成功placementだけがtarget position / yaw 0度のcueを生成する
- resident boxとannulusのvertex/index範囲、winding、finite値を検証する
- speech bubbleの対応glyph、未対応文字、距離境界、camera basis、index overflowを検証する
- hold / fall / landing / effect終了の境界と270度最終poseを検証する
- canonical schema/layoutにresident/cueが混入しないことを検証する
- runtime接続後はExcubitor経由の目視証跡をTestWorkflowへ記録する

## スコープ (編集可ディレクトリ)

- `tests/sim/`
- `tests/render/`
- `tests/CMakeLists.txt`
- `spec/test/`
- `spec/tasks/`

## 制約

このtaskのtest実行はユーザの明示指示を得たセッションだけで行う。
service起動を伴う場合はConcordia claim / releaseとプロジェクト本体フォルダを使う。

## 結果

7本のtest binaryを `tests/CMakeLists.txt` へ登録した。検証対象ごとにfileを
分け、共有するのは `tests/check.h` のassertion harnessだけにしている。
固定した契約の一覧は
[spec/test/presentation-validation-tests.md](../test/presentation-validation-tests.md)。

- `tests/sim/resident_presentation_content_test.cpp` — content v2の正常profile、
  key欠落 / 未知key、非finite値、非正値、期間不整合、remarksの字種と長さ
- `tests/sim/resident_presentation_projection_test.cpp` — 決定性 (同一seed /
  cell / tickでrecordとremarkが一致、table再構築とcycle周回でも一致)、
  unassigned、距離0、歩行 / 滞在 / 発話終了 / return の各境界、record順
- `tests/sim/placement_cue_projection_test.cpp` — 成功placementだけが
  target position / yaw 0度のcueを生成すること、全failure値でcue 0件、
  table不整合のfail-fast
- `tests/sim/presentation_schema_isolation_test.cpp` — canonical snapshotの
  byte長がauthoritativeレコードの合計と一致し、resident profileとplacement
  cueがcanonicalへ混入しないこと
- `tests/render/visia_primitive_geometry_test.cpp` — resident boxとannulusの
  vertex / index範囲、winding、finite値、pose適用、definition検証
- `tests/render/speech_bubble_geometry_test.cpp` — 対応glyph、未対応文字、
  距離境界 (閉区間)、camera basis、index空間
- `tests/render/store_placement_animation_test.cpp` — hold / fall / landing /
  effect終了の境界と270度の最終pose

`.anatomia/domains/resident-presentation.domain.json` で src と tests を対に
したドメイン宣言を追加した。

## 発見した不整合

test化の過程で見つけたが、このtaskのscope (production非変更) では直さず記録に
留める。別taskで扱う。

### bubbleHeightMeters がlayer間で食い違う

`validateResidentPresentationShape` (`src/sim/content/first_playable_content.cpp`)
は `bubbleHeightMeters > 0` しか要求しない。一方
`makeSpeechRequest` (`src/render/resident_visual_geometry.cpp`) は
resident Visiaの頭頂 (`head.centerMeters.y + head.halfExtentsMeters.y` =
1.72 m) より**大きい**ことを要求し、満たさなければ
`std::invalid_argument("invalid resident speech presentation")` を投げる。

つまりsim側の検証を通ったcontentが、発話residentが1体でも現れたframeで
render側を落とす。contentVersion 2のpin値は 2.2 m なので現行dataでは
到達しないが、検証の責務がlayerをまたいで分裂している。

最小再現:

1. `ResidentPresentationContent` の `bubbleHeightMeters` を `1.0` にする
   (他fieldはv2のまま)。`validateResidentPresentationContent` は通る。
2. store滞在中で発話中のresidentを含むtickで
   `projectResidentPresentations` を呼ぶ。residentは `speech` を持つ。
3. その結果を `buildResidentVisualGeometry` へ渡すと throw する。

想定される直し方はいずれもproduction変更を伴うため別task:
`bubbleHeightMeters` の下限をcontent検証側へ持ち上げるか、下限の正本を
resident Visia definition側の1箇所へ寄せる。

### 到達不能な防御checkが2箇所ある

- `projectStorePlacementCues` の `!isFinite(store.positionMeters)`:
  `StoreTable::append` が非finite positionを既に拒否するため到達しない。
- `speech_bubble_geometry.cpp` の `reserveVertexIndices` overflow:
  到達には32bit index空間を埋める数十億vertexが要る。

どちらも「防御として残す」判断はあり得るので不具合とは扱わないが、testでは
到達不能として扱い、代わりに不変条件側を固定している。

## 想定される失敗パターン (未実行のため)

このtestは一度も実行していない。初回実行で次が出る可能性がある。

- **compile error**: designated initializerの並び順、include漏れ、
  MSVC `/permissive-` とGCCでの差異。特に新規の `tests/render/` target。
- **浮動小数の厳密比較**: 射影・sampler側と同じ式をtest側で組み直して
  bit一致を期待している箇所がある。x87の余分精度が効く32bit x86 buildでは
  一致しない可能性がある。
- **canonical byte長のpin**: authoritativeなfieldを足す変更を入れると
  `presentation_schema_isolation_test` が落ちる。これは意図した検知であり、
  そのときはtest側の定数も同じPRで更新する。
- **counter RNGの定義変更**: 位相境界testは `counterRandom` の現定義を
  test側で組み直しているため、streamやmixerを変えると落ちる。これも意図した
  検知。

上記のうちcompile errorだけがtest自身の欠陥であり、残りは検知として正しい。

## 残作業

- build / ctest の初回実行。このtaskを実装したsessionはtest実行許可を
  持たないため、testは登録のみで未実行。
- runtime接続後のExcubitor経由の目視証跡をTestWorkflowへ記録する
  (完了条件の最終項)。
