"""Package the built game and its assets; never launch it."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
import subprocess
import sys

sys.dont_write_bytecode = True

from pe_dependencies import imported_dlls
from playtest_artifacts import git
from package_validation import validated_playtest

SHADERS = (
    "konbini_world.vert.spv", "konbini_world.frag.spv",
    "konbini_composite.vert.spv", "konbini_composite.frag.spv",
    "konbini_hud.vert.spv", "konbini_hud.frag.spv",
)
SYSTEM_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "shell32.dll", "advapi32.dll",
    "ole32.dll", "oleaut32.dll", "winmm.dll", "ntdll.dll", "ws2_32.dll",
    "setupapi.dll", "hid.dll", "imm32.dll", "version.dll", "shlwapi.dll",
    "comdlg32.dll", "comctl32.dll", "crypt32.dll", "secur32.dll", "bcrypt.dll",
    "dinput8.dll", "dxguid.dll", "xinput9_1_0.dll", "vulkan-1.dll",
}


def verified_source_revision(root: Path, requested: str) -> str:
    if len(requested) != 40 or any(c not in "0123456789abcdef" for c in requested):
        raise ValueError("revision must be a full lowercase commit SHA")
    head = subprocess.run(
        ["git", "-C", str(root), "rev-parse", "HEAD"], check=True,
        capture_output=True, text=True, encoding="utf-8",
    ).stdout.strip()
    dirty = subprocess.run(
        ["git", "-C", str(root), "status", "--porcelain"],
        check=True, capture_output=True, text=True, encoding="utf-8",
    ).stdout
    if dirty:
        raise ValueError("source changes must be committed before packaging")
    if requested != head:
        raise ValueError("requested revision does not match repository HEAD")
    return head


def verify_build_revision(revision_stamp: Path, expected_revision: str) -> None:
    expected_stamp = expected_revision + "\nclean"
    if (not revision_stamp.is_file() or
            revision_stamp.read_text(encoding="ascii").strip() != expected_stamp):
        raise ValueError("executable was not built from repository HEAD")


def main(content_version: int = 4, label: str = "Phase1-4") -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build-native")
    parser.add_argument("--revision", required=True)
    parser.add_argument("--playtest-run", type=Path, help="Successful merged-main no-LLM runtime evidence (required for campaign releases)")
    parser.add_argument("--allow-unmerged", action="store_true", help="Explicitly label a verified development ZIP; never mark the merge gate passed")
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = (root / arguments.build_dir).resolve()
    if root not in build.parents:
        raise ValueError("build directory must be inside the repository")
    if len(arguments.revision) != 40 or any(c not in "0123456789abcdef" for c in arguments.revision):
        raise ValueError("revision must be a full lowercase commit SHA")
    head = verified_source_revision(root, arguments.revision)
    release = build / "Release"
    revision_stamp = release / "BUILD_REVISION.txt"
    verify_build_revision(revision_stamp, head)
    executable = release / "konbini_dominant.exe"
    dependencies = imported_dlls(executable)
    missing = [name for name in dependencies if name.lower() not in SYSTEM_DLLS
               and not name.lower().startswith(("api-ms-win-", "ext-ms-win-"))]
    if missing:
        raise ValueError("non-system DLL dependencies must be packaged: " + ", ".join(missing))
    files = {"konbini_dominant.exe": executable,
             "BUILD_REVISION.txt": revision_stamp,
             "README-ja.txt": root / ("data/distribution/README-phase1-ja.txt" if content_version == 3 else "data/distribution/README-ja.txt"),
             "data/content/first-playable.json": release / "data/content/first-playable.json"}
    files["licenses/RobotoMono-OFL.txt"] = root / "data/fonts/RobotoMono/OFL.txt"
    for shader in SHADERS:
        source = release / "shaders" / shader
        if source.read_bytes()[:4] != b"\x03\x02\x23\x07":
            raise ValueError("invalid SPIR-V: " + shader)
        files["shaders/" + shader] = source
    notices = sorted((build / "notices").glob("*.txt"))
    if len(notices) < 3:
        raise ValueError("required third-party notices are missing")
    for notice in notices:
        files["licenses/" + notice.name] = notice
    for source in files.values():
        if not source.is_file() or root not in source.resolve().parents:
            raise ValueError("missing or out-of-scope package input: " + str(source))
    profile = json.loads(files["data/content/first-playable.json"].read_text(encoding="utf-8"))
    if profile["contentVersion"] != content_version or "phase1" not in profile:
        raise ValueError("package profile version does not match the requested release")
    if content_version == 4 and not all(key in profile.get("campaign", {})
                                         for key in ("vertical", "dimensions", "aion", "skills")):
        raise ValueError("campaign package is missing gameplay rules")
    verification = None
    if content_version == 4 and arguments.playtest_run is None:
        raise ValueError("campaign release requires --playtest-run from the merged main build")
    if arguments.playtest_run is not None:
        body = Path(git(root, "rev-parse", "--path-format=absolute", "--git-common-dir")).resolve().parent
        verification = validated_playtest(arguments.playtest_run, body, arguments.revision, files, arguments.allow_unmerged)
    folder = "KonbiniDominant-" + label + "-Windows-x64"
    pre_merge = verification is not None and verification.get("preMerge") is True
    suffix = "-development-unmerged" if pre_merge else ""
    destination = build / (folder + suffix + "-" + arguments.revision[:8] + ".zip")
    if destination.exists():
        raise FileExistsError(destination)
    manifest = {
        "revision": arguments.revision, "platform": "Windows x64", "configuration": "Release",
        "contentVersion": profile["contentVersion"], "runtimeDependencies": dependencies,
        "testsExecuted": False, "gameLaunched": verification is not None,
        "nativePlaytestExecuted": verification is not None,
        "nativePlaytest": verification,
        "localValidationPassed": verification is not None,
        "mergeGatePassed": False,
        "releaseStatus": "development-unmerged" if pre_merge else ("locally-verified-merged" if verification else "unverified"),

        "files": {name: hashlib.sha256(source.read_bytes()).hexdigest() for name, source in files.items()},
    }
    with zipfile.ZipFile(destination, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, source in files.items():
            archive.write(source, folder + "/" + name)
        archive.writestr(folder + "/BUILD_INFO.json", json.dumps(manifest, ensure_ascii=False, indent=2))
    # Read the finished archive and compare every asset hash, without launching
    # the application or executing any unit/integration tests.
    with zipfile.ZipFile(destination) as archive:
        for name, expected in manifest["files"].items():
            actual = hashlib.sha256(archive.read(folder + "/" + name)).hexdigest()
            if actual != expected:
                raise ValueError("archive hash mismatch: " + name)
    print(json.dumps({"zip": str(destination), "bytes": destination.stat().st_size,
                      "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
                      "runtimeDependencies": dependencies, "files": len(files) + 1}, ensure_ascii=False))


if __name__ == "__main__":
    main()
