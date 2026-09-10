"""Build identity, staged runtime assets, and reversible test configuration."""
import contextlib
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

FILES = ["konbini_dominant.exe", "data/content/first-playable.json"] + [
    "shaders/konbini_" + name + ".spv" for name in
    ("world.vert", "world.frag", "composite.vert", "composite.frag", "hud.vert", "hud.frag")]


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_locations(body, build, script_root, allow_unmerged):
    if not (body / ".git").is_dir() or git(body, "branch", "--show-current") != "main":
        raise ValueError("project body must be the main checkout, not a worktree")
    common = Path(git(script_root, "rev-parse", "--path-format=absolute", "--git-common-dir")).resolve()
    if common != (body / ".git").resolve():
        raise ValueError("project body must own this script checkout, not a clone")
    if body not in build.parents:
        raise ValueError("build must be inside the project body")
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    match = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$", cache, re.MULTILINE)
    if not match:
        raise ValueError("CMake source directory is unavailable")
    source = Path(match.group(1).strip()).resolve()
    if source != body and body not in source.parents:
        raise ValueError("CMake source is outside the project")
    if Path(git(source, "rev-parse", "--path-format=absolute", "--git-common-dir")).resolve() != common:
        raise ValueError("build source is a different repository")
    if git(source, "status", "--porcelain", "--untracked-files=normal"):
        raise ValueError("commit source changes before running a recorded test")
    revision, main = git(source, "rev-parse", "HEAD"), git(body, "rev-parse", "HEAD")
    # A clean build worktree at the exact main commit is also an integrated
    # source. The executable is still staged and launched only from the body.
    pre_merge = revision != main
    if pre_merge and not allow_unmerged:
        raise ValueError("use the merged main build, or explicitly pass --allow-unmerged for preliminary validation")
    return source, {"sourceRevision": revision, "mainRevision": main, "preMerge": pre_merge}


@contextlib.contextmanager
def staged(body, build, evidence, identity):
    release = body / "build/Release"
    release.mkdir(parents=True, exist_ok=True)
    config, report = release / "playtest.cfg", release / "playtest-report.jsonl"
    if config.exists():
        raise RuntimeError("a playtest.cfg already exists; another test may own the executable")
    changes = []
    manifest = dict(identity, files=[])
    try:
        for name in FILES + ["playtest-report.jsonl"]:
            destination = release / name
            if destination.exists():
                backup = evidence / "original" / name
                backup.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(destination, backup)
            else:
                backup = None
            changes.append((destination, backup))
            if name == "playtest-report.jsonl":
                destination.unlink(missing_ok=True)
                continue
            source = build / "Release" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            if source.resolve() != destination.resolve():
                shutil.copy2(source, destination)
            manifest["files"].append({"path": name, "sha256": sha256(destination)})
        (evidence / "artifact-manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        config.write_text("100 autoplay\n", encoding="utf-8")
        yield release / "konbini_dominant.exe", report, manifest
    finally:
        config.unlink(missing_ok=True)
        for destination, backup in reversed(changes):
            if backup:
                shutil.copy2(backup, destination)
            else:
                destination.unlink(missing_ok=True)
