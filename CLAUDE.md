# KonbiniDominant — 作業ルール

## プロジェクト概要

Pictor / Ergo / Figmentum を用いる C++20 の新規ゲーム。
Unity は使用せず、Unity DOTS の意図は engine-neutral な Data-oriented Design
(DoD) として実装する。

## 正本と優先順位

1. ユーザ / neco の最新指示
2. `spec/` の確定仕様
3. Notion「コンビニドミナント」
4. Notion「コンビニドミナント - 最適化/高速化/DOTS」

曖昧な原案を黙って確定値へ変えない。`BASE-*` は実装可能にするための暫定解釈、
`TBD-*` は未決事項として識別する。

## アーキテクチャ不変条件

- `sim/` は Pictor、Vulkan、Ergo render、Figmentum に依存しない。
- hot path は stable integer ID + flat array / SoA を基本とし、entity ごとの
  heap allocation、pointer chase、mass entity の Actor 継承を避ける。
- 構造変更は command buffer に積み、fixed tick 境界でまとめて適用する。
- Figmentum は都市の seed / recipe / geometry を所有する。ゲーム側は
  `CityManifest` の stable ID と gameplay delta を所有する。
- Pictor は simulation の読み取り専用 `RenderSnapshot` だけを消費する。
- Pictor は最下層。game domain から Vulkan handle を直接扱わない。
- Ergo の game-specific editor はこのリポジトリの plugin pack に置く。
- `GameManager` のような複数責務クラスを作らず、system / adapter / storage を
  責務ごとに分ける。

## spec

AIFormat `FORMAT_SPEC.md` の標準分類を使う。恒久仕様は
`data / feature / interface / setup / test`、実装計画は `plan`、
調査・設計背景は `faq` に置く。1機能1ファイルを守る。

## Git

初期 bootstrap の `main` push のみユーザ承認済み。以後は
`feat/`、`fix/`、`docs/` 等の短命 branch + PR を必須とし、main へ直接 push
しない。

## 実行

ビルド、テスト、ゲーム起動、サービス起動は、セッションのユーザ指示と
Cc policy に従う。実行していない検証を成功済みと記載しない。
