# UI / UX

## 原則

- 都市俯瞰で、配置可否と支配関係をpointer移動中に理解できる
- 2D / vertical / dimensionの複雑化を同時表示しすぎない
- simulationの確定値とpreviewを視覚的に分ける
- parody textだけに依存せず、色・形・iconでもchainを識別できる

## Title / Chain Select

Title:

- New Game
- Continue（valid saveがある時）
- Settings
- Quit

Chain Select card:

- chain名 / visual identity
- build cost傾向
- ZOC傾向
- 固有passive
- variant（ローサンのみ複数）
- 未確定content keyがあるbuildでは開始をfail-fastし、空欄のまま進めない

## Common HUD

- cash / 直近収益
- active store数
- population / customer share
- phase名 / timer / 進行条件
- selected chain / variant
- cursor lotのbuild costとvalidation結果
- ZOC / Triangle表示toggle
- simulation pause / speed（speed可否は `TBD-UX-SPEED-01`）

invalid placementは色だけでなくreason textを出す。

## Phase 1

- lot hoverで置換されるfacility、人口影響、costをpreview
- store placement ghost
- ZOC円とinfluence heatmap
- candidate / active Dominant Triangleを別style
- encircled rivalへ予兆表示

## Phase 2

- facility選択時にvertical stack columnを開く
- floor slider / jump
- 各slotのowner / faith / build cost
- image strategy対象とcooldown
- 256階progress

俯瞰cameraだけで819m towerを追わせず、選択stackへfocusできる。

## Phase 3

- dimension tab / map
- dimension state（Active / Collapsing / Destroyed）
- 同一position anchorの対応表示
- faith maxのinvert可能store
- anti-storeとmatching自storeを結ぶpreview
- convenience / inconvenience energyと残duration

off-screen dimensionで重大eventが起きた場合はqueue表示し、cameraを強制移動しない。

## Boss

- manifestation 30秒countdown
- boss spawn warning
- MaxValueの発火条件 / 予兆（条件確定後）
- escape先dimension生成status
- 全dimensionに残る自store数

巨大damage数値は演出表示できるが、HP barが存在するように誤解させない。

## Result

[game-flow.md](game-flow.md)の統計を表示し、New Game / Titleへ戻れる。
victory/loseの判定理由を明示する。

## Accessibility

- chain / ZOC / Triangleを色だけで区別しない
- UI scale
- camera motion / screen shake / flash強度
- countdownのvisual + audio cue
- input remap
- pause中にrule説明を読める

具体的なplatform input schemeとlocalizationは `TBD-ACCESS-01`。
