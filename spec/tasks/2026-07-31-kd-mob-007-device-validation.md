---
task: kd-mob-007-device-validation
project: KonbiniDominant
kind: テスト
status: pending
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 677
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-mob-005-android-package-integration.md
  - spec/tasks/2026-07-31-kd-mob-006-ios-package-integration.md
  - spec/interface/mobile-platform.md
  - spec/setup/mobile-development.md
  - spec/test/verification-strategy.md
---

# KD-MOB-007 — Smartphone actual-device validation

## 目的

review済みAndroid / iOS package candidateを実端末で確認し、
「スマホでも出来る」を
install、操作、復帰、決定性、性能の証跡で成立させる。

## 前提

- KD-MOB-005 / KD-MOB-006がmerge済み
- 対象exact SHAのpackageを再現可能にbuildできる
- 対象端末 / OS / GPU / graphics profileを記録できる
- Cc、Concordia、Excubitorのmobile deployment serviceが登録済み

## 手順

1. プロジェクト本体folderをreview済みbranch / SHAへ安全に切り替える
2. Concordia testing claimを取得する
3. Excubitor経由でpackage build / deploy / startする
4. Android / iOSで下記scenarioを実行する
5. TestWorkflow threadへ端末情報、SHA、結果、画像 / logを記録する
6. Excubitor経由でstop / cleanupする
7. 成否にかかわらずtesting claimをreleaseする

worktree、複製folder、直接binary起動を使用しない。

## Functional acceptance

- Figmentum `CityPlan`由来の一区画が表示される
- touchだけでchain選択、facility選択、store配置、cancel、camera操作ができる
- placement後にcash / store count / revenue / ZOCが更新される
- rotation / resize / safe area変化でUIとpickingがずれない
- background / resumeとsurface再生成でsimulation stateを失わない
- memory pressure時にauthoritative stateを破棄しない
- window / app終了後にGPU / native resource leak警告がない
- required capability欠落時は明示errorになる

## Determinism acceptance

- 同じworld seedと正規化済みcommand列をWindows / Android / iOSへ入力する
- canonical snapshot / hashが一致する
- render profile、display density、touch sample rateがgame resultへ影響しない

## Performance probe

first probe baseline:

- foreground rendering: 30 FPS
- simulation: 10 TPSを維持
- background / suspend: GPU submissionなし
- thermal / memory pressure: explicit profile変更とcache evictionを記録

端末tier別の製品budgetは計測結果から`TBD-MOBILE-PERF-01`を更新して確定する。
baseline未達を無断のgameplay削減で隠さない。

## Failure handling

- mainへ直接修正しない
- failureをproblem logへ端末 / SHA / reproduction付きで記録する
- platform固有fixを専用branch / PRへ分離する
- TestWorkflow証跡とtesting claim releaseを必ず完了する
