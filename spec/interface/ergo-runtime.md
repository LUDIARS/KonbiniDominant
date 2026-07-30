# Ergo runtime contract

## 目的

Ergoから入力、frame clock、render host、UI/audio等の共通機能を利用し、
game-specific ruleやmass entity storageをErgoへ持ち込まない。

調査対象: `LUDIARS/Ergo@771b027f0e5492015b27f54c3bab1fd5c1ae4790`

## 利用候補module

| module | 用途 | 制約 |
|---|---|---|
| `ergo_input` | command入力 | OS pollは現状no-op、adapter必須 |
| `ergo_frame` | frame count / dt / FPS | simulation fixed tickとは分離 |
| `ergo_render` | Pictor frame orchestration | game固有GPU bridgeは本作側 |
| `ergo_ui` | headless UI bitmap / frame部品 | UI system全体ではない |
| `ergo_audio` / `ergo_sound` | SE / BGM候補 | backendを明示選択、silent dummy禁止 |
| `ergo_world_time` | presentation time scale候補 | simulation tickの正本にしない |
| `ergo_bind` | 開発時tuning | production gameplay stateの書換境界を限定 |
| `ergo_log` | runtime log | game event streamと混ぜない |

実装時は必要moduleだけenableする。

## Input adapter

現行`ergo_input`のplatform `poll()`はno-op。Pictorの
`GlfwSurfaceProvider::glfw_window()`へcallbackを登録し、Ergoのinject APIへ変換する。

```text
GLFW callback
  → ErgoInputAdapter
  → ergo_input double buffer
  → InputActionMap
  → PlayerCommand
```

adapterの責務:

- key / mouse button / pointer / scroll callback
- window座標→world rayはcamera/picking層へ委譲
- frame開始時のbuffer swap
- input action remap
- UI capture時にgame commandを抑止
- focus loss時にstuck keyを残さない

raw callbackからsimulation stateを直接変更しない。

## Frame / simulation

render frameの`dt`をsimulationへ直接積算せず、fixed-step accumulatorをapp層に置く。

```text
poll input
→ frame tick
→ accumulator += clamped render dt
→ while accumulator >= fixedDt:
     map commands
     simulation.tick()
→ publish/interpolate RenderSnapshot
→ render
```

catch-up上限とpause時の扱いは `TBD-RUNTIME-01`。tick dropが必要な場合はlogし、
結果をsilentに変えない。

## Render host

`ergo_render`はPictorの上でframe lifecycleを共通化するが、game-specificな
drawable変換、GPU asset store、batch sourceは本作側。

origin/main調査では`FrameComposer`のlayer initializationとrender pass設定順、
`StageRenderer`のpipeline作成順にcustom passでの整合リスクがある。
実装時に次をgateとする。

1. default passでlifecycleを確認
2. render passをpipeline作成前に確定できるcontractへ修正
3. PictorFrameBridgeのHDR / overlay passを統合

問題をadapterの呼出順偶然で隠さない。必要ならErgoへupstream fixをPRする。

## Ergoを使わない領域

- `ergo_actor`: store / facility / population cellごとに作らない
- `ergo_scene`: runtime worldの正本にしない。look-dev document用途のみ
- `ergo_blackboard`: high-frequency store stateを置かない
- `ergo_physics2d`: ZOC / 256階 / dimension空間indexに使わない
- `ergo::math::ObjectPool<T>`: stable handleには利用可能だがcomponent SoAの代替にしない

DoD hot loopは本作のdense tablesを正本とする。

## UI

`ergo_ui`はRGBA bitmap生成等のbuilding blockとして使い、HUD stateは
simulationから生成した`HudViewModel`を入力にする。

- UI callback→PlayerCommand
- UIはcash / faith / phase等を直接書換えない
- animated bitmapを毎frameuploadする場合はGPU costを計測
- 256階 / dimension UIはgame-specific widgetとして本作側に置く

## Audio

game event→audio cue mappingをdata化する。backend欠落時に自動Dummyへ落ちて
「成功扱い」にしない。headless testだけは明示configでDummyを選べる。

Figmentumのformula audioを使う場合も、生成とruntime playbackのownershipを
明示し、frame threadで重い生成を行わない。

## Tools

game-specific editor / tuning surfaceはhost repositoryのplugin packに置き、
`ERGO_PLUGIN_DIR`で`tools/ergo`へ読み込ませる。

候補:

- chain / economy content editor
- N-KXi recipe inspector
- ZOC / Triangle debugger
- dimension / save inspector

toolはsimulation interface越しに操作し、内部tableのpointerを公開しない。

## Failure

- required input backend不在
- Pictor/Vulkan実描画経路不成立
- required Ergo module無効
- content / plugin schema mismatch

これらは起動時に明示error。capabilityが必須なのにno-op moduleへ自動縮退しない。
