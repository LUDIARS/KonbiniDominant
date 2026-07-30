---
task_id: KD-FP-002
title: First playable post-merge validation
status: blocked
blocked_by: [KD-FP-001]
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

1. native windowにFigmentum `CityPlan`由来の一区画が表示される
2. `1` / `2` / `3` でchainを選択できる
3. facilityを選択し、資金が足りる場合だけstoreへ置換できる
4. placement後にcashとstore countが更新される
5. fixed tick後にpopulation由来の収益が反映される
6. storeとZOCがfacilityとは異なる表示になる
7. camera操作、選択解除、操作表示toggleが機能する
8. windowを閉じるとprocessが正常終了する
9. validation error、Vulkan error、resource leak警告がlogにない

これはstartup smokeであり、unit / integration testは実行しない。

## Failure handling

- `main`へ直接修正しない
- log、再現手順、期待値 / 実際値をproblem logへ記録
- 修正は新しいtask branch / PRへ分離
- 起動失敗でもtesting claimを必ずrelease
