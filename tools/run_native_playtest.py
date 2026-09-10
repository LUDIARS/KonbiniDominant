"""Build, claim, launch, verify, capture, close, restore, release. No LLM."""
import argparse
import datetime
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time

sys.dont_write_bytecode = True

from playtest_artifacts import git, sha256, staged, validate_locations
from playtest_cc import TestingClaim
from playtest_verdict import complete_records, verify
from playtest_window import capture, close


def execute(args):
    body, build = args.project_body.resolve(), args.build_dir.resolve()
    source, identity = validate_locations(body, build, Path(__file__).resolve().parents[1], args.allow_unmerged)
    evidence = body / "build/test-runs" / datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S-%fZ")
    evidence.mkdir(parents=True)
    result = dict(identity, passed=False, llmCalls=0, evidence=str(evidence))
    process = None
    try:
        with (evidence / "build.log").open("wb") as output:
            subprocess.run(["cmake", "--build", str(build), "--config", "Release", "--target", "konbini_dominant", "--parallel", "4"],
                           cwd=source, stdout=output, stderr=subprocess.STDOUT, check=True, timeout=args.build_timeout)
        result["buildLogSha256"] = sha256(evidence / "build.log")
        if git(source, "rev-parse", "HEAD") != identity["sourceRevision"] or git(source, "status", "--porcelain", "--untracked-files=normal"):
            raise RuntimeError("source changed during build")
        if git(body, "rev-parse", "HEAD") != identity["mainRevision"]:
            raise RuntimeError("main changed during build; record a fresh validation")
        with TestingClaim(evidence, body):
            with staged(body, build, evidence, identity) as (exe, report, manifest):
                with (evidence / "stdout.log").open("wb") as out, (evidence / "stderr.log").open("wb") as err:
                    process = subprocess.Popen([str(exe)], cwd=body, stdout=out, stderr=err, creationflags=subprocess.CREATE_NO_WINDOW)
                    try:
                        deadline = time.monotonic() + args.timeout
                        progress_captured = False
                        while True:
                            records = complete_records(report)
                            if (not progress_captured and records and records[-1].get("phase") == 1
                                    and records[-1].get("stores",0) >= 3 and records[-1].get("framesPresented",0) > 0):
                                result["progressScreenshot"] = capture(process.pid, evidence / "phase1-progress.png")
                                progress_captured = True
                            if any(row.get("event") == "resultPresented" for row in records):
                                break
                            if process.poll() is not None:
                                raise RuntimeError(f"native game exited before a result: {process.returncode}")
                            if time.monotonic() >= deadline:
                                raise TimeoutError("native playtest timed out")
                            time.sleep(0.1)
                        result.update(verify(records))
                        result["screenshot"] = capture(process.pid, evidence / "clear-screen.png")
                        result["screenshotSha256"] = sha256(evidence / "clear-screen.png")
                    finally:
                        if report.exists():
                            (evidence / "playtest-report.jsonl").write_bytes(report.read_bytes())
                        result["exitCode"] = close(process)
                        process = None
                if result["exitCode"] != 0:
                    raise RuntimeError("native game returned nonzero exit status")
                if (evidence / "stderr.log").read_bytes().strip():
                    raise RuntimeError("native stderr is not empty; inspect errors or dropped ticks")
                for asset in manifest["files"]:
                    if sha256(exe.parent / asset["path"]) != asset["sha256"]:
                        raise RuntimeError("runtime asset changed during playtest")
        result["passed"] = True
    except Exception as error:
        result["error"] = f"{type(error).__name__}: {error}"
    finally:
        if process and process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        (evidence / "validation-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["passed"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-body", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--allow-unmerged", action="store_true", help="Record preliminary validation, never mark the merge gate passed")
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--build-timeout", type=float, default=600)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("this runner launches the Windows native game")
    if not all(math.isfinite(value) and value > 0 for value in (args.timeout, args.build_timeout)):
        parser.error("timeouts must be positive")
    try:
        return execute(args)
    except Exception as error:
        print(json.dumps({"passed": False, "error": f"{type(error).__name__}: {error}", "llmCalls": 0}))
        return 1


if __name__ == "__main__":
    sys.exit(main())
