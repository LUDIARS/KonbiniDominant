---
tags: [notion, requirements, pictor, ergo, figmentum, dod]
date: 2026-07-31
kind: investigation
---

# 原資料と読み替え

## Sources

1. [コンビニドミナント](https://app.notion.com/p/31a39cbfbab980f582dbed1f5da4b5ec)
2. [コンビニドミナント - 最適化/高速化/DOTS](https://app.notion.com/p/DOTS-31a39cbfbab980e88471e30fd0de0cbd)
3. 2026-07-31の指示:
   - 新しいrepository
   - UnityではなくPictor / Ergo
   - DOTSはDoDに読み替え
   - Figmentumで都市を形成

Notion connectorで2026-07-31に取得。page viewが返したcontent snapshot時刻は
それぞれ2026-03-05。

## Priority

最新指示が技術メモを上書きする。

| 原記述 | 本spec |
|---|---|
| Unity | 不採用 |
| DOTS | Unity APIでなくengine-neutral DoD |
| GPU Instancing | Pictor batch / instance候補 |
| WFC | Figmentum内部で必要な場合のみ候補 |
| 都市動的生成 | Figmentumが正本 |

## Direct requirements

- 架空都市N-KXi。西葛西の特徴を持つが西葛西ではない
- 中央駅anchorから都市を動的生成
- chain selectと初期資金5店舗分
- Phase 1: facility破壊、store配置、ZOC、3店Triangle、顧客化、収益buff、囲み破壊
- Phase 2: vertical stack、高階cost、他社上配置、faith、image strategy
- Phase 3: random dimension、inversion、anti-store、対消滅、energy、2dimension消滅
- Boss: Aion勢力、MaxValue、時間切貼り、30秒、無限dimensionへ逃走、1店残す
- screen flow: 起動、Title、In Game、Result

## Ambiguities found

- phase mechanicが累積か置換か
- single-player / remaining 2chain AIか
- 「近くの3店」の距離・chain条件
- Triangle内rival破壊が自動かcommandか
- ZOC攻略の「相手より多い」の範囲
- economy / population / faithの式
- 「256回建」が256階の誤記か
- セバンイレバンの「7社」「イレバン」
- 別dimensionの「同じ位置」
- Bossのtime cut/paste rule
- 30秒後の肯定的なwin condition
- 「死」がavatar / 全店 / dimension collapseのどれか
- バンサバライクとpointer配置のみの関係

暫定解釈は [open-questions.md](open-questions.md) へ分離した。

## Engine investigation

参照した追跡点:

- Pictor `origin/main` `c088e8d...`
- Ergo `origin/main` `771b027...`
- Figmentum `origin/main` `719af46...`
- AIFormat `origin/main` `0cb3232...`

主な発見:

- Pictor内部はflat array / SoA、境界はOOPというDoD方針
- Pictorのmesh upload / host-driven drawはconsumer側bridgeが必要
- Ergo renderはPictor data integrationをgame側に残す
- Ergo inputのOS pollはno-opでcallback adapterが必要
- Figmentumは`1 unit=1m`、city/building/roadのSDF生成を持つ
- Figmentumのcity一括結果にはstable facility semanticsが無い
- interactive破壊にはsemantic plan APIと施設単位geometryが必要

詳細は各`interface/`文書を正本とする。

## Naming / IP

原案のchain / boss名はparodyとして保持した。実在brand名へ戻さず、公開・配布前に
法務/商標/表現reviewを行う。
