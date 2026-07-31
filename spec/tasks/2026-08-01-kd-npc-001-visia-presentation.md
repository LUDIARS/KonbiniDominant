---
task: kd-npc-001-visia-presentation
project: KonbiniDominant
kind: 実装
status: done
created: 2026-08-01
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: null
actio_task_id: null
memory_links:
  - spec/feature/npc-conversations-and-placement-feedback.md
  - spec/interface/visia-presentation.md
  - spec/interface/pictor-rendering.md
  - spec/data/content-schema.md
---

# KD-NPC-001 — Ambient residents and Visia placement presentation

## 目的

集約人口を個別simulation entityへ変えず、街を店舗まで歩いて短く発話するresidentを
render snapshotへ派生する。人間と店舗着地effectをgame-owned Visiaで定義し、
Pictor / Ergo / Vulkan非依存のprimitive dummy geometryとして生成可能にする。

## 完了条件

- population cellとassigned storeからresidentの往復phase、位置、向きを決定的に派生する
- 来店中だけ近さに基づくdummy remarkと頭上bubble設定をsnapshotへ公開する
- 成功した店舗配置だけが一時placement cueを公開する
- placement samplerが0.20秒hold、0.65秒fall、270度回転、着地effectを返す
- residentとlanding ringのVisia definition / CPU geometryを持つ
- camera距離でcullするbubble background、tail、5x7英字text geometryを持つ
- contentVersion 2でresident presentation baselineをstrict validationする
- resident、speech、cue、effectをcanonical/saveへ追加しない
- unit / integration / behavior / startup testは明示指示なしに実行しない

## スコープ (編集可ディレクトリ)

- `include/konbini/sim/`、`src/sim/`
- `include/konbini/render/`、`src/render/`
- `data/content/`
- `spec/`

Pictor object registration、Ergo frame接続、native app、道路／歩道path、
日本語font assetとlocalizationは含めない。

## 実装結果 (2026-08-01)

- DoDの`PopulationCellTable`から非権威resident sampleを生成するprojectionを追加
- `RenderSnapshot`へresidentと配置成功cueを値コピーで追加
- schedule / remark用counter RNG streamとcontentVersion 2を追加
- resident boxとlanding annulusのVisia primitive geometryを追加
- snapshot residentから人型とspeech bubble geometryを配置するCPU projectionを追加
- camera-facing speech bubbleとuppercase 5x7 glyph geometryを追加
- 上空hold、eased fall、270度回転、ring effectを返すpure samplerを追加
- NPC、Visia、Pictor境界、world-state、UI、roadmapのspecを更新
- JSON / CMake source / spec heading link / Markdown link / frontmatter / diff whitespaceの
  静的検査のみを行い、unit / integration / behavior / startup testは未実行
