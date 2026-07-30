# Phase 3 — 4次元的侵略

## 目的

別次元へ出店し、相手次元のstoreをanti-storeへ反転させ、同一位置で対消滅させる。

## Dimension生成

REQ-P3-DIM-01: random生成された別次元へ出店できる。

Phase 3開始時に攻略対象のforeign dimensionを2つ生成する。

```text
dimensionSeed = derive(worldSeed, "dimension", ordinal, seedDerivationVersion)
```

FigmentumはseedごとにCityManifest / geometryを生成する。gameplayの同一位置判定を
成立させるため、各dimensionは次のどちらかを満たす必要がある。

1. 同型のlot graphを共有し見た目だけ変える
2. `CrossDimensionAnchorMap`で対応lotを明示する

選択は `TBD-DIM-COORD-01`。座標の近似一致だけで対消滅させない。

## 次元移動 / 出店

playerは表示dimensionを切り替え、通常のplacement commandにtarget dimensionを指定する。
移動cost、同時active dimension数、off-screen simulation frequencyは `TBD-DIM-UI-01`。
off-screenでもrule結果は省略せず、同じfixed tickで処理する。

## 反転衝動

REQ-P3-INVERT-01:

- Phase 3でunlock
- rival dimension storeのfaithが最大値の時、anti-storeへ変換可能

BASE-P3-INVERT-01:

- command対象はrival store
- `faith == faithMax`をtick境界で検証
- ownerは元chainのまま、`AntiStore` flagを付ける
- 同じstoreを二重反転できない

cost、cooldown、誰が操作できるかは `TBD-INVERT-*`。

## 対消滅

REQ-P3-ANNIHILATE-01:

- anti-storeと同じpositionのplayer-origin dimensionに自storeが存在
- 両者が「出会った瞬間」に対消滅
- convenience / inconvenience energyへ変換

BASE-P3-ANNIHILATE-01:

1. CrossDimension位置keyが一致したpairをtickで検出
2. 両storeを`PendingAnnihilation`
3. 同tick終端で両storeをdestroy
4. player dimensionへConvenience modifier
5. rival dimensionへCollapse command

presentation animationはeventを遅延表示してよいが、simulation resultを待たせない。

## Energy

REQ-P3-ENERGY-01:

- convenience energy: 自次元の収益・信仰度上昇buff
- inconvenience energy: 相手次元を消滅させる

量、duration、stacking、collapse delayは `TBD-ENERGY-*`。
dimension消滅時はstore / population / Triangle / modifierを参照順序に従って破棄し、
stale IDを残さない。

## Phase進行

foreign dimensionを2つ消滅させると全次元支配となり、boss「アイオーン」が出現する。
Boss逃走用の追加dimensionはこの2つとは別で、必要時にordinalを増やして生成する。

## 不変条件

- dimension seed derivationはversion付きで再現可能
- Destroyed dimensionへcommandを送れない
- anti-store変換と対消滅は同じstoreへ1回だけ
- dimension collapse後にRenderSnapshotへobjectを残さない
- 同位置判定は明示mappingに基づき、float epsilonだけに依存しない
