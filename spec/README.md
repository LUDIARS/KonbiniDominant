# spec/

KonbiniDominant の設計仕様。AIFormat `FORMAT_SPEC.md` の標準分類を基本とする。

## 読む順序

1. [design.md](design.md) — 採用アーキテクチャ、DoD、基盤の責務境界
2. [feature/game-flow.md](feature/game-flow.md) — 画面と Phase 状態遷移
3. [data/world-state.md](data/world-state.md) — runtime state と system tick
4. [interface/figmentum-city-generation.md](interface/figmentum-city-generation.md)
5. [interface/pictor-rendering.md](interface/pictor-rendering.md)
6. [interface/ergo-runtime.md](interface/ergo-runtime.md)
7. [faq/open-questions.md](faq/open-questions.md) — 実装前に残る判断

## 分類

- [data/](data/) — runtime / content / save のデータ契約
- [feature/](feature/) — プレイヤーから見たゲーム機能
- [interface/](interface/) — Pictor / Ergo / Figmentum との境界
- [plan/](plan/) — 実装計画と委託可能な作業単位
- [setup/](setup/) — native 開発環境
- [test/](test/) — 将来の検証戦略
- [faq/](faq/) — 原資料の分析、設計背景、未決事項

`knowledge/` は問題・障害が実際に発生した時点で追加する。

## 記法

- `REQ-*`: 原資料または今回指示から確定した要求
- `DEC-*`: この spec で確定した設計判断
- `BASE-*`: 原資料の空白を埋める暫定 baseline。変更可能
- `TBD-*`: owner の判断または計測が必要な未決事項

確定仕様と暫定解釈を混ぜない。数値が未定の場合は、仮のマジックナンバーではなく
設定キー名と acceptance condition を記載する。

最初の実装単位は
[plan/tasks/first-playable.md](plan/tasks/first-playable.md) に固定する。
