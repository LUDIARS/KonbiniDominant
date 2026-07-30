# Boss — 高次元存在アイオーン

## 登場

foreign dimensionを2つ消滅させた後、高次元存在「アイオーン」が現れる。

原資料の勢力:

- 超次元立方マーケット体「アイオーン」
- 眷属4公「死角悦」「霞」「重成」「大栄」
- 弁当随一「めにーすとっぷ」
- 価格破壊マーケット「まいびすけっと」

名称は原案のパロディ表現を保持する。公開前に商標・表現review
`TBD-IP-01`を必須とし、実在brand名へ勝手に置換しない。

## 侵略

REQ-BOSS-01: boss勢力は全dimensionへ超高速でmarketを展開する。

spawn cadence、対象lot選択、4公/2chainの固有能力は `TBD-BOSS-SPAWN-*`。
AIは通常opponentと同じcommand interfaceを使うが、boss専用eventは
content dataからcommandを生成する。

## MaxValue

REQ-BOSS-MAX-01:

- boss側のDominant形成で必殺技`MaxValue`
- 9223372036854775807 damage
- 次元崩壊

原文自身が「ダメージ？」と疑問を残しているため、通常HP systemは追加しない。
DEC-BOSS-MAX-01として、この値は演出payloadであり、rule effectは
`CollapseDimension(reason=MaxValue)`とする。

誰のどのTriangleが発火させるか、予兆、回避可能時間は `TBD-MAXVALUE-*`。

## 時間の切り取り / 貼り付け

REQ-BOSS-TIME-01: 特定時間を切り取り、別位置へ貼り付けることでdimensionを
アイオーン側へ塗り替える。

game ruleへの写像は未定。候補:

- 過去tickのstore ownership snapshotをregionへ再適用
- future intervalをskipしboss commandだけ適用
- regionのtime scale / command windowを差し替える

これはresultを大きく変えるため `TBD-AION-TIME-01`をowner決定するまで
実装しない。単なるvisual rewindで要求を消化したことにしない。

## 30秒の顕在化

REQ-BOSS-WINDOW-01: 顕在化限界は30秒。

fixed tickへ変換し、`bossManifestationEndTick`を設定する。pause / settings中に
simulation tickが止まる場合はwindowも止まる。

## 逃走

REQ-BOSS-ESCAPE-01:

- playerは無限に続く他次元へ逃げる
- 少なくとも1店舗、自分の存在を世界へ残す

dimensionは実際に無限配列を生成せず、escape command時にordinalを増やしてlazy生成する。
同時resident dimension数とcache evictionは性能budgetで定める。

## 勝敗 baseline

原資料は肯定的な勝利条件を明記していない。BASE-BOSS-RESULT-01:

- `survivingPlayerStore` は `DimensionState::Active` にある自chain storeと定義する
- `Collapsing` / `Destroyed` dimensionのstoreは生存判定に含めない
- 各boss tickのdimension state更新後、生存storeが0なら即敗北
- 30秒window終了時、生存storeが1つ以上なら勝利、0なら敗北
- MaxValueでorigin dimensionが消えても、escape先にstoreがあれば継続

これによりwindow終了時の判定は必ずWin / Loseのどちらかになる。
「死」の定義はこのactive dimension上の全店消滅とする暫定解釈。owner確定は
`TBD-BOSS-RESULT-01`。

## 不変条件

- manifestationは30秒ちょうどをtickで測る
- `MaxValue`をinteger overflow計算へ使わない
- lazy dimension生成は同じordinalなら同じworld
- GameEnded後はboss spawn / collapse commandを適用しない
