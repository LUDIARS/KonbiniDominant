---
task: kd-npc-002-pictor-runtime-integration
project: KonbiniDominant
kind: 実装
status: in_review
created: 2026-08-01
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 683
actio_task_id: ed35caa9-4d33-475d-898b-040eb4fd690e
memory_links:
  - spec/tasks/2026-08-01-kd-npc-001-visia-presentation.md
  - spec/interface/visia-presentation.md
  - spec/interface/pictor-rendering.md
  - spec/feature/npc-conversations-and-placement-feedback.md
  - spec/plan/implementation-roadmap.md
---

# KD-NPC-002 — Pictor / Ergo NPC runtime integration

## 目的

KD-NPC-001のresident、speech、placement cue、Visia geometryをproduction
Pictor / Ergo frameへ接続し、native app上で表示・更新・破棄する。

## 前提

- Gate 5の`GpuAssetStore`、`IBatchGpuSource`、`PictorFrameBridge`がreview済み
- native appがimmutable `RenderSnapshot`をframeごとにconsumeできる
- Pictor text atlasへ同梱できるfontとライセンスが確定している

前提未達の間にKonbini側へ仮のPictor実装や偽のsuccess pathを追加しない。

前提の状態 (2026-10-02): Gate 5、KD-NPC-003、KD-MOB-002 / 003はmain (04b4b42) に
マージ済み。fontは親セッションがNoto Sans JP (SIL OFL 1.1) に決定した。

## 完了条件

- resident stable IDとPictor object lifecycleを同期する
- ZOC再割当で目的地が変わるresidentを直前frame poseから補間し、瞬間移動を見せない
- Visia resident meshを1回uploadし、resident instance間で共有する
- world head anchorをscreenへ投影し、距離・off-screen・同時表示上限でbubbleをcullする
- 日本語を含む有限の会話lineを共有text atlasから描画する
- placement cueから店舗pose overrideとlanding effect lifecycleを開始する
- cue取りこぼし時も店舗をfinal poseで表示する
- removed resident / expired effectのPictor objectとresource参照を解放する
- desktopとmobileの同じsnapshot contractで動作する
- Excubitor経由の起動確認とTestWorkflow証跡を残す

## 実装内容

価値: UX-KD-W1 (都市俯瞰で住民と店舗支配の関係を読める)、UX-KD-W4 (desktop / mobile
で同じ契約)。正本は
[Presentation objects](../interface/pictor-rendering.md#presentation-objects)、
[Runtime resident sync](../feature/npc-conversations-and-placement-feedback.md#runtime-resident-sync)、
[Bubble culling](../feature/npc-conversations-and-placement-feedback.md#bubble-culling)、
[Japanese speech lines](../feature/npc-conversations-and-placement-feedback.md#japanese-speech-lines)。

- render (engine-neutral): `ResidentPoseTracker` (stable id、retarget blend)、
  `cullSpeechBubbles`、`appendSpeechBubbleDraws`、`buildPresentationMeshes`、
  `buildNpcPresentationDraws`、Noto Sans JP subset atlas (`speech_glyph_atlas`) と
  ja catalog (`speech_line_catalog`)。`WorldDrawList::presentation`へ出す
- Pictor adapter: `PresentationObjectSync` (PresentationObjectKey ↔ ObjectId、
  transform / bounds更新、mesh参照のacquire / release)、`loadPresentationGeometry`
  (startupで1回upload)、`GpuMeshKey` (facility / presentationのkey domain分離)、
  `IObjectTintSource` (batch planが両syncへtintを問う)。`PictorFrameBridge::consume`
  が両syncを適用し、registry総数 = 両mapping数を検査する
- app: `FramePresenter`がtickごとにtrackerをobserveし、composeでpresentation drawを
  組む。`GameSession`が起動時にcontent remarkのlocalization keyを検証し、
  `uploadGeometry` (desktop / mobile共通) がpresentation meshをuploadする
- data / tools: `data/fonts/NotoSansJP` (source.json、OFL.txt)、
  `data/locale/ja/resident_remarks.json`、`tools/bake_speech_glyphs.py`、
  生成物`src/render/generated/noto_sans_jp_speech_mesh.inc`。配布物へOFLを同梱

再利用探索の採否:

- 採用: Gate 5の`GpuAssetStore` / DYNAMIC pool / instanced pipeline / batch plan、
  `PictorSceneSync`のdiff規則、Visia CPU geometry、`ConstructionPresentation`の
  store pose override、`tools/bake_vector_font.py`の平坦化と台形分割
- 不採用: Pictor `text/`のraster font loader。world pipelineはvertex colorのみで
  sampler / texture descriptorを持たず、texture atlasを入れるとpipeline・descriptor
  の追加が要る。既存vector fontと同じ「glyphごとの共有mesh」をatlasとした
- 不採用: 5x7 bitmap speech geometry。日本語を描けず、本文ごとにmeshを作り直す

判断と理由 (BASE):

- BASE-NPC-LOCALE-01: content `remarks`の文字列をそのままlocalization keyにする。
  content v2 / v3 / v4の値とcanonical bytesを変えずに済む
- BASE-NPC-RETARGET-01: `targetStore`か`route`の変化をZOC再割当とみなし、0.6秒の
  smoothstepで直前表示poseから補間する
- BASE-NPC-BUBBLE-LIMIT-01: 同時bubbleは近い順に8件、NDC margin 0.1
- BASE-NPC-RING-FRAMES-01: 着地ringは内外径の拡大率が違い剛体変換で表せないので
  age 16 frameをbakeする

## 受け入れ条件

- C-1 PresentationObjectSync::apply(draws): residentはstable IDごとに1つのObjectIdを持ち、移動ではObjectIdを保ったままtransform / boundsだけを更新する
- C-2 ResidentPoseTracker::observe(residents, tick): targetStoreまたはrouteが変わったresidentは直前に表示したposeから補間し、最初のframeで新しい位置へ飛ばない
- C-3 loadPresentationGeometry(assets): resident Visia meshを1回だけuploadし、全residentが同じmeshの1 instanced batchで描かれる
- C-4 cullSpeechBubbles(camera, poses, spec): 距離超過・off-screen・同時8件超過のbubbleを除き、残りを近い順に返す
- C-5 appendSpeechBubbleDraws(camera, bubble, style): localization keyの日本語本文をNoto Sans JP subset atlasの共有glyph meshで描き、未知key・atlas外の文字は例外にする
- C-6 buildNpcPresentationDraws(residents, construction, camera, spec): placement sampleがlanding effectを持つ間だけLandingEffect drawを出す
- C-7 buildAnimatedStoreGeometry(stores, spec, {}): cueを取りこぼした店舗はsnapshotのtarget poseで描かれる
- C-8 PresentationObjectSync::apply(draws): snapshotから消えたresidentと終了したeffectのobjectを同じapplyでunregisterし、mesh参照をreleaseする
- C-9 GameSession::uploadGeometry(graph): desktopとmobileが同じupload / compose経路で同じsnapshot contractを使う

## 検証 (2026-10-02)

実施:

- Windows x64 Debug (Visual Studio 17 2022、`KONBINI_BUILD_TESTS=ON`、依存はpinと
  同じrevisionのsourceを`FETCHCONTENT_SOURCE_DIR_*`で指定) で全target build成功
- `ctest -C Debug`: 28/28 passed (main f2780ff 取り込み後は 31/31 passed)。追加した
  `konbini_npc_runtime_presentation_tests` (C-2, C-4, C-5, C-6, C-7)、
  `konbini_pictor_presentation_tests` (C-1, C-3, C-8)、
  `konbini_speech_font_asset_tests` (stale bake検出、OFL provenance、配布物のOFL同梱) を含む
- `tools/bake_speech_glyphs.py`の再bakeで生成物がbyte一致することを確認
- glyph bakeの目視確認: 便・いをASCII rasterで確認

未実施:

- Excubitor経由のアプリ起動確認とTestWorkflow証跡: 本委託ではアプリ / サービスの
  起動テストが禁止されているため実施していない。C-9はコード経路 (desktop
  `AppRunner`とmobile `NativeMobileRuntime`がともに`GameSession::uploadGeometry`と
  `FramePresenter::compose`を通る) とbuildで確認しただけで、実機・実画面の描画は未確認
- Android / iOSのbuild: 本変更では実施していない
- web (Pictor WebGL2) hostは`WorldDrawList::presentation`を描かない (scope外)

## 復旧方法

presentation objectは`WorldDrawList::presentation`が空なら何も登録されない。
問題が出た場合は`FramePresenter::compose`の`buildNpcPresentationDraws`呼び出しを
外せば、facility / store描画は変更前と同じ経路に戻る。共有meshのuploadは
`GameSession::uploadGeometry`の`loadPresentationGeometry`だけ。

## スコープ (編集可ディレクトリ)

- KonbiniDominant Pictor / Ergo adapter
- resident / effect object sync
- speech atlas / overlay layer
- native frame integration

Figmentum pedestrian path schemaはKD-NPC-003へ分離する。gameplayへ影響する
店舗評価とauthoritative個人AIは要件確定前のfuture candidateであり、このtaskへ
暗黙に含めない。
