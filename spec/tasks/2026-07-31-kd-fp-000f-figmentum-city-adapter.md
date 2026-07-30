---
task: kd-fp-000f-figmentum-city-adapter
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000c-model-city.md
  - spec/tasks/2026-07-31-kd-fp-000e-content-simulation.md
  - spec/interface/figmentum-city-generation.md
  - spec/plan/tasks/figmentum-city-plan.md
  - spec/plan/tasks/first-playable.md
  - spec/setup/native-development.md
---

# KD-FP-000F — Figmentum city adapter

## 目的

固定 revision の Figmentum を game process 内へ組み込み、semantic CityPlan を
game-owned CityManifest へ射影し、interactive facility ごとの geometry を生成する
adapter 境界を実装する。

## 完了条件

- Figmentum `3ee998f487d984f54003c4ec3c4f7ba00b53eec3` を exact revision と
  clean worktree の両方で検証してから dependency CMake を評価する
- `planCity()` の stable facility key、cell、role、recipe、bounds を
  game-owned `CityManifest` へ変換する
- Figmentum の型を `ICityGenerator` と game domain の公開境界へ漏らさない
- facility ごとに SDF を polygonize し、finite vertex、triangle index、
  deterministic normal を検証する
- geometry cache を generator revision、recipe fingerprint、polygonize resolution、
  vertex format version で識別する
- cache hit と concurrent miss のどちらでも呼出元 facility key を返す
- atomic city-generation path の失敗時に caller-owned facility ID pool を進めない
- `konbini_review` が Figmentum adapter を含む stable aggregate のまま維持される
- [Figmentum city-generation contract](../interface/figmentum-city-generation.md)、
  [KD-FG-001](../plan/tasks/figmentum-city-plan.md)、
  [KD-FP-001](../plan/tasks/first-playable.md)、
  [native development setup](../setup/native-development.md) を実装内容へ同期する
- unit / integration / startup test はこの task では実行しない

## スコープ (編集可ディレクトリ)

- `CMakeLists.txt`
- `cmake/`
- `data/content/README.md`
- `include/konbini/adapters/figmentum/`
- `include/konbini/city/`
- `src/adapters/figmentum/`
- `src/city/`
- `spec/design.md`
- `spec/faq/`
- `spec/interface/`
- `spec/plan/tasks/`
- `spec/setup/`
- `spec/tasks/`

content / simulation system、Pictor / Ergo rendering、native app、
Excubitor 起動確認はこの task に含めない。

## 実装結果 (2026-07-31)

- exact revision / clean source を dependency CMake 評価前に検証する経路を追加済み
- semantic CityPlan の manifest projection と施設単位 meshing を実装済み
- recipe geometry cache と facility identity の分離を実装済み
- city generation failure 時の facility ID allocation rollback を実装済み
- Revisor の configure / build 結果は PR review で記録する
- unit / integration / startup test は未実行
