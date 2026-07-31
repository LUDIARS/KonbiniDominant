# Revisor opposite-model review failed without actionable diagnostics

- Date: 2026-07-31
- Status: investigating; retry pending
- Area: Revisor local PR review
- Severity: PR review blocked; no KonbiniDominant defect identified

## Summary

Revisor could not complete the first review of `LUDIARS/KonbiniDominant#12`.
The local PR remained open, but its Check Run was marked failed with only the
generic message `Opposite-model reviewer failed; output was withheld from the
Check Run.` This is an infrastructure incident rather than evidence that the
smartphone specification or registered project checks failed.

## Evidence

- Local PR: `LUDIARS/KonbiniDominant#12`
- Head: `f75b980222f2a2d5ec678ba77f62615e5442f673`
- Review job: `c5d8b0b2-4fb7-429b-b438-bb6e17e5cd0a`
- Created: `2026-07-30T22:08:55.882Z`
- Failed: `2026-07-30T22:09:05.205Z`
- Elapsed time: approximately 9.3 seconds
- Revisor state: PR `open`, check `failed`, no review result
- The same generic failure occurred once among the 49 jobs retained by the
  running Revisor instance at investigation time.
- The configured reviewer CLI is installed and reports an authenticated session.
- `Revisor/src/runner.mjs` replaces every unsuccessful reviewer process result
  with the same generic error before the job result is stored.

## Regression Context

Earlier KonbiniDominant local PRs completed automated review. This failure did
not retain a reviewer exit code or a safely redacted failure category, so the
operator cannot distinguish a transient provider failure from an invocation or
configuration fault without rerunning the review.

## Cause

The immediate cause is a non-success result from the opposite-model reviewer
process. The exact upstream cause is unknown because its stdout, stderr, and exit
code were intentionally withheld and were not represented by a redacted
diagnostic category.

The short duration is consistent with an early CLI or provider failure. A missing
CLI or logged-out local session is not indicated by the read-only checks, leaving
a transient provider failure, rate limit, or invocation-specific error as the
leading possibilities.

## Fix Requirements

- Preserve the rule that model output must not be copied into a public Check Run.
- Retain safe operational diagnostics separately: reviewer identifier, exit code,
  timeout state, and a redacted failure category.
- Never persist credentials, prompts, raw model output, or unredacted stderr.
- Mark known transient reviewer failures as retryable without presenting them as
  repository test or code failures.
- Keep retry idempotent and re-resolve the local head and base refs before enqueue.

## Verification

No project test or startup command was run during diagnosis.

- Retry PR #12 after confirming the feature branch is clean and no review is
  already queued or running.
- Confirm the retry re-resolves the current branch head and creates a distinct job.
- If the retry succeeds, record the first failure as transient.
- Revisor regression coverage should verify safe failure classification and
  forced retry of an unchanged head without leaking reviewer output.

## Follow-up

If the retry fails with the same message, create a task-workflow item in Revisor
for redacted reviewer diagnostics before attempting repeated blind retries.
