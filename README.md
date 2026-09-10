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

## Grid town and 720p UI

Phase 1はビルのない16×16の地面グリッド。空きマスを1回クリック／タップして正方形のコンビニを置く。
同じチェーンを辺で隣接させると外観と看板がつながる。既定1280×720の小型UIで、Roboto Monoの輪郭をPictorからベクタ描画する。
[変更仕様](spec/feature/grid-town-and-vector-ui.md)を参照。

## Phase 1–4 campaign and skills

既定の content v4 は、都市支配 → 256階の縦積み → 別次元攻略 → アイオーン戦を接続する。
最初に選ぶチェーンは1種類だけ。アイオーン登場前に5秒のWARNINGが出る。
集客で経験値を稼ぎ、最大3択の強化を選ぶ。スキルは11種、所持6種まで、各5段階。
選択中はゲーム時間が止まる。上底テクニックは売上と信仰の交換、怪音波は一時的な顧客奪取。

[キャンペーン仕様](spec/feature/full-campaign-baseline.md)、
[スキル仕様](spec/feature/skill-upgrades.md)、
[日本語の遊び方](data/distribution/README-ja.txt) を参照。
v3のPhase 1専用設定は data/content/phase1.json に保存してある。
Windows Releaseの起動・100倍速クリアは確認済み。統合後の受入判定は[テスト要件](spec/test/campaign-release-acceptance.md)に従う。

ZIP作成: python tools/package_campaign.py --build-dir build-native --revision FULL_COMMIT_SHA --playtest-run ABSOLUTE_RUN_DIRECTORY

## Controls

Original storefronts: [three-brand design and Pictor captures](spec/feature/three-store-brands.md).

マウスまたはWindowsタッチ画面だけで、チェーン選択から再挑戦まで操作できる。
[操作仕様](spec/feature/pointer-controls.md)と[日本語の遊び方](data/distribution/README-ja.txt)を参照。

| 入力／画面ボタン | 動作 |
|---|---|
| カードをクリック／タップ | チェーン・強化スキルを選択 |
| 区画をクリック／タップ → BUILD | 出店（同じ区画の再クリック／再タップも対応） |
| BUILD長押し | Phase 2以降の連続建設。成功後は次の空き階へ |
| 地図を左ドラッグ／1本指ドラッグ | カメラ移動 |
| ホイール／2本指ピンチ／ZOOM - / + | 拡大・縮小 |
| FLOORS | 階移動・次の空き階・地上へ |
| ACTIONS | イメージ戦略・次元切替・反転・逃走 |
| DESELECT / PAUSE / HELP | 選択解除・一時停止・説明表示 |
| PLAY AGAIN | 結果画面からチェーン選択へ戻る |
| ウィンドウを閉じる | 正常終了 |

ボタンは離した時に確定し、UI領域への操作は背後の区画へ通さない。
DPIと画面幅へ追従する。旧キーボードショートカットは補助として保持する。
Windowsの起動と100倍速クリアを確認済み。Android/iOSの接続コードは追加済みだが、両OSのビルド・実機タッチは未確認。
[モバイル構成](spec/setup/mobile-native.md)と[LLM不要のBTテスト](spec/test/no-llm-native-playtest.md)を参照。

## Source

- [コンビニドミナント](https://app.notion.com/p/31a39cbfbab980f582dbed1f5da4b5ec)
- [コンビニドミナント - 最適化/高速化/DOTS](https://app.notion.com/p/DOTS-31a39cbfbab980e88471e30fd0de0cbd)

今回の指示で、技術メモ中の Unity は Pictor / Ergo に、DOTS は DoD に
置き換え、都市形成の正本を Figmentum とした。

## Status

決定的simulation / city model、Figmentum city adapter、CPU render domain、
Pictorのper-flight world targetとswapchain composite、Ergo input / frame を
つないだ native first playable (`konbini_dominant`) まで実装済み。

起動・クリアの反復検証は[LLM不要のBTテスト](spec/test/no-llm-native-playtest.md)で行う。
テストEXEの直接起動はユーザー承認済み。Cc通知と本体フォルダ制限は維持する。

ライセンスは未決定のため、この初期リポジトリには追加していない。
