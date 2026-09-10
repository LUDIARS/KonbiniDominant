"""Claim, stage browser assets under the project body, run fixed checks, release."""
import argparse
import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
from playtest_artifacts import git, sha256
from playtest_cc import TestingClaim
from playtest_verdict import complete_records, verify

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-body", type=Path, required=True)
    parser.add_argument("--dist", type=Path, required=True)
    parser.add_argument("--playwright", type=Path, required=True)
    args = parser.parse_args()
    body, dist = args.project_body.resolve(), args.dist.resolve()
    source = Path(__file__).resolve().parents[1]
    playwright = args.playwright.resolve()
    if not all(p.is_relative_to(body) for p in (source, dist, playwright)):
        parser.error("source, distribution and Playwright must be inside the project body")
    if git(body, "branch", "--show-current") != "main":
        parser.error("project body must remain on main")
    if git(source, "status", "--porcelain", "--untracked-files=normal"):
        parser.error("commit the source before recording release validation")
    head = git(source, "rev-parse", "HEAD")
    if (dist / "BUILD_REVISION.txt").read_text(encoding="utf-8").splitlines() != [head, "clean"]:
        parser.error("relink the browser build at the current committed revision")
    evidence = body / "build/test-runs" / ("web-" + datetime.datetime.now(
        datetime.timezone.utc).strftime("%Y%m%dT%H%M%S-%fZ"))
    evidence.mkdir(parents=True)
    site = evidence / "site"
    shutil.copytree(dist, site)
    files = {str(p.relative_to(site)): sha256(p) for p in site.rglob("*") if p.is_file()}
    result = {"passed": False, "sourceRevision": head, "llmCalls": 0,
              "preMerge": head != git(body, "rev-parse", "main"),
              "files": files, "evidence": str(evidence)}
    try:
        note = "Pictor WebGL2 browser mouse/touch/landscape and existing 100x BT clear. Browser EXE cwd is project body; harness supplies static responses, no listening server. No LLM."
        with TestingClaim(evidence, body, note=note):
            temp = evidence / "tmp"
            temp.mkdir()
            env = dict(os.environ, TMP=str(temp), TEMP=str(temp))
            with (evidence / "driver.log").open("wb") as output:
                run = subprocess.run(["node", str(source / "tools/web_playtest.mjs"),
                    str(site), str(evidence), str(playwright)], cwd=body, env=env,
                    stdout=output, stderr=subprocess.STDOUT, timeout=300)
            browser = json.loads((evidence / "browser-result.json").read_text(encoding="utf-8"))
            result["browser"] = browser
            if run.returncode or not browser["passed"]:
                raise RuntimeError(browser.get("error", "browser verification failed"))
            result.update(verify(complete_records(evidence / "playtest-report.jsonl")))
            if git(source, "rev-parse", "HEAD") != head or git(source, "status", "--porcelain", "--untracked-files=normal"):
                raise RuntimeError("source changed during browser validation")
            for name, expected in files.items():
                if sha256(site / name) != expected:
                    raise RuntimeError("staged browser asset changed")
            result["screenshotSha256"] = sha256(evidence / "clear-screen.png")
        result["passed"] = True
    except Exception as error:
        result["error"] = str(error)
    (evidence / "validation-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result))
    return 0 if result["passed"] else 1

if __name__ == "__main__":
    sys.exit(main())
