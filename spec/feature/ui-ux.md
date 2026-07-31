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

### Ambient residents and placement feedback

- ZOC内で店舗へ割り当てられたpopulation cellからresident dummyを表示する
- residentは施設中心と店舗中心を往復し、来店中だけ短い吹き出しを頭上に表示する
- 吹き出しはcameraから離れると非表示にし、都市俯瞰の可読性を優先する
- 店舗配置成功時は上空で短く溜め、270度回転しながらeasingで着地する
- 着地時はVisiaで定義した短命ring effectを表示する
- animation中もcash、店舗数、placement結果はsimulationの確定値を表示する

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

## Smartphone interaction

REQ-MOBILE-UX-01: mobile版はhover、right click、keyboard shortcutがなくても
全gameplay操作を完結できること。raw touchをsimulationへ直接渡さず、
gestureをsemanticなinput actionへ変換してから`PlayerCommand`を生成する。

first mobile baseline:

| gesture / UI | action |
|---|---|
| single tap | facility / UIを選択 |
| 選択後の明示`Place` action | store配置を確定 |
| drag | camera移動。tapとの判定thresholdを持つ |
| pinch | camera zoom |
| `Cancel` / platform back | 選択解除、上位screenへ戻る |
| pause action | simulation pauseとrule説明 |

- hover previewはselection previewへ置き換える
- accidental placementを避けるため、tapだけで即時購入しない
- safe area、display density、UI scaleをlayout inputとして扱う
- rotation / resize後もselected facilityとsimulation stateを失わない
- touch targetの最小値は対象OSのaccessibility guidelineに従い、実機taskで検証する
- landscapeをfirst mobile baselineとし、portrait対応は
  `TBD-MOBILE-ORIENTATION-01`で確定する

## Accessibility

- chain / ZOC / Triangleを色だけで区別しない
- UI scale
- camera motion / screen shake / flash強度
- countdownのvisual + audio cue
- input remap
- pause中にrule説明を読める

desktop / controllerの追加input schemeとlocalizationは `TBD-ACCESS-01`。
