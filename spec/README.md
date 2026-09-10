# spec/

KonbiniDominant の設計仕様。AIFormat `FORMAT_SPEC.md` の標準分類を基本とする。

## 読む順序

1. [design.md](design.md) — 採用アーキテクチャ、DoD、基盤の責務境界
2. [feature/game-flow.md](feature/game-flow.md) — 画面と Phase 状態遷移
3. [feature/full-campaign-baseline.md](feature/full-campaign-baseline.md) — Phase 1–4 の実装 baseline
4. [data/world-state.md](data/world-state.md) — runtime state と system tick
5. [interface/figmentum-city-generation.md](interface/figmentum-city-generation.md)
6. [interface/pictor-rendering.md](interface/pictor-rendering.md)
7. [interface/visia-presentation.md](interface/visia-presentation.md)
8. [interface/ergo-runtime.md](interface/ergo-runtime.md)
9. [interface/mobile-platform.md](interface/mobile-platform.md)
10. [setup/mobile-development.md](setup/mobile-development.md)
11. [setup/mobile-native.md](setup/mobile-native.md) — 実装済み host と未検証項目
12. [faq/open-questions.md](faq/open-questions.md) — 実装前に残る判断

## 分類

- [data/](data/) — runtime / content / save のデータ契約
- [feature/](feature/) — プレイヤーから見たゲーム機能
- [interface/](interface/) — Pictor / Ergo / Figmentum との境界
- [plan/](plan/) — 実装計画と委託可能な作業単位
- [plan/problem_logs/](plan/problem_logs/) — 実際に発生した問題・障害の記録と
  upstream への修正要求
- [setup/](setup/) — native 開発環境
- [test/](test/) — 将来の検証戦略
- [faq/](faq/) — 原資料の分析、設計背景、未決事項
- [tasks/](tasks/) — task-workflow 2.1形式の作業単位。`pending` は残作業、
  `done` は完了した実装 / 設計結果を記録する

個別の問題・障害は [plan/problem_logs/](plan/problem_logs/) に1件1ファイルで残す。
`knowledge/` は、そこから再発防止の恒久知見を切り出す必要が出た時点で追加する。

## 記法

- `REQ-*`: 原資料または今回指示から確定した要求
- `DEC-*`: この spec で確定した設計判断
- `BASE-*`: 原資料の空白を埋める暫定 baseline。変更可能
- `TBD-*`: owner の判断または計測が必要な未決事項

確定仕様と暫定解釈を混ぜない。数値が未定の場合は、仮のマジックナンバーではなく
設定キー名と acceptance condition を記載する。

最初の実装単位は
[plan/tasks/first-playable.md](plan/tasks/first-playable.md) に固定する。
