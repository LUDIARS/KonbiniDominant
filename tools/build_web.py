"""Build the Pictor WebGL2 browser campaign with a project-local Emscripten SDK."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emsdk", required=True, type=Path)
    parser.add_argument("--ninja", type=Path)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--pictor-source", type=Path)
    parser.add_argument("--ergo-source", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = (args.build_dir or root / "build-web").resolve()
    if not build.is_relative_to(root):
        parser.error("build directory must be inside this checkout")
    sdk = args.emsdk.resolve()
    config = sdk / ".emscripten"
    cmake_driver = sdk / "upstream/emscripten/emcmake.py"
    if not config.is_file() or not cmake_driver.is_file():
        parser.error("install and activate Emscripten 6.0.9 in --emsdk first")
    ninja = str(args.ninja.resolve()) if args.ninja else shutil.which("ninja")
    if not ninja:
        parser.error("Ninja is required; use --ninja for Visual Studio's bundled ninja.exe")
    env = dict(os.environ, EMSDK=sdk.as_posix(), EM_CONFIG=config.as_posix(),
               EM_CACHE=(sdk / "upstream/emscripten/cache").as_posix())
    command = [sys.executable, str(cmake_driver), "cmake", "-S", str(root),
               "-B", str(build), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
               "-DCMAKE_MAKE_PROGRAM=" + ninja]
    for name in ("pictor", "ergo"):
        source = getattr(args, name + "_source")
        if source:
            command.append("-DFETCHCONTENT_SOURCE_DIR_" + name.upper() + "=" + str(source.resolve()))
    subprocess.run(command, cwd=root, env=env, check=True)
    # A commit can change while object files remain byte-identical. Relink the
    # generated output so the existing post-link stamp describes this revision.
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    stamp = build / "dist/BUILD_REVISION.txt"
    output = build / "dist/index.html"
    if stamp.exists() and stamp.read_text(encoding="utf-8").splitlines() != [head, "clean"]:
        output.unlink(missing_ok=True)
    subprocess.run(["cmake", "--build", str(build), "--target", "konbini_web", "--parallel", "4"],
                   cwd=root, env=env, check=True)
    print(build / "dist")
    return 0

if __name__ == "__main__":
    sys.exit(main())
