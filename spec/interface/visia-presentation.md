# Visia presentation contract

## 目的

VisiaはKonbiniDominantが所有する「何をどういう意味で見せるか」の定義である。
Pictor、Ergo、Vulkanのresource handleをgame domainへ持ち込まず、residentと
短命effectをprimitive dummyから将来assetへ交換できる境界にする。

## Naming boundary

Pictorには描画recipeの`Visus`が存在する。名称が似ていても責務は異なる。

| type | owner | responsibility |
|---|---|---|
| `VisiaDefinition` | KonbiniDominant | semantic visual IDとgame-owned primitive parameter |
| `VisusDesc` | Pictor | Pictor sceneへ登録する描画recipe |
| GPU mesh / material / object | Pictor adapter | upload、mapping、lifetime |

将来の`PictorVisiaResolver`はVisia IDをPictor resourceへ明示的に解決する。
未解決IDを別のplaceholderへsilent fallbackしない。

## First playable definitions

`VisiaId`は少なくとも次を持つ。

```text
ResidentPrimitive
StoreLandingEffectPrimitive
```

`ResidentPrimitive`はbodyとheadのbox寸法、local center、色を持つ。
head上端は吹き出しanchorの下限で、contentの`bubbleHeightMeters`が足元からの
最終anchor高を所有する。anchor高がhead上端以下なら明示errorにする。

`StoreLandingEffectPrimitive`はringの開始半径、終了半径、厚さ、segment数、色を
持つ。instanceの`normalizedAge`は閉区間`[0, 1]`で、半径とalphaを補間する。

definition、instance、色、寸法、角度、ageが非finiteまたは範囲外なら
`std::invalid_argument`として拒否する。unknown Visia IDをresident等へ
置き換えない。

## CPU primitive geometry

native Pictor bridge完成前もcontractを検証できるよう、game-owned
`WorldVertex`と32bit indexを生成する。

- resident boxはY-up、XZ yaw回転、外向きnormal、counter-clockwise triangle
- landing ringはXZ平面、上向きnormal、stable segment順
- 複数instanceはinput順を維持する
- vertex/index countの32bit overflowを生成前に拒否する
- geometry生成はPictor / Ergo / Vulkan headerへ依存しない

production adapterは同じgeometryをinstance uploadできる。CPU geometryの存在を
「画面表示統合済み」とは扱わない。

## Speech bubble geometry

inputはworld-space anchor、ASCII dummy text、hide distanceである。
`IsometricCamera`の`right`と`up`からcamera-facing planeを作り、background、tail、
5x7 glyph quadを同じworld vertex streamへ出す。camera forward方向へ小さくoffsetし、
backgroundとのz-fightingを避ける。

- 対応glyphは`A`〜`Z`とspace
- 未対応glyphを`?`へsilent fallbackせず、明示errorにする
- empty text、非finite anchor、非正のhide distanceを拒否する
- camera eyeとの平方距離がhide distance平方を超えるrequestはgeometryへ追加しない
- input順を維持し、distance cullingの結果だけを除外する

日本語font atlasへ移行した後も、world anchor、distance culling、stable speaker IDは
この境界を維持する。native frameの日本語bubbleは
[Japanese speech lines](../feature/npc-conversations-and-placement-feedback.md#japanese-speech-lines)
と[Bubble culling](../feature/npc-conversations-and-placement-feedback.md#bubble-culling)
が正本で、この5x7 helperはCPU検証用に残る。

## Store placement sampler

samplerはtarget poseとcueからの経過秒を受ける純粋関数で、次を返す。

```text
StorePlacementSample
  positionMeters
  yawDegrees
  isComplete
  landingEffectInstance?
```

hold、fall、effectの時間とspawn height、rotationはvalidated spec値である。
fallは明示したeasing関数を使い、境界値で不連続を作らない。effect期間だけ
`StoreLandingEffectPrimitive` instanceを返し、期間後は返さない。

samplerは店舗のauthoritative transformを変更しない。rendererはsampleを店舗objectの
一時poseへ適用し、完了後はsnapshotのtarget poseへ戻す。
first playableの`RenderStorePlacementCue`はtarget positionと明示的な最終yaw 0度を
持ち、adapterが未定義の向きを補完しない。

## Pictor integration boundary

KD-NPC-001時点の想定接続は次の通り (KD-NPC-002の実装は後述)。

```text
RenderSnapshot residents[]
  -> Resident Visia instances
  -> CPU geometry / Pictor objects on the shared resident mesh

RenderSnapshot placementCues[]
  -> StorePlacementSample
  -> store transform override + landing Visia instance

resident speech
  -> distance culling
  -> dummy glyph geometry / shared Noto Sans JP glyph atlas
```

`buildResidentVisualGeometry`はsnapshot residentのradian yawをVisiaのdegree yawへ
明示変換し、人型primitiveと発話bubbleを同じ入力から生成する。これはCPU側の
配置までを閉じるhelperであり、GPU uploadやframe表示の完了を意味しない。

KD-NPC-001はgame-owned value、CPU geometry、animation samplerまでを実装した。
KD-NPC-002でnative frameへ接続した。

```text
ResidentPoseTracker (stable id, retarget blend)
  -> Resident draw: shared ResidentBody mesh + yaw/translation model
StoreConstructionVisual.animation.landingEffect
  -> LandingEffect draw: baked ring age frame (translucent)
cullSpeechBubbles (distance / off-screen / limit)
  -> appendSpeechBubbleDraws: background + tail + Noto Sans JP glyph draws
WorldDrawList::presentation
  -> PresentationObjectSync (pictor-rendering.md#Presentation objects)
```

共有meshは`buildPresentationMeshes()`がVisia定義から作る。residentの共有mesh +
modelは`buildResidentPrimitiveGeometry`のCPU結果と同じ頂点になる (testで照合)。
着地ringは内径と外径の拡大率が異なり剛体変換で表せないため、age 16 frameへbakeし、
instanceの`normalizedAge`に最も近いframeを使う (BASE-NPC-RING-FRAMES-01)。
未解決のVisia IDやframe外のageは例外で、別placeholderへ置き換えない。
