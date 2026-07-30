---
task: kd-fp-000g-render-domain-foundation
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000f-figmentum-city-adapter.md
  - spec/interface/pictor-rendering.md
  - spec/plan/tasks/first-playable.md
---

# KD-FP-000G — Render-domain foundation

## 目的

Pictor / Ergo / Vulkan の所有型を game domain へ漏らさず、isometric camera、
screen-to-world ray、deterministic facility picking、presentation palette、
ZOC overlay geometryを first playable の描画境界として実装する。

## 完了条件

- cameraが有限な設定とgame-owned `ViewportExtent` から決定的な行列を生成する
- screen pointをcameraと同じextentのworld rayへ変換する
- pickerがfinite AABB ray intersectionを行い、等距離時はstable facility keyで決める
- chain / facility stateのpaletteを明示的なfirst-playable値へ固定する
- ZOC geometryがstoreごとの検証、32-bit overflow検査、stable vertex/index順を持つ
- CPU-side `WorldVertex` がPictor / Vulkan headerへ依存しない
- `konbini_review` が `konbini_render_domain` をbuild対象にする
- unit / integration / behavior / startup testはこのtaskでは実行しない

## スコープ (編集可ディレクトリ)

- `include/konbini/render/`
- `src/render/`
- `src/CMakeLists.txt`
- `spec/interface/`
- `spec/tasks/`

Pictor dependency、GPU asset upload、depth target、graphics pipeline、
Ergo layer、native app、Excubitor起動確認は後続taskに分離する。

## 実装結果 (2026-07-31)

- game-owned viewport / vertex / ray型を追加済み
- isometric cameraとdeterministic facility pickerを追加済み
- presentation paletteとZOC CPU geometryを追加済み
- headless `konbini_render_domain` targetを追加済み
- Revisorのconfigure / build結果はPR reviewで記録する
- unit / integration / behavior / startup testは未実行
