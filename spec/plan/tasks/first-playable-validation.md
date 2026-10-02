---
task_id: KD-FP-002
title: First playable post-merge validation
status: done
blocked_by: [KD-FP-001]
actio: actio:3bef8134-5536-47f6-a63f-8464ec4311a6
---

# KD-FP-002 — First playable post-merge validation

## Outcome

KD-FP-001 merge後の `main` をプロジェクト本体folderからExcubitor経由で起動し、
最初の操作可能版が実画面で成立することを確認する運用task。

## Preconditions

- KD-FG-001とKD-FP-001がreview済みでmergeされている
- `E:\Document\Ars\KonbiniDominant` がcleanな`main`で、`origin/main`と一致
- Ccへ実checkout branch `main` を登録済み
- Excubitorがrepo直下のservice `konbini-dominant-app` を認識

## Procedure

1. Concordiaの現在のtesting claimを確認
2. `konbini-dominant-app` をこのsessionでclaim
3. Excubitor経由でstartし、catalogのRelease buildを完了
4. process healthとnative windowを確認
5. 下記smoke scenarioを手動実行
6. Excubitor経由でstop
7. 成否にかかわらずtesting claimをrelease

worktree、直接exe、`cmake --build`後の直接起動は使用しない。

## Smoke acceptance

1. native windowに街が表示される。現行のPhase 1はグリッドの街
   (`city::isGridTown`) で、施設meshを持たない
   (`src/adapters/pictor/world_geometry_loader.cpp` の `isGridTown` 分岐、
   起動logは `uploaded 0 facility meshes`)。Phase 1ではグリッドの街と
   他chainのstore・ZOCが表示されること。グリッドの街でない都市のmodeでは、
   Figmentum `CityPlan` 由来の一区画 (facility mesh) が表示されること
2. 起動時のchain選択cardをtap / clickしてchainを選択できる。keyboardの
   `1` / `2` / `3` でも選択できる (`src/adapters/ergo/input_action_map.cpp`)
3. 配置先 (Phase 1はグリッドの空きマス、それ以外のmodeはfacility) を選び、
   資金が足りる場合だけstoreを置ける
4. placement後にcashとstore countが更新される
5. fixed tick後にpopulation由来の収益が反映される
6. storeとZOCがfacilityや他chainと異なる表示になる
7. camera操作 (`ZOOM -` / `ZOOM +`、drag)、操作表示のtoggle
   (`HELP` ⇔ `HIDE HELP`)、facilityを選択するmodeでの選択解除 (`DESELECT`)
   が機能する。`DESELECT` はfacilityを選択しているときだけ有効
   (`src/app/pointer_controls.cpp` の `PointerAction::Cancel`)。グリッドの街の
   Phase 1ではmouse clickで空きマスの選択と配置が同時に行われる
   (`src/app/game_session.cpp` の `tapPlacementFor` / `TapPlacement::PlaceImmediately`)
   ため、desktopのPhase 1では選択解除の操作は発生しない。touchのtapは選択だけを
   行い (`TapPlacement::SelectOnly`)、`PLACE` で確定するので、選択中は
   `DESELECT` が有効になる
8. windowを閉じるとprocessが正常終了する
9. validation error、Vulkan error、resource leak警告がlogにない

これはstartup smokeであり、unit / integration testは実行しない。

## Failure handling

- `main`へ直接修正しない
- log、再現手順、期待値 / 実際値をproblem logへ記録
- 修正は新しいtask branch / PRへ分離
- 起動失敗でもtesting claimを必ずrelease

## 結果

2026-10-02に `main` `6c38947` で実施し、9項目すべて合格した。詳細は
[../../test/first-playable-smoke-results.md](../../test/first-playable-smoke-results.md)。

- 環境: Windows 11 / NVIDIA GeForce GTX 1070 / Vulkan、Excubitor経由、
  プロジェクト本体folder、Cc testing claim 1654 / 1655 / 1660
- 前処理: `build/CMakeCache.txt` が別worktree由来でconfigureに失敗したため、
  `build/CMakeCache.txt` と `build/CMakeFiles` を削除して再生成した
- 未確認: 資金不足時の配置拒否 (smoke 3の後半)
