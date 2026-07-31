# Task reconciler rewrites date scalars and exposes encoding-sensitive reads

- Date: 2026-07-31
- Status: unresolved upstream; mitigated in working tree
- Area: Concordia task-workflow reconciler
- Severity: task metadata drift and apparent text corruption in PowerShell 5.1

## Summary

This is a task-workflow regression. After seven pending mobile task files were created,
Concordia registered them in Memoria and rewrote each Markdown file. The rewrite changed
the required `created: YYYY-MM-DD` scalar into an ISO timestamp. Reading the rewritten
BOM-less UTF-8 files through PowerShell 5.1 without an explicit UTF-8 encoding also
displayed Japanese text as mojibake.

## Evidence

- Affected files:
  `spec/tasks/2026-07-31-kd-mob-001-*.md` through
  `spec/tasks/2026-07-31-kd-mob-007-*.md`
- Memoria IDs `671` through `677` were written successfully.
- Before reconciliation: `created: 2026-07-31`
- After reconciliation: `created: 2026-07-31T00:00:00.000Z`
- `KD-MOB-000` had `status: done`, was not reconciled, and retained its original date.
- Byte inspection and explicit UTF-8 decoding showed the Japanese body remained valid
  UTF-8. The mojibake was a default PowerShell decoding symptom, not damaged UTF-8 bytes.

Relevant Concordia code:

- `src/taskflow/md-store.ts::parseTaskMarkdown`
- `src/taskflow/md-store.ts::renderTaskMarkdown`
- `src/taskflow/md-store.ts::updateMemoriaTaskId`

## Regression Context

Task-workflow spec §2.1 requires `created` in `YYYY-MM-DD` form. The reconciler is
supposed to update only `memoria_task_id`; reserializing unrelated scalars changes the
repository source of truth.

## Cause

Leading diagnosis:

- `js-yaml` parses an unquoted YAML date as a date value.
- `yaml.dump` serializes that value as an ISO timestamp when the complete frontmatter is
  regenerated.
- Node `writeFile(..., "utf8")` emits BOM-less UTF-8. PowerShell 5.1 callers that omit
  explicit UTF-8 decoding can therefore display valid Japanese bytes incorrectly.

## Fix Requirements

- Updating `memoria_task_id` must preserve unrelated frontmatter values and body bytes.
- `created` must remain `YYYY-MM-DD` after parse / render.
- Unicode task titles, kinds, and bodies must round-trip unchanged.
- The writer and PowerShell operational commands must have an explicit UTF-8 contract.
- Existing task files must not receive duplicate Memoria registrations during repair.

## Verification

No tests were run in this session.

Required Concordia regression coverage:

- parse → update ID → render preserves a Japanese body byte-for-byte
- `created: 2026-07-31` remains the same scalar
- only `memoria_task_id` changes
- a second reconcile does not create another Memoria task

## Follow-up

Create the implementation task in `LUDIARS/Concordia/spec/tasks/` before changing the
reconciler. This KonbiniDominant change only restores the affected metadata and records
the upstream requirement.
