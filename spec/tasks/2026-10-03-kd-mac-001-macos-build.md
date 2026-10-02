---
task: kd-mac-001-macos-build
project: KonbiniDominant
kind: 実装
created: 2026-10-03
memory_links:
  - spec/setup/macos-development.md
  - spec/setup/native-development.md
  - spec/setup/mobile-development.md
  - spec/interface/figmentum-city-generation.md
---

# KD-MAC-001 — macOS (Apple Silicon / Apple clang) build and test

Actio: `actio:84f04cd4-bbba-431a-854a-84aaaad428ed`

## 目的

KonbiniDominantをmacOS (Apple Silicon、Apple clang、MoltenVK) でconfigure /
build / `ctest`できるようにし、手順を
[macos-development](../setup/macos-development.md) に記録する。Macでの初回
buildは次の3件で止まっていた。

1. KD `FacilityGeometryCacheKey`のdefaulted `operator<=>`が暗黙に削除される
2. Pictor `src/visus/visus_json.cpp`の浮動小数点`std::from_chars`
3. Figmentum `src/garment/profile.cpp`の浮動小数点`std::from_chars`

2と3は上流で修正・merge済み (Pictor #2309 `502ba022`、Figmentum #2308
`ff09a65d`)。上流repositoryは編集しない。

## 正本

- [macos-development.md](../setup/macos-development.md)
- [native-development.md](../setup/native-development.md) Dependency revisions
- [mobile-development.md](../setup/mobile-development.md) NDK libc++ compatibility
- [figmentum-city-generation.md](../interface/figmentum-city-generation.md) Geometry generation

## 完了条件

- [x] `FacilityGeometryCacheKey`はdefaulted `==`と、宣言順の明示的な辞書式
      `operator<`を持つ。`std::string`の三方比較にも`std::tuple`の比較にも
      依存しない (Apple clang 14のlibc++は`std::string`の`<=>`を持たない)。
      Windowsでの`std::map`順序は宣言順の比較で変わらない
- [x] `KONBINI_PICTOR_REVISION`を`502ba022b66fbba0356b820fc17a064abe37435e`、
      `KONBINI_FIGMENTUM_REVISION`と`city::kFigmentumRevision`を
      `ff09a65db1a1db6711537a7ce49f207ca068c8b5`へ更新した。exact-source検証は
      既存の`konbini_verify_exact_git_source`のまま
- [x] `konbini_dependency_pin_contract_tests`をFigmentumへ拡張した: CMake pin
      (Pictor / Ergo / Figmentum) とsetup spec / READMEの表、Figmentumの
      exact-source配線、`city::kFigmentumRevision`の一致を検査する
- [x] Android専用shim (`mobile/android/compat/libcxx_float_from_chars.*`と
      `mobile/CMakeLists.txt`の`-include`) を削除した。Android
      `:app:assembleDebug`とAPK検査が通る。再導入しない契約を
      `konbini_portable_libcxx_contract_tests`で検査する
- [x] `spec/setup/macos-development.md`を追加した
- [x] Windows x64 Debug、macOS Release (Apple M1) でbuildと`ctest`全件が通る
- [x] `spec/domains/platform-adapters.domain.json`のspecRefsと
      `cc.acceptance.json`のsource ↔ testを更新した

## 判断

- 比較の直し方: メンバー`std::string generatorRevision`が`<=>`を持たない
  libc++でdefaulted `<=>`が削除される。`std::tie`経由の比較はC++20で
  synth-three-wayへ流れるためlibc++の版差を受けうるので、各メンバーの`!=` / `<`
  だけを使う手書きの辞書式比較にした。順序は宣言順で、defaulted `<=>`と同じ。
  最初の`std::tie`を返すprivate helper (戻り値型推論) はNDK clangが
  「定義前の使用」として拒否したため採らなかった (MSVCは通していた)
- shimの要否: pinned Pictor `502ba022`は`pictor::float_parse`、Figmentum
  `ff09a65d`は`fg::parseFloat`を使い、KDがbuildする上流sourceから浮動小数点
  `std::from_chars`は無くなった (Figmentumの`app/monster_lab`は
  `EXCLUDE_FROM_ALL`でbuildしない)。KDの`json_document.cpp`は
  `__cpp_lib_to_chars`が無い環境でclassic locale streamを使い、他の
  `std::from_chars`呼び出しは整数だけ。Android buildで確認して削除した
- pin更新に伴うgeometry影響: Figmentum `d0437cd5..ff09a65d`の差分は
  monster_creator、garment profileの数値parse、`io/parse_float`で、
  `planCity()` / city polygonizeは不変。manifest / cache keyの
  `generatorRevision`だけが変わり、hashはruntime計算なのでgolden値の更新は無い
- `spec/setup/native-development.md`のFigmentum行は`3ee998f4`のまま古く、
  READMEとCMake (`d0437cd5`) とずれていた。pin契約をFigmentumへ広げて同時に直した

## 検証記録 (2026-10-03)

| host | 対象commit | configure / build | ctest |
|---|---|---|---|
| Windows 11 x64、Visual Studio 17 2022 | 作業tree (最終commitと同じsource) | Debug、成功 | 34/34 passed |
| macOS 26.6.2 / Apple M1、Apple clang 14.0.3、Ninja | `e2b3de2` (bundle → `~/LUDIARS/.kd-verify-macos`) | Release、成功 (615/615) | 34/34 passed |
| Android arm64-v8a、NDK r27c、JDK 17 | 作業tree | `gradlew.bat --no-daemon :app:assembleDebug` 成功、`verifyKonbiniDebugStagedAssets` / `verifyKonbiniDebugApk`通過 | — |

`e2b3de2`以後のcommitはspec / task mdだけで、build対象sourceは変わらない。
Macでは`PATH`に`/opt/homebrew/bin`を足し、FigmentumはGit URL読み替えの
環境変数だけでSSH取得した。Mac上の`~/LUDIARS/KonbiniDominant`には触れていない。

## 範囲外

- アプリの起動、MoltenVK上の実描画、window / input確認 (未実施)
- iOS build (Xcode本体の選択が必要。[mobile-native](../setup/mobile-native.md))
- macOS build時の既存警告 (`-Wswitch`、`-Wmissing-field-initializers`) の解消

## スコープ (編集可ディレクトリ)

- `include/konbini/city/`、`src/adapters/figmentum/`、`CMakeLists.txt`、`mobile/`
- `tests/city/`、`tests/adapters/`、`tests/cmake/`、`tests/CMakeLists.txt`
- `spec/setup/`、`spec/interface/`、`spec/design.md`、`spec/domains/`、
  `spec/tasks/`、`README.md`、`data/content/README.md`、`cc.acceptance.json`
