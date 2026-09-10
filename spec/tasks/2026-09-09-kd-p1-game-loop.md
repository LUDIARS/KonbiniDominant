---
task: kd-p1-game-loop
project: KonbiniDominant
kind: 実装
status: implemented
created: 2026-09-09
memory_links:
  - spec/feature/phase-1-game-loop.md
  - spec/feature/phase-1-dominant-triangle.md
---

# KD-P1 — Phase 1 のゲームループ

## ユーザー承認

2026-09-09 に「未実装のタスク全部実装してゲームループまでつないで」と指示。
3勢力、3秒包囲で自動破壊、敵全滅＋人口60%で勝利、5分で人口比較、
再建資金なし＋店舗ゼロで敗北、結果から再挑戦という暫定ルールを承認。
残る実装判断は委任された。参考テンポは Vampire Survivors。
Phase 2、他次元、Boss には進まない。

## 作業単位と対応

| ID | 作業 | 実装の入口 |
|---|---|---|
| KD-P1-01 | ルールと調整データ | phase1_content / content v3 |
| KD-P1-02 | 3勢力の初期資金と出店 | select_chain_system / placement_system |
| KD-P1-03 | 商圏争奪と人口・収益 | competitive_zoc_system / population_change_system |
| KD-P1-04 | Delaunay 三角形と収益強化 | dominant_triangle |
| KD-P1-05 | 包囲予告と同時破壊・跡地 | encirclement_system |
| KD-P1-06 | 敵の出店と時間経過による圧力 | opponent_ai_system |
| KD-P1-07 | 勝敗・停止・再挑戦 | phase1_outcome_system / SimulationHost |
| KD-P1-08 | 説明・HUD・地図・操作 | phase1_hud_text / phase1_overlay_geometry / AppRunner |

## 完了条件

- 5店舗分の資金で開始し、出店・顧客獲得・収益・再投資が循環する。
- 敵も同じ資金と配置検証を使う。外部入力で敵の command を偽装できない。
- 三角形形成、予告、破壊、跡地への再出店がスナップショットへ反映される。
- 収益は同じ人口を二重計上せず、三角形強化も重複乗算しない。
- 敵の間隔は6秒から2秒へ、最初の4分間で短縮する。
- 終了条件で Result に入り、以後は進行・出店を停止する。
- R で都市 geometry を再利用し、command、資金、人口、店舗、包囲、時計を初期化する。
- Windows Release をビルドし、必要資産を含めた ZIP を元スレッドへ送る。
- テスト・ゲーム起動・サービス操作・マージは実行しない。

## 今回の境界

既存の3チェーンの建設費・商圏・収益差を用いる。原案の未定義な上げ底・
イレバン・商品 variant はこの8タスクへ混ぜず、共通ゲームループの完成を優先する。
NPC 表現の強化、モバイル版、セーブ／ロードは既存の別タスクを維持する。

## 実装・ビルド記録（2026-09-09）

KD-P1-01〜08 の実装を完了。Windows x64 Release の `konbini_dominant`
ビルドに成功。回帰確認コード `konbini_phase1_tests` はコンパイルのみ成功し、
実行していない。ゲーム起動、単体・統合テスト、サービス操作も行っていない。
Pictor 依存コードには既存のコンパイラ警告が残る。

配布物は `tools/package_phase1.py` で同梱資産・PE の DLL import・ZIP 内の
ハッシュを検査して作成する。EXE の import は Windows 標準 DLL と
`vulkan-1.dll` のみで、VC ランタイムの追加 DLL は不要。
遊んだ際の表示・バランスは未確認。

成果物のスレッド配送と Revisor 提出は実装後の手順。
Cc の `implement begin` は記録時点で `implementation_bind_failed` を返すため、
作業用 worktree の登録を確認してから提出する。登録不一致のまま
main を提出せず、配送結果・提出結果はセッションの最終報告で確定する。
