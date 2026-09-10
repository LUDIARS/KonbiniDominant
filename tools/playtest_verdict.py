"""Deterministic clear criteria; consumes game output, never writes game state."""
import json

EXPECTED_PHASES = [1, 3, 4, 5, 6, 2]


def complete_records(report):
    if not report.exists():
        return []
    text = report.read_text(encoding="utf-8")
    # Ignore a final partial write; a complete line must end with a newline.
    return [json.loads(line) for line in text.splitlines(keepends=True) if line.endswith("\n")]


def verify(records):
    if not records or records[0].get("timeScale") != 100 or records[0].get("autoplay") is not True:
        raise ValueError("100x deterministic autoplay was not configured")
    results = [r for r in records if r.get("event") == "resultPresented"]
    if len(results) != 1:
        raise ValueError("exactly one presented result is required")
    result = results[0]
    run = result["run"]
    phases = [r["phase"] for r in records if r.get("event") == "phase" and r.get("run") == run]
    if phases != EXPECTED_PHASES:
        raise ValueError("campaign phase progression is incomplete: " + str(phases))
    if not (result["phase"] == 2 and result["outcome"] == 1 and result["endReason"] == 5
            and result["foreignDestroyed"] >= 2 and result["framesPresented"] > 0 and result["stores"] > 0):
        raise ValueError("normal Aion-survival victory was not achieved")
    if result["elapsedTicks"] <= 0 or result["ticksPerSecond"] <= 0 or result["wallSeconds"] <= 0:
        raise ValueError("invalid timing evidence")
    game_seconds = result["elapsedTicks"] / result["ticksPerSecond"]
    return {"run": run, "configuredTimeScale": 100, "gameSeconds": game_seconds,
            "wallSecondsIncludingStartup": result["wallSeconds"],
            "effectiveSpeedIncludingStartup": game_seconds / result["wallSeconds"],
            "phaseSequence": phases, "result": result}
