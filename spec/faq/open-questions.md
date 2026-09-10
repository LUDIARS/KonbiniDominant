---
tags: [tbd, decisions, baseline, game-design]
date: 2026-07-31
kind: design
---

# 未決事項と暫定baseline

## P0 — 実装開始前

| ID | 問い | 現baseline | owner |
|---|---|---|---|
| `TBD-BOSS-RESULT-01` | Bossのwin/lose | 30秒後1店以上でwin、全店消滅でlose | neco |
| `TBD-PHASE-CUMULATIVE-01` | phase mechanicは累積か | 累積 | neco |
| `TBD-AI-PLAYERMODE-01` | player mode | 1人 + 残り2chain AI | neco |
| `TBD-NKXI-PROFILE-01` | 西葛西らしさの要素 | 中央駅+density bandのみ確定 | neco / design |
| `TBD-PERF-01` | target規模 / FPS / tick / memory | 未設定 | tech |
| `TBD-MOBILE-OS-01` | Android / iOSの最低OS、device tier | 未設定 | tech |
| `TBD-MOBILE-PERF-01` | mobile FPS / frame time / GPU memory / thermal budget | 30 FPS、10 TPSをfirst probe baseline | tech |
| `TBD-MOBILE-ORIENTATION-01` | portraitを製品要件に含めるか | first mobileはlandscape | neco / design |
| `TBD-DIM-COORD-01` | 別dimensionの同位置 | explicit anchor map | design |
| `TBD-TRI-CAPTURE-01` | 囲み破壊 | Encircled後の発火方式未決 | neco |
| `TBD-DEPS-01` | dependency pin方式 | reproducible pin必須 | tech |
| `REQ-CITY-GAP-01` | Figmentum semantic plan | upstream API追加 | Figmentum |

## P1 — Rule実装前

| ID | 問い |
|---|---|
| `TBD-DOMINATION-01` | N-KXi支配を人口 / lot / 面積のどれで判定するか |
| `TBD-ZOC-01` | ZOC radius / score / overlap |
| `TBD-ZOC-CONTEST-01` | 「相手より多い」の範囲とtie |
| `TBD-TRI-ALGO-01` | Delaunay candidateで原案を満たすか |
| `TBD-TRI-STACK-01` | 重複Triangleのbuff |
| `TBD-ECON-*` | build / revenue / refund / vertical cost |
| `TBD-POP-*` | 破壊、流入、回復、capacity |
| `TBD-FAITH-*` | range、近隣支配、増減 |
| `TBD-IMAGE-*` | image strategyのcost / cooldown / target |
| `TBD-STACK-SUPPORT-01` | 下層破壊時の上層 |
| `TBD-PHASE-256-01` | 256階解釈 |
| `TBD-INVERT-*` | inversion cost / operator |
| `TBD-ENERGY-*` | energy量 / duration / collapse |
| `TBD-SEBAN-01..03` | 7社、上げ底、イレバン |
| `TBD-AION-TIME-01` | 時間切取/貼付のgame rule |
| `TBD-MAXVALUE-*` | 発火Triangle、予兆、回避 |

## P2 — Content / polish

- chain数値balance
- AI difficulty / lookahead
- WFC / Voronoi visualization / minimax採否
- 商品flavorをmechanic化するか
- save slot / autosave
- localization
- accessibility optionの既定
- result score / ranking
- parody名称のIP review

## Baseline change rule

`BASE-*`を変更する時は:

1. 変更理由とsource/owner判断を本fileへ記録
2. affected `feature/` / `data/` / `interface/`を同じchangeで更新
3. deterministic replay / save compatibilityへの影響を判定
4. 実装済みならmigration / testも同時更新

baselineが書かれていることをowner確定とみなさない。

## Phase 1 限定の決定 (2026-09-09)

ユーザーは3勢力、3秒包囲の自動破壊、敵全滅＋現在人口60%で勝利、
5分で支配人口比較（首位同点は引き分け）、店舗ゼロ＋資金不足で敗北、
結果から再挑戦を承認し、残る実装判断を委任した。
今回の `TBD-TRI-CAPTURE-01` / `TBD-DOMINATION-01` / `TBD-LOSE-02` は
[Phase 1 単独版](../feature/phase-1-game-loop.md) の範囲で解消する。
3チェーンの原案固有能力、モバイル、後続Phaseの未決事項は確定した扱いにしない。

## Decision log

| date | decision |
|---|---|
| 2026-07-31 | repository名を`KonbiniDominant`とした |
| 2026-07-31 | Unityを不採用、Pictor/Ergo、DOTS→DoD、都市→Figmentum |
| 2026-07-31 | initial repositoryはprivate、license未指定 |
| 2026-07-31 | N-KXi表記を標準化（原文の`N-Kxi`揺れを統一） |
| 2026-07-31 | 個別住民でなくPopulationCell集約をbaseline |
| 2026-07-31 | neco指示によりsmartphone対応を製品要件へ追加。Android先行、iOS後続 |
