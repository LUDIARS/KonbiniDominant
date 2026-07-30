# Phase 1 — 2次元的侵略

## 目的

都市平面へ店舗を配置し、ZOCと3店舗のDominant Triangleで人口・収益・
rival店舗を破壊して支配を奪う基礎phase。

## 店舗配置

REQ-P1-PLACE:

1. playerが配置可能lotをpoint/clickで選ぶ
2. 配置対象の既存施設を破壊または置換する
3. chain / variantの建設費を消費する
4. 同じlotへ店舗をspawnする

commandは次を全て満たす場合だけ成功する。

- dimensionがActive
- phaseで平面配置が許可
- lotが存在し配置可能
- `verticalSlot=0`
- 同じslotが未占有
- chainに十分なcash
- variantが選択chainで利用可能

facility破壊とstore spawnは同じstructural transactionとして適用し、片方だけ成功する
中間状態を作らない。

## ZOC

各active storeはchain特性とmodifierから求めた影響半径を持つ。

```text
effectiveRadius =
  baseRadius(chain, variant)
  × chainPassive
  × faithModifier
  × timedModifiers
```

- BASE-P1-ZOC-01: XZ平面の円形range
- facility形状や道路で遮蔽しない
- 同chainの重複ZOCは影響scoreを加算する
- rival chainのscoreはpopulation cell単位で比較する
- Voronoi表示を採用する場合も、表示境界とgameplay scoreの正本を混同しない

半径とscore式は `TBD-ZOC-01`。

## Dominant Triangle

REQ-P1-TRI-01: 近接する3店舗でTriangleを形成する。

BASE-P1-TRI-01:

- 同一chain、同一dimension、activeな3店舗のみ
- storeのXZ中心を点とする
- chainごとのDelaunay triangulationからcandidateを得る
- 全辺 `<= maxEdgeMeters`
- 面積 `>= minAreaSquareMeters`
- 同一直線上の3点は無効
- 頂点storeの移動・破壊・anti-store化で即invalid

Delaunayは候補数を抑える手段。原案が「任意3店の組合せ」を意図する場合は
`TBD-TRI-ALGO-01`で変更する。

## Triangle内の効果

REQ-P1-TRI-02:

- 内部の住民を自chainの顧客へ誘導
- 収益buff
- rival storeを囲むと破壊可能

point-in-triangleはboundary ruleをcontentで固定する。
BASE-P1-CAPTURE-01として、rival storeがactive Triangle内部へ入り、
そのtick終了時にもTriangleが有効なら`Encircled`になる。破壊が自動かplayer commandかは
`TBD-TRI-CAPTURE-01`。

複数Triangleの収益buff stackingも `TBD-TRI-STACK-01`。実装前は
`Max`を暫定baselineとし、指数的な重複を避ける。

## 相手ZOCの攻略

REQ-P1-CONTEST-01: 相手より多く店舗を建てる必要がある。

「多い」の範囲は原案で未定。BASE-P1-CONTEST-01として、対象PopulationCellを
覆う各chainのactive store数を比較し、同数なら既存owner維持とする。
距離weightを含む最終式は `TBD-ZOC-CONTEST-01`。

## Phase進行

次のいずれかでPhase 2へ進む。

- すべてのrival storeを破壊し、N-KXiのdomination条件を満たす
- `phase1DurationTicks`経過

domination条件（人口比、lot比、面積比）は `TBD-DOMINATION-01`。
timer進行時にrivalが残っていても削除せず、Phase 2へ持ち越す。

## 不変条件

- cashはplacement成功時だけ減る
- invalid Triangleは収益・captureに寄与しない
- store破壊とTriangle無効化は同tickで完了
- simulation結果は描画frame rateに依存しない
- Triangle生成順に関係なくstable store IDから同じ結果を得る
