# マウス／タッチ操作 — BASE

2026-09-09。ユーザーの「マウス/タッチ操作にして」に対応する操作仕様。
既定の Phase 1–4 + 11種スキルを、キーボードを使わず開始から再挑戦まで操作する。
対象は Windows x64 ネイティブ版と Windows タッチ画面。Android / iOS / Web 移植ではない。

## 操作

| 場面 | マウス | タッチ |
|---|---|---|
| チェーン／スキル選択 | カードをクリック | カードをタップ |
| 区画選択 | 区画をクリック | 区画をタップ |
| 出店 | 選択後 BUILD、または同じ区画を再クリック | 選択後 BUILD、または同じ区画を再タップ |
| カメラ移動 | 地図を左ボタンでドラッグ | 地図を1本指でドラッグ |
| 拡大・縮小 | ホイール、ZOOM - / + | 2本指ピンチ、ZOOM - / + |
| 連続建設 | Phase 2以降で BUILD 長押し | 同左 |
| 階の変更 | FLOORS 内の FLOOR - / +、NEXT FREE、GROUND | 同左 |
| 次元・能力 | ACTIONS 内の IMAGE、NEXT WORLD、INVERT、ESCAPE | 同左 |
| 一時停止・説明 | PAUSE / RESUME、HELP / HIDE HELP | 同左 |
| 再挑戦 | 結果画面の PLAY AGAIN | 同左 |

最初に選んだ1種類のみ出店可能。強化カード表示中のシミュレーション停止は維持。
建設成功時、選択施設と建設階が現在の選択に一致すれば次の空き階へ移動する。
既存店舗の上に積み始める場合は FLOORS → NEXT FREE。
BUILD は押下後0.35秒で連続建設になり、既存の固定tick制限で1tickあたり最大1回要求する。
長押し後の指離しでは追加の単発建設を送らない。資金・土台・上限の最終判定はsimulationが行う。

## 入力の契約

- ボタンは同じボタン上で離した時に決定。無効ボタン、操作欄の余白、状態表示欄も接触を占有する。
- ボタン外へ移動した操作、複数指になった操作は決定しない。地図へ移っても出店しない。
- 地図操作は10 UI pixel以上動いたらドラッグ。ドラッグ・ピンチ終了は区画クリックに変換しない。
- 押下と解放が同じ描画フレームに入っても、押下位置と両端のイベントを保持する。
- フォーカス喪失、最小化、描画領域変更、フェーズ／強化候補／表示次元の変更で進行中のジェスチャーを破棄する。
- クライアント座標をframebuffer座標へ変換。描画と当たり判定は共通の画面モデルを使う。
- ボタンは通常高さ56 UI pixel。DPIと画面幅へ追従し、2〜4列で再配置する。極小画面では全体を縮小する。
- Windows側のタッチ→マウス昇格イベントを除外し、二重選択／二重出店を防ぐ。通常マウスとペンのマウス経路は保持する。
- 登録したWindowsサブクラスとタッチ登録を終了時に解除。処理したWM_TOUCHハンドルは異常時も閉じる。
- 既存ショートカットも補助として残す。ゲームの進行速度、出店費用、スキル効果は変更しない。

## 構成

NativeTouchBridge が Windows の接触を収集し、ErgoInputBridge がマウスと共通の PointerSample にまとめる。
InputActionMap が座標を正規化し、PointerInputController がドラッグ／ボタン／単発操作へ変換する。
PointerControls は配置・有効状態・説明欄の寸法、pointer_control_geometry は描画を担当する。
SelectionHud は選択施設の価格・状態の読取りを担当し、simulationを直接書き換えない。
Windows側の固定32接触配列では最初の2接触をカメラ操作に利用し、容量超過は明示エラーにする。

## 確認範囲

ReleaseコンパイルとZIP内のバイナリ・資産の整合性を確認する。
ユーザーのセッションポリシーに従い、単体／統合テスト、ゲーム起動、実機タッチ、表示プレビューは行わない。
実機の操作感・DPI切替・端末固有のタッチドライバー挙動は未確認。

## Windows API 根拠

タッチ登録・入力ハンドル管理は [Microsoft WM_TOUCH ガイド](https://learn.microsoft.com/en-us/windows/win32/wintouch/getting-started-with-multi-touch-messages)、
昇格イベントの識別は [Microsoft タッチ入力トラブルシューティング](https://learn.microsoft.com/en-us/windows/win32/wintouch/troubleshooting-applications)、
ウィンドウ接続は [Microsoft SetWindowSubclass](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-setwindowsubclass) に基づく。
