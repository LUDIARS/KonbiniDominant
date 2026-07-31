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

成果物の中心は仕様一式。入口は [spec/README.md](spec/README.md)、
全体設計は [spec/design.md](spec/design.md) を参照。

## Build

simulation / city / Figmentum adapterに加え、Pictor / Ergoを使うGPU composition
foundationまでbuildできる。native game executableは後続stageで追加する。

既定構成はVulkan SDKと固定revisionの3依存を必要とする。Vulkan非依存の
headless reviewはrender / Figmentum adapterを明示的に無効化する。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DKONBINI_BUILD_RENDER=OFF -DKONBINI_BUILD_FIGMENTUM_ADAPTER=OFF `
  -DKONBINI_BUILD_TESTS=ON
cmake --build build --config Debug --target konbini_review
ctest --test-dir build -C Debug --output-on-failure
```

Visual Studio generator は multi-config なので、`--config` / `-C` を省くと
`ctest` が test を見つけられない。single-config generator (Ninja 等) を使う
場合のみ省略できる。

setup contract は [spec/setup/native-development.md](spec/setup/native-development.md)。

## Source

- [コンビニドミナント](https://app.notion.com/p/31a39cbfbab980f582dbed1f5da4b5ec)
- [コンビニドミナント - 最適化/高速化/DOTS](https://app.notion.com/p/DOTS-31a39cbfbab980e88471e30fd0de0cbd)

今回の指示で、技術メモ中の Unity は Pictor / Ergo に、DOTS は DoD に
置き換え、都市形成の正本を Figmentum とした。

## Status

決定的simulation / city model、Figmentum city adapter、CPU render domain、
Pictorのper-flight world targetとswapchain composite foundationまで実装済み。
ゲーム本体の起動経路はまだ無い。

ライセンスは未決定のため、この初期リポジトリには追加していない。
