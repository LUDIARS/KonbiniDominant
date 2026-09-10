"""Bind a release to a successful local playthrough of identical runtime files."""
import hashlib
import json

from playtest_artifacts import FILES
from playtest_verdict import complete_records, verify


def validated_playtest(evidence, body, revision, files, allow_unmerged=False):
    evidence = evidence.resolve()
    if body != evidence and body not in evidence.parents:
        raise ValueError("playtest evidence must be inside the project body")

    def checked(name):
        path = (evidence / name).resolve()
        if evidence not in path.parents or not path.is_file():
            raise ValueError("missing or out-of-scope playtest evidence: " + name)
        return path

    result = json.loads(checked("validation-result.json").read_text(encoding="utf-8"))
    manifest = json.loads(checked("artifact-manifest.json").read_text(encoding="utf-8"))
    build_log = checked("build.log")
    if not build_log.stat().st_size or hashlib.sha256(build_log.read_bytes()).hexdigest() != result.get("buildLogSha256"):
        raise ValueError("playtest build log is missing or differs from the validation record")
    if checked("claim-released.txt").read_text(encoding="utf-8") != "released\n":
        raise ValueError("playtest testing claim was not released")
    for identity in (result, manifest):
        if identity.get("sourceRevision") != revision:
            raise ValueError("package revision differs from tested source")
        if not allow_unmerged and (identity.get("mainRevision") != revision or identity.get("preMerge") is not False):
            raise ValueError("release requires a playtest of this merged main revision")
        if any(identity.get(key) != result.get(key) for key in ("mainRevision", "preMerge")):
            raise ValueError("playtest identity records disagree")
    if result.get("passed") is not True or result.get("exitCode") != 0 or result.get("llmCalls") != 0:
        raise ValueError("release requires a successful no-LLM native playtest")
    if checked("stderr.log").read_bytes().strip():
        raise ValueError("playtest stderr is not empty")
    verdict = verify(complete_records(checked("playtest-report.jsonl")))
    if any(result.get(key) != value for key, value in verdict.items()):
        raise ValueError("playtest verdict does not match the recorded gameplay")
    screenshot = checked("clear-screen.png")
    if hashlib.sha256(screenshot.read_bytes()).hexdigest() != result.get("screenshotSha256"):
        raise ValueError("playtest screenshot hash mismatch")
    assets = manifest.get("files", [])
    if len(assets) != len(FILES) or {asset["path"] for asset in assets} != set(FILES):
        raise ValueError("playtest runtime manifest is incomplete")
    for asset in assets:
        source = files.get(asset["path"])
        if source is None or hashlib.sha256(source.read_bytes()).hexdigest() != asset["sha256"]:
            raise ValueError("package differs from tested runtime: " + asset["path"])
    result.pop("evidence", None)
    # These files provide reproducible local evidence, not an authenticated CI
    # attestation. Callers must not elevate them into an external merge gate.
    result["evidenceTrust"] = "local-self-attested"
    return result
