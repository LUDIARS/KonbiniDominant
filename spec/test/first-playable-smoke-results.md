# First playable smoke 結果

[KD-FP-002](../plan/tasks/first-playable-validation.md) のstartup smokeを
プロジェクト本体から実行した記録。合格基準は同taskの `Smoke acceptance` を正とする。
unit / integration testは対象外。

## 2026-10-02

- source: `main` `6c38947`
- 環境: Windows 11 / NVIDIA GeForce GTX 1070 / Vulkan
- 起動: Excubitor経由、プロジェクト本体folder (`E:\Document\Ars\KonbiniDominant`)
- Cc testing claim: 1654 / 1655 / 1660 (いずれもrelease済み)
- Actio: `actio:3bef8134-5536-47f6-a63f-8464ec4311a6`

### 前処理

`build/CMakeCache.txt` が別worktree由来でconfigureに失敗した。
`build/CMakeCache.txt` と `build/CMakeFiles` を削除して再生成した。

### 結果

| # | 判定 | 観測 |
|---|------|------|
| 1 | ○ | グリッドの街と他chainのstore・ZOCを表示 (Phase 1、facility meshなし) |
| 2 | ○ | chain選択card (MOONPANTRY・DAYLARK) のclickで選択 |
| 3 | ○ | 空きマスへ配置。資金不足時の拒否は未確認 |
| 4 | ○ | CASH 5000→4839、STORES 0→1 |
| 5 | ○ | INCOME 420、XP加算でlevel upの選択が出る |
| 6 | ○ | 色とZOC ringで区別 |
| 7 | ○ | `ZOOM +` でzoom、`HELP` ⇔ `HIDE HELP` で操作説明の表示切替。選択解除はdesktopのPhase 1では発生しない (codeで確認) |
| 8 | ○ | `WM_CLOSE` で正常終了、Excubitor状態 `stopped` |
| 9 | ○ | stderr空、Vulkan・validation errorとleak警告なし |

### 未確認

- 資金不足時の配置拒否 (smoke 3の後半)
- touch端末での操作 (このrunはWindows desktopで実施)
