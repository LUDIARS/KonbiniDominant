# Four-PR integration regression checks

- Trigger: merge of PRs 1627, 1628, 1629 and 1643, including their independent reviewer fixes.
- Observed: 3 of 13 native regression executables failed on integration revision 9fb6a6e.
- Runtime cause: repeated floating-point subtraction counted 0.25 seconds at 100 Hz as 24 ticks instead of 25. Count whole ticks once, accounting for one representable rounding step at the boundary; preserve catch-up limits and drop reporting.
- Fixture causes: the campaign AI fixture had zero-size facility bounds, rejected by the existing table invariant. Give it a valid grid-sized bound.
- Stale expectations: vector glyphs use triangles and support lowercase; same-tick store destruction suppresses the landing cue. Test the current contracts without removing invalid-data checks.
- Verification: existing native suites plus a 600-frame, 100x fixed-step count assertion; full campaign BT run from the project body before release. Results remain in build/test-runs; no external CI approval is implied.
