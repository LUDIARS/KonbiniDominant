# LLMを使わないネイティブBTテスト

- 決定日: 2026-09-10
- 目的: テストの繰り返しでLLMトークンを消費しない。初期調整と失敗原因の調査だけを人・エージェントが担当する。
- 自動プレイ、合否判定、証跡収集にモデル・プロンプト・LLM APIは使用しない。HTTP通信はローカルCcの予約・解放のみ。

## BT

Reactive Priority Selectorを毎fixed tickで評価する。

1. チェーン未選択なら、通常のチェーン選択コマンドを発行。
2. 強化選択待ちなら、提示された候補から決定的な優先順位で選択。
3. Boss戦・警告中に条件が揃えば、通常の次元退避コマンドを発行。
4. Phase 3では観測済みの本世界アンカーを使い、対消滅対象の配置・世界移動・反転を行う。
5. それ以外は人口と既存店舗から配置候補を決め、資金を残しながら出店。

条件不成立はFailure、コマンド発行はSuccess、資金・信仰等の待機はRunning。
Runningなら下位ノードへ進まない。状態の直書き、資金追加、敵停止、勝利の強制は行わない。
blackboardには画面のsnapshotから観測した情報だけを保持する。

## 実行

Windows、Python（Pillow導入済み）、CMake、Git、Lictor CLI、現在のLICTOR_PORTと
Excubitor ProcessMapから供給されたloopbackのCONCORDIA_URLが必要。
新たなパッケージの自動ダウンロードやモデル呼び出しは行わない。
実行前にソースをcommitする。ランナーはdirtyなソース、worktree／複製本体からの起動を拒否する。
ゲームウィンドウを前面に出してキャプチャするため、画面操作と同時に実行しない。

統合後の本体mainで実行する標準コマンド:

python tools/run_native_playtest.py --project-body PROJECT_BODY --build-dir PROJECT_BODY\build

マージ前の作業branchを本体へ配置して検証する場合:

python tools/run_native_playtest.py --project-body PROJECT_BODY --build-dir CLEAN_SOURCE_WORKTREE\build-native --allow-unmerged

mainと異なるcommitはpreMerge=trueとして記録し、マージ・統合後検証を合格扱いしない。
本体に他作業の未コミット変更がある場合、同じリポジトリのcleanなworktreeをmainと完全に同じcommitに置いてビルドしてよい。
mainとsourceのSHAが一致した場合だけpreMerge=falseにする。起動先はこの場合も本体build/Releaseに固定する。
テストのEXE直接起動はユーザー承認済み。Cc claimと本体フォルダ制限は維持する。

## 自動処理と判定

1. Releaseビルド、source commit・main commit・実行資産SHA-256を記録。
2. Ccへ本体のmain branchを登録し、テスト枠を予約。
3. 実行資産を本体build/Releaseに配置して、100 autoplayで直接起動。
4. ローカルファイルを監視し、同じrunの全Phase到達、通常のアイオーン生存勝利、結果のpresentを確認。
5. 対象プロセスの実ウィンドウをPNG保存。黒画面・取得失敗は不合格。
6. 対象プロセスを通常終了し、終了コード0、stderr空、資産ハッシュ不変を確認。
7. テスト用設定と一時配置を元に戻し、Ccテスト枠を解放。
8. validation-result.jsonと終了コードだけで合否を返す。合格=0、不合格=1。

ビルドは既定600秒、プレイは既定180秒でタイムアウト。失敗時も自分のプロセスを終了・回収する。
証跡は本体build/test-runs/<実行日時>/へ保存する。成功ログだけで失敗ログを上書きしない。
出力: build.log、artifact-manifest.json、stdout/stderr、playtest-report.jsonl、clear-screen.png、validation-result.json。
package toolはbuild.logを含む証跡hashと資産hashを再照合する。ただし同じローカル利用者が
変更可能な自己申告証跡であり、認証済みCI attestationや外部merge gateとしては扱わない。
100倍速設定と実効倍率は別の値として記録する。ゲーム時間と実時間はログから算出する。

初期校正では実画面のVICTORY表示を人・エージェントが確認する。
以後の自動実行は通常勝利ログ・present・非空の実画面保存を機械判定し、humanVisualReview=falseを記録する。
テキストの可読性や表示内容の目視確認済みを自動で捏造しない。画面確認が必要な変更時・失敗時にだけ画像を調べる。
自動入力はmouse/touch hit testの代替ではない。Android/iOS実機検証も別途必要。

## 初回BTランナーの実行結果

baf85e1でpassed=true、LLM呼び出し0、終了コード0、stderr空。
100倍速設定でゲーム内546.1秒を約9.70秒（起動込み）でクリアし、実効約56.3倍を記録した。
画面保存・一時配置復元・Cc releaseも自動完了。
[実行記録](results/2026-09-10-bt-native-clear.json)。この結果はmain取り込み前の版であり、統合後は再実行する。

## 配布時の照合

python tools/package_campaign.py --build-dir build-native --revision FULL_COMMIT_SHA --playtest-run ABSOLUTE_RUN_DIRECTORY

キャンペーンZIPは統合後の成功したBT実行記録を必須とする。
source/main SHAの一致、通常クリアの生ログ、終了コード、stderr、画面ハッシュ、EXE・content・全shaderのハッシュを照合する。
未マージ版の記録や、テスト後に差し替えたEXEは拒否する。BUILD_INFO.jsonには照合した検証結果を入れる。
