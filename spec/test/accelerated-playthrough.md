# Accelerated native playthrough
Status: initial native 100x clear verified on the pre-merge 911d960 build; BT runner validation recorded separately.

Use [no-llm-native-playtest.md](no-llm-native-playtest.md) for subsequent local runs without an LLM.

Required acceptance gate (2026-09-10): [campaign-release-acceptance.md](campaign-release-acceptance.md).
Merge the intended changes, including the startup fix and playtest support, then verify the integrated build from the project body.
Shared infrastructure updates are owned by headquarters; this does not count as a successful game test.

Place playtest.cfg beside the Windows executable with one line:
100 autoplay

This opts into normal-command automation and scales the fixed-tick accumulator by 100.
Absent this file, normal 1x manual play is used. '100 manual' accelerates time without automation.
Economy, enemy AI, content, skill offers, tick duration and victory conditions are unchanged.
Autoplay uses a deterministic reactive-priority behavior tree and reads only visible render snapshots and remembers observed origin-world stores. It submits ordinary select-chain, placement, skill, view-dimension, inversion and escape commands.
This is a game-logic driver; it does not exercise mouse or touch hit testing. Manual input validation remains required.
The configured 100x rate is a target: render stalls, tick catch-up limits and skill-selection boundaries can reduce achieved wall-clock acceleration.

playtest-report.jsonl records configuration, phase transitions, periodic progress, and resultPresented separately.
A simulation win without a resultPresented event is not proof that the native result screen was presented.
A resultPresented event is not a screenshot or a human readability check.
For a clear, require Result phase (2), Win outcome (1), foreignDestroyed >=2, and a resultPresented record with presented frames >0.
Retain startup/render stderr and inspect the real result screen. A lost match must not be labeled a clear.

The user authorized direct test-EXE launches on 2026-09-10; Excubitor is optional for this test.
Claim Concordia testing, launch from the project body only, stop the process started for this test, then release.
Do not launch from a worktree or a copied project.
Do not distribute playtest.cfg or a successful test report from another revision.
Normal manual input and both mobile platform launches are separate verification gates.
