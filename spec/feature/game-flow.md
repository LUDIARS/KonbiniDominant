# Game flow

> v4実装: ユーザーの判断委任とPhase 2–4実装指示に基づく具体値・暫定決定は
> [full-campaign-baseline](full-campaign-baseline.md)を正本とする。
> 下記の原案に残るTBDのうち実装済み項目は同baselineで解決し、
> 追加の11種スキルは[skill-upgrades](skill-upgrades.md)で定義する。

## Phase 1 単独版 (2026-09-09)

今回の出荷範囲は `Chain Select → Phase1 → Result → Chain Select`。
勝敗・5分制限・再挑戦は [phase-1-game-loop.md](phase-1-game-loop.md) が正本。
Title / Continue / Phase 2 以降は下記の将来仕様であり、今回の実装には含めない。

## 画面

REQ-FLOW-01:

```text
Boot → Title → Chain Select → World Generation → In Game → Result
```

- Boot: content / dependency / shader / save schemaをfail-fast検証
- Title: New Game / Continue / Settings / Quit
- Chain Select: ローサン / ファモマ / セバンイレバン
- World Generation: Figmentumが中央駅anchorを持つN-KXiを生成
- In Game: Phase 1 → Phase 2 → Phase 3 → Boss
- Result: 勝敗、支配統計、再挑戦 / Title

Continue / Settings / Result内の詳細導線は `TBD-UX-01`。

## 状態機械

```text
WorldGeneration
  → Phase1.Active
      ├─ rivalsDestroyed && cityDominated
      └─ phase1TimerElapsed
  → Phase2.Active
      ├─ targetVerticalSlotsReached
      └─ phase2TimerElapsed
  → Phase3.Active
      └─ twoForeignDimensionsDestroyed
  → Boss.Active
      ├─ playerStoresInActiveDimensions == 0 → Result.Lose
      └─ manifestationWindowElapsed
           && playerStoresInActiveDimensions >= 1
           → Result.Win  [BASE-BOSS-RESULT-01]
```

通常phaseの遷移条件が同tickで複数成立した場合は、敗北判定 → 進行条件 →
timerの順で解決する。

Boss tickだけは、boss event / collapse適用 → dimension state確定 →
active dimension上のplayer store数算出 → 0なら敗北 → 1以上かつ30秒window満了なら
勝利、の順に固定する。同tickのMaxValueをwindow満了より後へ遅延してはならない。
Resultへ入ったtick以後のcommandは受理しない。

## 累積解禁

BASE-FLOW-01: インクリメンタルゲームとして、前phaseのmechanicは次phaseでも残る。

- Phase 2でも平面配置、ZOC、Triangle、人口、収益が動く
- Phase 3でも垂直stackと信仰度が動く
- Bossでは全mechanicを使って「どこかに1店舗を残す」

原案は累積か置換かを明記していないため、ownerが別解釈を選ぶ場合は
`TBD-PHASE-CUMULATIVE-01`を更新する。

## カメラ / 操作mode

プレイヤーavatarの移動・自動攻撃は原案に無い。BASE-FLOW-02として、
本作は都市俯瞰のpointer配置ゲームとする。「バンサバライク」は
時間圧、敵勢力の増殖、survival curveの参照に限定する。

## Result

表示候補:

- 生存時間 / 到達phase
- 最大同時店舗数
- 最大Dominant Triangle数 / 支配人口
- 破壊した次元数
- 最終生存店舗と次元
- chain固有metric

score式とrankingは原案外のため `TBD-RESULT-01`。
