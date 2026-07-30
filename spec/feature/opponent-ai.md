# Opponent AI

## Baseline

BASE-AI-01: single-playerで、chain select後の残り2chainをAIが操作する。
AIもplayerと同じcommand validation、資金、ZOC、Triangle、phase ruleを使い、
cheat用の別state mutation経路を持たない。

## Observation

AIが読める情報:

- publicな施設 / lot / store配置
- 自chainの資金、収益、faith、modifier
- visibleなZOC / Triangle / dimension state
- phase timer

fog of warは原案に無いため採用しない。

## Candidate generation

全lot総当たりを避け、spatial indexから次を列挙する。

- 自店舗周辺の拡張lot
- rival ZOC境界 / Triangle周辺
- 高人口 / 高需要lot
- Phase 2の有効vertical slot
- Phase 3のdimension / anti-store対象

candidateはstable ID順にcanonicalizeし、同scoreならseeded tie-breakを使う。

## Utility baseline

```text
utility =
  expectedRevenue
  + dominanceGain
  + rivalDenial
  + phaseProgress
  + survivalValue
  - buildCost
  - exposureRisk
```

weightは難易度profileで変える。AIは一定tick間隔で1 commandを選び、
simulation threadと別にworld stateを変更しない。

## Minimax

技術メモのminimaxは採用確定ではない。

- action branching、depth、evaluation、thinking budgetが未定
- real-timeでlot数が多い場合、完全minimaxは不適切になりうる
- baseline utility AIでdeterministic behaviorを作り、対戦深度が必要と計測された時だけ
  bounded minimax / MCTS等をADRで比較する

`TBD-AI-01`: 難易度、AI cadence、lookaheadの採否。
