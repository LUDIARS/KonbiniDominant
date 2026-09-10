# KD マウス／タッチ操作

要求: 「マウス/タッチ操作にして」。既存のPhase 1–4、スキル、WARNINGとゲームループを保ち、入力を画面上で完結させる。

作業場所: KonbiniDominant/build-pointer-worktree、branch feat/pointer-controls。
ローカルmain起点の専用worktree。既存実装への依存として29f1d19と266004bを取り込んだ。
共有checkoutと既存の作業branchは編集していない。

## 実装

- [x] チェーン、スキル、結果画面の大きな選択カード。
- [x] 建設、長押し縦積み、階移動、次元切替、反転、イメージ戦略、逃走の画面ボタン。
- [x] 一時停止、説明表示、選択解除、再挑戦。
- [x] マウス／1本指ドラッグ、2本指ピンチ、ズームボタン。
- [x] UI占有、解放時決定、ドラッグ終了時の誤配置防止。
- [x] Windowsネイティブ接触、昇格マウス抑止、DPI座標変換、終了時解放。
- [x] 建設成功後に次の空き階を表示。
- [x] 日本語配布説明と操作仕様を更新。
- [x] Windows x64 Releaseビルド成功（KONBINI_BUILD_TESTS=OFF）。
- [x] git diff --check。

ビルド記録: build-native/build-pointer-release.log（生成物のためgit対象外）。
コミット後に対応するrevisionのZIPを作成して共有し、Revisor local PRを提出する。
ZIPの容量・配送状況とPR番号は最終報告に記録する。

単体／統合／起動テストはユーザー未指示のため実行しない。
配布後のDiscord到着はAPI受付とは別に記録で確認し、確認できなければ未確認と報告する。
PR作成後に停止。merge、auto-merge、main更新は行わない。
