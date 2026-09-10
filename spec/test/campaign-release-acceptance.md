# キャンペーン版の受入・配布要件

- 決定日: 2026-09-10
- 指示: 共通の更新は本社が引き取り、マージと100倍速クリアをテスト要件にする。
- 現状: 最新mainの店舗描画を取り込んだ未マージ版c50abfeでもBTランナーが起動・100倍速クリアに成功。LLM呼び出し0、終了コード0、stderr空。PR競合は解消して順次審査中。マージと統合後の再検証は未実施。
- 共通基盤（Pictor等）の更新担当: 本社。KonbiniDominant側は更新後の接続・動作を確認する。

## 必須条件1: 対象変更のマージ確認

- [ ] Revisor PR 1627（Phase 1）、1628（Phase 1–4・強化）、1629（マウス／タッチ）の統合状態を確認する。
- [ ] 起動時の退化三角形修正、モバイル接続、100倍速検証機構（実装commit `944b1b599d7bd40067e2b07c47068e04c5a9070e`）が統合対象に含まれることを確認する。古いPR headのマージだけで完了扱いしない。
- [ ] PR番号・統合したhead・マージ結果・統合後mainのcommitを記録する。squash等でSHAが変わる場合は対応する差分を照合する。
- [ ] 統合した版からReleaseビルドし、実行EXE・content・SPIR-VのSHA-256とビルドログを保存する。

本体mainにある既存の未コミット変更を上書き・破棄して進めない。
この項目は統合確認であり、マージ成功だけで起動テスト合格とはしない。

## 必須条件2: 統合した版で100倍速クリア

- [ ] Concordiaでテスト利用状況を確認し、自分のsessionで `konbini-dominant-app` をclaimする。
- [ ] 2026-09-10のユーザー指示により、テスト用EXEはExcubitorを経由せずproject bodyから直接起動してよい。worktree・複製フォルダからは起動しない。
- [ ] contentとshaderを含む上記ビルドで、街とHUDの実描画を確認する。起動直後の例外終了がないことを確認する。
- [ ] `playtest.cfg` の `100 autoplay` を使い、選んだ1種類のコンビニで通常の資金・敵AI・強化・勝敗条件を維持する。
- [ ] 同じrunでPhase 1 → Phase 2 → Phase 3 → アイオーン警告 → Boss → 勝利結果まで到達する。勝利状態への直接書き換えやルール緩和は使用しない。
- [ ] `playtest-report.jsonl` に `timeScale:100` と各Phaseの到達を記録し、同じrunの `resultPresented` が `phase:2`、`outcome:1`、`foreignDestroyed>=2`、`framesPresented>0` を満たす。
- [ ] 初期校正時は実際のクリア画面を目視確認し、スクリーンショットまたは動画を保存する。以後の反復は[LLM不要のBTテスト](no-llm-native-playtest.md)で通常勝利・present・実画面保存を機械判定する。自動実行を毎回の目視確認済みとはしない。
- [ ] 起動・描画ログ、ゲーム内経過時間、実時間、実効倍率、tick欠落の有無を記録する。100倍速設定と実測速度を混同しない。
- [ ] 成否にかかわらず自分がテスト起動したプロセスを終了し、testing claimをreleaseする。

操作・ログの詳細は [accelerated-playthrough.md](accelerated-playthrough.md) を正本とする。
負け・途中終了・画面未確認・証跡不足は合格にしない。修正した場合はその版で再確認する。

## 配布判定と残る確認

両条件の証跡が揃うまで修正版ZIPを「検証済み」として提出しない。
ZIPは確認したEXE・content・shaderから作り、ハッシュで照合する。`playtest.cfg` は同梱せず、通常の1倍速・手動操作を既定にする。
ローカル証跡は再現可能な自己申告記録であり、署名済みCI attestationではない。
BUILD_INFO.jsonは`localValidationPassed`として記録し、`mergeGatePassed`をtrueにしない。

100倍速の自動入力はタッチのhit testを検証しない。既存の手動操作・モバイル検証項目は別途残る。
Android Studio／Xcode環境がないため、Android・iOSのビルドと実機動作は未検証のまま扱う。
Windowsの合格をAndroid・iOSの合格へ流用しない。

## 結果記録

| 項目 | 結果 |
|---|---|
| PR 1627 / 1628 / 1629 | 2026-09-10 Cc参照時点: すべてopen |
| 起動修正・モバイル・100倍速機構 | 944b1b5に実装、ローカルcommit済み |
| 統合後main commit / 実行物SHA-256 | 未記録 |
| 統合後ビルド / 起動 / 100倍速クリア | 未実施 |
| マージ前の初回クリア | 911d960、run 1、VICTORY画面確認済み、9.31977秒、設定100倍速／実効約58.7倍（起動込み）、stderr空 |
| BT自動クリア（最新店舗描画取り込み版） | c50abfe、10.6265秒、設定100倍速／実効約51.4倍、LLM 0、終了コード0。証跡: results/2026-09-10-bt-storefront-clear.json |
| 配布判定 | 統合後の検証待ち |

## 最新開発版の共有例外（2026-09-10）

ユーザーの「最新のビルドをまとめてDiscordに転送」に従い、審査中の最新ビルドも開発版として共有する。
その場合は --allow-unmerged を明示し、ZIP名とBUILD_INFO.jsonにdevelopment-unmerged、mergeGatePassed=falseを記録する。
起動・100倍速通常クリア・実行資産ハッシュの照合は省略しない。統合後の受入合格とは区別する。
