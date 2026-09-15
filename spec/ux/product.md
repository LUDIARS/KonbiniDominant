---
title: KonbiniDominant — ゲームが現実のルールを壊して侵略してくる感覚
type: feature
ux_definition: 1
id: UX-KD-PRODUCT
service: konbini-dominant
ux_scope: product
status: draft
owner: neco
---

# KonbiniDominant のプロダクト UX

本文書は [全体設計](../design.md) §1 の目的と、[game-flow](../feature/game-flow.md) /
[ui-ux](../feature/ui-ux.md) の既存仕様から起こした draft。価値 ID と判断方法は設計案であり、
人間の承認や実機評価が済んだことを意味しない。DDD ゲート (spec/ux の存在と
`spec/domains/` の specRefs) を満たすための入口として置き、内容の確定は neco の判断に従う。

## 誰の、どの状況の問題か

架空都市 N-KXi を舞台にしたインクリメンタル都市支配ゲームを遊ぶプレイヤー。
店舗を置き、支配領域 (ZOC / Dominant Triangle) を広げ、Phase が進むにつれて
垂直スタック・多次元対消滅・高次元存在からの逃走へエスカレートする体験を求めている。
現状の代替手段 (通常の経営シミュレーション) は「ゲーム側がルールを壊して侵略してくる」
感覚を与えない。

## このプロダクトが何を解決するか

利用前: 配置と収益を最適化するだけの都市経営。
利用後: コンビニの平面支配から始まり、ゲームが現実のルールを壊して侵略してくる感覚を、
Phase ごとに段階的に強めながら最後まで存在を残す遊びになる。

## どの価値を実現するか

| 価値 ID | 利用者に起きる望ましい変化 | 判断方法・条件 | 証拠 | 現状 |
|---|---|---|---|---|
| UX-KD-W1 | 都市俯瞰で、配置可否と支配関係を pointer 移動中に理解できる | lot hover のプレビュー、invalid placement の reason text、ZOC / Triangle 表示 | 操作シナリオ + render snapshot | 未評価 |
| UX-KD-W2 | 同じ seed と command stream から同じ結果が再現され、勝敗が納得できる | 決定論テストと save / replay の一致 | simulation テスト | 未評価 |
| UX-KD-W3 | Phase が進むごとに「ルールが壊れて侵略してくる」と感じる | Phase 1 → 2 → 3 → Boss の遷移で新しい制約・脅威が提示される | 遷移シナリオ + neco の観察 | 未評価 (今回の出荷範囲は Phase 1 単独) |
| UX-KD-W4 | Windows / Android / iOS で同じ simulation・content・save が使える | platform host 以外へ OS API を漏らさない境界の維持 | adapter 契約と device validation | 一部未検証 |

## 主要シナリオと失敗からの回復

- Chain Select → Phase 1 → Result → Chain Select (2026-09-09 の出荷範囲)。
  勝敗・5 分制限・再挑戦は [phase-1-game-loop](../feature/phase-1-game-loop.md) が正本。
- Boot で content / dependency / shader / save schema を fail-fast 検証する。欠落は起動時に
  明示的に失敗し、上方探索の fallback で隠さない。
- 未確定 content key があるビルドでは開始を fail-fast し、空欄のまま進めない。

## 守る制約と優先順位

[全体設計](../design.md) §1 の優先順位 (再現性 → 一括 data scan → rule と描画・都市生成の
分離 → データ追加での再利用 → 未決事項を偶然で確定しない → 3 platform で同じ契約) に従う。

## コア / 支援 / 汎用の分類理由

| 分類 | ドメイン | 理由 |
|---|---|---|
| コア | simulation, city-model | 支配・Phase・経済・人口・盤面の意味を所有し、価値 W2 / W3 を直接決める |
| 支援 | rendering, resident-presentation, app-entry | 価値 W1 の提示と入口。simulation の確定値を射影するが意味を変えない |
| 汎用 | platform-adapters | Pictor / Ergo / Figmentum / OS の差の吸収。技術を交換しても価値は変わらない |
