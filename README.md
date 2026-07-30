# KonbiniDominant

「コンビニチェーンが異常発達した架空都市 N-KXi を侵略し、最後まで存在を
残す」インクリメンタル都市支配ゲームの設計リポジトリ。

実装スタックは Unity ではなく C++20 / CMake とし、次の責務分担を採用する。

- **Pictor** — 描画基盤
- **Ergo** — 入力・フレーム・共通ゲームランタイム
- **Figmentum** — N-KXi と各次元の都市・施設形状生成
- **KonbiniDominant** — Data-oriented Design (DoD) のゲームシミュレーションと
  各基盤への adapter

Unity DOTS という製品/APIへの依存は持たない。元資料の「DOTS」は、連続データ、
stable ID、system pass、tick 境界の deferred command といった DoD 要件として
読み替える。

現在の成果物は実装前の仕様一式。入口は [spec/README.md](spec/README.md)、
全体設計は [spec/design.md](spec/design.md) を参照。

## Source

- [コンビニドミナント](https://app.notion.com/p/31a39cbfbab980f582dbed1f5da4b5ec)
- [コンビニドミナント - 最適化/高速化/DOTS](https://app.notion.com/p/DOTS-31a39cbfbab980e88471e30fd0de0cbd)

今回の指示で、技術メモ中の Unity は Pictor / Ergo に、DOTS は DoD に
置き換え、都市形成の正本を Figmentum とした。

## Status

仕様策定段階。実装、ビルド、テスト、ゲーム起動はまだ行っていない。

ライセンスは未決定のため、この初期リポジトリには追加していない。
