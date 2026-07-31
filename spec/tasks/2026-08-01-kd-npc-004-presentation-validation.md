---
task: kd-npc-004-presentation-validation
project: KonbiniDominant
kind: テスト
status: pending
created: 2026-08-01
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 685
actio_task_id: null
memory_links:
  - spec/tasks/2026-08-01-kd-npc-001-visia-presentation.md
  - spec/feature/npc-conversations-and-placement-feedback.md
  - spec/interface/visia-presentation.md
  - spec/test/verification-strategy.md
---

# KD-NPC-004 — Resident / Visia presentation validation

## 目的

KD-NPC-001で追加したpure projection、content v2、CPU geometry、animation samplerの
境界値と決定性を登録testで固定する。

## 完了条件

- content v2の正常profile、missing / unknown key、非finite値、期間不整合を検証する
- 同じseed / cell / tickでresident recordとremarkが一致する
- unassigned、距離0、歩行境界、store滞在／発話終了、return境界を検証する
- 成功placementだけがtarget position / yaw 0度のcueを生成する
- resident boxとannulusのvertex/index範囲、winding、finite値を検証する
- speech bubbleの対応glyph、未対応文字、距離境界、camera basis、index overflowを検証する
- hold / fall / landing / effect終了の境界と270度最終poseを検証する
- canonical schema/layoutにresident/cueが混入しないことを検証する
- runtime接続後はExcubitor経由の目視証跡をTestWorkflowへ記録する

## スコープ (編集可ディレクトリ)

- `tests/sim/`
- `tests/render/`
- `tests/CMakeLists.txt`
- `spec/test/`
- `spec/tasks/`

## 制約

このtaskのtest実行はユーザの明示指示を得たセッションだけで行う。
service起動を伴う場合はConcordia claim / releaseとプロジェクト本体フォルダを使う。
