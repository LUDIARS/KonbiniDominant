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

simulation / city / Figmentum adapter、Pictor / Ergo の GPU composition
foundation、そして native executable `konbini_dominant` をbuildできる。

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

native app (Release) は Excubitor catalog と同じ command でbuildする。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DKONBINI_BUILD_TESTS=OFF
cmake --build build --config Release --target konbini_dominant
```

生成物と runtime asset:

| 種別 | 既定path | 上書き |
|---|---|---|
| executable | `build/Release/konbini_dominant.exe` | — |
| SPIR-V | `build/shaders/*.spv` (`konbini_shaders`) | `KONBINI_SHADER_DIR` |
| content | `build/data/content/first-playable.json` (`konbini_runtime_content`) | `KONBINI_CONTENT_FILE` |

shader / content は上方探索の fallback を持たない。欠けている場合は起動時に
明示的に失敗する。

起動は Excubitor service `konbini-dominant-app`
([excubitor.catalog.yaml](excubitor.catalog.yaml)) 経由で、プロジェクト本体
folder からのみ行う。

### Dependency revisions

| 依存 | pinned revision |
|---|---|
| Pictor | `c088e8d1b7b9e2625b7a8d923c89d4d684566c16` |
| Ergo | `771b027f0e5492015b27f54c3bab1fd5c1ae4790` |
| Figmentum | `3ee998f487d984f54003c4ec3c4f7ba00b53eec3` |

setup contract は [spec/setup/native-development.md](spec/setup/native-development.md)。

## Controls

| 入力 | 動作 |
|---|---|
| `1` / `2` / `3` | chain 選択 (ローサン / ファモマ / セバンイレバン) |
| left click | facility 選択、同じ有効候補を再clickで配置確定 |
| right click / `Esc` | 選択解除 |
| `WASD` / 中ボタンdrag | camera移動 |
| wheel | zoom |
| `F1` | 操作表示のtoggle |
| window close | 正常終了 |

## Source

- [コンビニドミナント](https://app.notion.com/p/31a39cbfbab980f582dbed1f5da4b5ec)
- [コンビニドミナント - 最適化/高速化/DOTS](https://app.notion.com/p/DOTS-31a39cbfbab980e88471e30fd0de0cbd)

今回の指示で、技術メモ中の Unity は Pictor / Ergo に、DOTS は DoD に
置き換え、都市形成の正本を Figmentum とした。

## Status

決定的simulation / city model、Figmentum city adapter、CPU render domain、
Pictorのper-flight world targetとswapchain composite、Ergo input / frame を
つないだ native first playable (`konbini_dominant`) まで実装済み。

実機での起動確認 (Excubitor経由のsmoke) は
[KD-FP-002](spec/plan/tasks/first-playable-validation.md) で行う。

ライセンスは未決定のため、この初期リポジトリには追加していない。
