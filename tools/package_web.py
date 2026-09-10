"""Package only the exact committed, browser-validated static distribution."""
import argparse
import json
from pathlib import Path
import sys
import zipfile
sys.dont_write_bytecode = True
from playtest_artifacts import git, sha256

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, required=True)
    parser.add_argument("--playtest-run", type=Path, required=True)
    parser.add_argument("--pictor-source", type=Path, required=True)
    parser.add_argument("--emsdk", type=Path, required=True)
    parser.add_argument("--allow-unmerged", action="store_true")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    head = git(source, "rev-parse", "HEAD")
    if git(source, "status", "--porcelain", "--untracked-files=normal"):
        parser.error("source is not clean")
    dist = args.dist.resolve()
    if not dist.is_relative_to(source):
        parser.error("distribution must be inside this checkout")
    evidence = args.playtest_run.resolve()
    verdict = json.loads((evidence / "validation-result.json").read_text(encoding="utf-8"))
    if not verdict.get("passed") or verdict.get("sourceRevision") != head or verdict.get("llmCalls") != 0:
        parser.error("successful browser validation must match this source revision")
    if not (evidence / "claim-released.txt").is_file():
        parser.error("testing claim has not been released")
    if verdict["preMerge"] and not args.allow_unmerged:
        parser.error("unmerged development distribution requires --allow-unmerged")
    if (dist / "BUILD_REVISION.txt").read_text(encoding="utf-8").splitlines() != [head, "clean"]:
        parser.error("build revision does not match")
    for name, expected in verdict["files"].items():
        if sha256(dist / name) != expected:
            parser.error("browser asset changed after validation: " + name)
    label = "development-unmerged-" if verdict["preMerge"] else ""
    output = dist.parent / ("KonbiniDominant-Web-" + label + head[:8] + ".zip")
    info = {"sourceRevision": head, "renderer": "Pictor WebGL2",
            "preMerge": verdict["preMerge"], "browserValidated": True,
            "llmCalls": 0, "configuredTimeScale": verdict["configuredTimeScale"],
            "gameSeconds": verdict["gameSeconds"], "result": verdict["result"],
            "files": verdict["files"]}
    readme = ("Konbini Dominant Web版\n\n"
        "Pictor描画・マウス／タッチ操作・横画面対応。\n"
        "最初にチェーンを選び、グリッドへ出店して強化し、最終局面を生き残ってください。\n"
        "タップ：選択／出店　ドラッグ：移動　ピンチ／ホイール：ズーム。\n"
        "縦向きやバックグラウンドではゲームが停止します。\n\n"
        "このZIPを展開し、全ファイルを同じ静的HTTP(S)配信先へ置いてindex.htmlを開いてください。\n"
        "ローカル確認の例：展開先で python -m http.server --bind 127.0.0.1 8000 を実行し、\n"
        "ブラウザで http://127.0.0.1:8000/ を開きます。index.htmlの直接ダブルクリックは非対応です。\n"
        "WebGL2対応ブラウザが必要です。公開URLはこのZIPには含まれません。\n\n"
        "Windows上の実ブラウザでマウス／横画面タッチ操作、停止再開、100倍速BTクリアを検証。\n"
        "Android/iOS実機・Safariの動作確認は未実施です。\n"
        + ("この配布物は未マージの開発版です。\n" if verdict["preMerge"] else "")
        + "Source: " + head + "\n")
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name in verdict["files"]:
            archive.write(dist / name, name)
        archive.writestr("README.txt", readme.encode("utf-8"))
        archive.writestr("build-info.json", json.dumps(info, ensure_ascii=False, indent=2).encode("utf-8"))
        archive.write(source / "data/fonts/RobotoMono/OFL.txt", "notices/RobotoMono-OFL.txt")
        archive.write(args.pictor_source.resolve() / "LICENSE", "notices/Pictor-LICENSE.txt")
        archive.write(args.emsdk.resolve() / "upstream/emscripten/LICENSE", "notices/Emscripten-LICENSE.txt")
    print(json.dumps({"file": str(output), "bytes": output.stat().st_size, "sha256": sha256(output)}))
    return 0

if __name__ == "__main__":
    sys.exit(main())
