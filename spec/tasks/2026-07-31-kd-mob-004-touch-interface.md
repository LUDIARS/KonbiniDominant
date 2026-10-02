---
task: kd-mob-004-touch-interface
project: KonbiniDominant
kind: 実装
status: in_review
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 674
actio_task_id: "actio:69b59015-b775-42f0-9af2-9a6f40d1da45"
memory_links:
  - spec/tasks/2026-07-31-kd-mob-003-mobile-runtime-assets.md
  - spec/interface/mobile-platform.md
  - spec/feature/ui-ux.md
  - spec/interface/ergo-runtime.md
  - spec/tasks/2026-09-09-kd-pointer-controls.md
  - spec/feature/pointer-controls.md
value_ids:
  - UX-KD-W1
  - UX-KD-W4
---

# KD-MOB-004 — Touch input and responsive HUD

## 目的

hover、mouse、keyboardに依存せず、touchから既存のsemantic action /
`PlayerCommand`を生成してfirst playableを操作できるようにする。

## 完了条件

- native contactがfinger ID、phase、position、timestampを失わず正規化される
- tap / drag / pinch / cancelを責務別gesture recognizerで判定する
- drag開始後のcontactをtapとして確定しない
- app pause / contact cancel後にstuck pointerを残さない
- UI capture済みcontactからworld commandを生成しない
- tapでfacilityを選択し、明示`Place` actionでstore配置を確定する
- dragでcamera pan、pinchでzoomできる
- hover情報をselection stateから表示できる
- HUDがsafe area、display density、UI scaleへ追従する
- orientation / surface resize後もselectionとsimulation stateを保持する
- raw touch sample countやframe rateをgame resultへ使わない

## スコープ

- game-owned touch / gesture values
- Ergo input injection adapter
- input action mapping
- mobile HUD layout / placement confirmation
- interface / feature spec

OS Activity / ViewController、renderer surface、package、実機起動は含めない。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / PRを作成して停止する
- unit / integration / behavior / startup testは明示指示なしに実行しない

## 実装内容

Actio: `actio:69b59015-b775-42f0-9af2-9a6f40d1da45`。価値ID: `UX-KD-W1`
(hoverの代わりにselection previewで配置可否・costを示す)、`UX-KD-W4`
(Windows / Android / iOSで同じgesture層とsimulationを使う)。
KD-MOB-003 (main `de87fed`) の`DisplayMetrics`の上に実装した。型と契約の正本は
[mobile platform — Input and UI](../interface/mobile-platform.md#input-and-ui)。

既存の[マウス／タッチ操作](2026-09-09-kd-pointer-controls.md)を整理・拡張した。

- Windows `NativeTouchBridge`が独自に持っていた接触表 (`TouchContacts`と同じ
  処理の複製) を廃し、`TouchSample`へ正規化して共通`TouchContacts`へ渡す
- `PointerInputController`内のtap / drag判定と`TouchContacts`内のpinch計算を、
  `TapRecognizer` / `DragRecognizer` / `PinchRecognizer` /
  `recognizeGestureCancel`へ分けた。controllerはUI captureとbutton起動だけを持つ
- Ergo input injection adapter: pinned Ergo (7f0d6bbd) にtouch deviceが無いため、
  主contactをErgo mouseの位置 / 左buttonへinjectする (`planTouchPointerInjection`)。
  gestureの意味はgame側に残す
- `BUILD`操作を`PLACE` (`PointerAction::Place`) へ改名。touch tapは
  `TapPlacement::SelectOnly`で選択だけ行い、配置は明示Placeで確定する。マウスの
  再クリック確定 / Phase 1グリッド即時配置は維持
- HUDは`HudLayoutMetrics` (safe area / density / player UI scale) でsafe rect内に
  配置する。`FrameInput::safeArea`、`NativeMobileRuntime::displayMetrics`を追加
- `buildSelectionPreviewLines` / `placementHintLine`: hover相当の情報を
  selection stateから作る

### 判断 (BASE)

- BASE-MOB-004-TOUCH-PLACE-01: Phase 1グリッドもtouch tapでは即時配置しない。
  ui-ux.mdの「tapだけで即時購入しない」を優先し、mouseの既存挙動とは分けた
- BASE-MOB-004-UI-SCALE-01: player UI scaleは0.75〜1.5。densityは従来どおり
  0.75〜1.25へclampし、その積を画面寸法 (safe rect幅/320、高さ/720) で抑える
- BASE-MOB-004-CANCEL-01: OSの`Cancel`はcontact単位ではなくgesture全体を破棄する
  (Android `ACTION_CANCEL`、iOS `touchesCancelled`の実運用に合わせる)
- BASE-MOB-004-ERGO-01: Ergoへのtouch device追加はErgo repositoryの作業で、
  本taskでは上流を編集しない

### 再利用探索

- 採用: `TouchContacts` (全hostの接触表に一本化)、`PointerInputController` /
  `PointerControls` (UI captureとbutton配置をそのまま拡張)、`SelectionController`
  (配置方針を`TapPlacement`で追加)、`DisplayMetrics` / `DisplayMetricsChannel`
  (HUD layout入力)、`CampaignInputController` (Placeから`PlaceStoreCommand`)
- 不採用: Ergo `ui` / `ui_layout` module (KDでは無効化済みで、HUDは既存の
  vector font overlayが正本)、Ergo側touch device (pin外のため)

## 受け入れ条件

- C-1 normalizeTouchSample(id, phase, x, y, t) / TouchContacts::update(sample): finger ID、phase、position、timestampを失わず正規化し、非有限値・timestamp逆行はgestureを破棄して拒否する
- C-2 TapRecognizer / DragRecognizer / PinchRecognizer / recognizeGestureCancel: tap、drag、pinch、cancelを責務別に判定する
- C-3 PointerInputController::translate(): drag開始後のcontactは押下位置へ戻ってもtapとして確定しない
- C-4 TouchContacts::cancel() / PointerInputController::translate(focusLost) / planTouchPointerInjection(): app pause / contact cancel後にlive contact、captured button、Ergoの押下buttonを残さない
- C-5 PointerInputController::translate(): UI capture済みcontactからworld click / pan / pinchを生成しない
- C-6 PointerInputController::translate(): 1本指dragでcamera pan、2本指pinchでzoom値を出す
- C-7 tapPlacementFor(touch) / SelectionController::onPrimaryClick(SelectOnly) / PointerAction::Place: tapでfacilityを選択し、明示Placeだけが`PlaceStoreCommand`を作る
- C-8 buildSelectionPreviewLines(input): hover相当の情報をpointer位置ではなくselection stateから表示する
- C-9 buildPointerControls(input, HudLayoutMetrics, page): buttonとstatus textをsafe area内へ置き、density / UI scaleでbutton寸法が変わる
- C-10 DisplayMetricsChannel::publish → buildPointerControls / PointerInputController::translate(surface変更): orientation / resize後も選択とsimulation stateを保持し、進行中gestureだけ破棄する
- C-11 konbini_sim / PointerInputController: raw touch sample数で結果が変わらず、simulationはapp / adapter headerを含まない

C-1〜C-6・C-11は`tests/app/input/touch_gesture_test.cpp`、C-4のErgo部分は
`tests/adapters/touch_pointer_injection_test.cpp`、C-7〜C-10は
`tests/app/touch_hud_layout_test.cpp`、C-11のinclude境界は
`tests/cmake/sim_platform_isolation_test.cmake`で確認する。既存のtouch / HUD
回帰は`tests/app/first_playable_app_test.cpp`と`tests/render/grid_town_ui_test.cpp`。

Augur contract-wrapはTS / JS専用でC++に仕込めないため、契約の判定は上記C++
testで行う (augur.contracts.jsonは置かない)。

## 検証

- 実施: Windows x64 Debug (MSVC, `KONBINI_BUILD_TESTS=ON`) で
  `cmake --build build-mob004 --config Debug` 成功、`ctest -C Debug` 27/27 pass
  (2026-10-02)
- 未実施: アプリ起動テスト、Android / iOS 実機・emulator (スコープ外)

## 復旧方法

本変更はgame側のinput / HUD層だけで、save形式とsimulationに変更は無い。
不具合時はこのPRのsquash commitをrevertすればpointer controls (KD-POINTER) の
挙動へ戻る。
