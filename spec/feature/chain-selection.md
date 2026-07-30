# Chain selection

## 共通

New Game開始時に3chainから1つを選ぶ。BASE-AI-01として、残り2chainは
同じruleに従うAI opponentになる。

全chainは次を持つ。

- base build cost
- base ZOC radius
- revenue / quality
- variant list
- passive modifier list
- presentation tags

chain差はcontent dataで定義し、system内のchain名switchを増やさない。

## ローサン

REQ-CHAIN-LOSAN:

- 低単価、高単価、普通、少し良いローサンを展開できる
- 他2社よりも単一の固有特性は弱い
- 「牛乳がうまい」はflavor

設計意図はsituational variant選択。variantごとのbuild cost / ZOC / revenue /
quality trade-offは `TBD-CHAIN-LOSAN-01`。

## ファモマ

REQ-CHAIN-FAMOMA:

- 怪音波で住民を洗脳
- ZOCとDominant areaが広い
- 建設単価が高い
- 「チキンがうまい」はflavor

怪音波はZOC radius / influence modifierとpresentation eventの組で表現する。
住民個体へのaudio raycast systemは追加しない。

## セバンイレバン

REQ-CHAIN-SEBAN:

- 建設単価が低く、序盤の展開効率が高い
- dominance拡大に伴い「上げ底効果」でstatが下がる
- 「7社以上でドミナントするとイレバンする」

未決:

- `TBD-SEBAN-01`: 7が店舗数、Triangle数、領域数のどれか
- `TBD-SEBAN-02`: 低下するstatとcurve
- `TBD-SEBAN-03`: 「イレバン」の発動結果

実装まではpassive IDを予約しても、推測effectを付けない。

## Balance invariant

balance目標は同じ勝率ではなく、異なる展開判断を作ること。

- ローサン: variantによる適応
- ファモマ: 高コスト・広支配
- セバンイレバン: 低コスト・拡大に伴う劣化

content検証では、すべてのchainが初期資金で5店舗を建てられること、
固有modifierを除いた共通ruleが同じことを保証する。
