"""Repository checks for the Noto Sans JP speech assets (KD-NPC-002).

Standard library only: fontTools is needed to bake, not to verify the
committed result. @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines
"""
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import bake_speech_glyphs  # noqa: E402

CATALOG = ROOT / "data/locale/ja/resident_remarks.json"
SOURCE = ROOT / "data/fonts/NotoSansJP/source.json"
OFL = ROOT / "data/fonts/NotoSansJP/OFL.txt"
GENERATED = ROOT / "src/render/generated/noto_sans_jp_speech_mesh.inc"
CONTENT_FILES = [ROOT / "data/content/first-playable.json", ROOT / "data/content/phase1.json"]


class BakeHelpersTest(unittest.TestCase):
    def test_c_bytes_escapes_every_utf8_byte(self):
        self.assertEqual(bake_speech_glyphs.c_bytes("A"), '"\\x41"')
        escaped = bake_speech_glyphs.c_bytes("近")
        self.assertEqual(escaped, '"\\xE8\\xBF\\x91"')
        self.assertTrue(escaped.isascii())

    def test_catalog_rejects_duplicate_or_empty_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "catalog.json"
            path.write_text(json.dumps({"locale": "ja", "lines": [
                {"key": "A", "text": "x"}, {"key": "A", "text": "y"}]}), encoding="utf-8")
            with self.assertRaises(ValueError):
                bake_speech_glyphs.load_catalog(path)
            path.write_text(json.dumps({"locale": "ja", "lines": [
                {"key": "A", "text": ""}]}), encoding="utf-8")
            with self.assertRaises(ValueError):
                bake_speech_glyphs.load_catalog(path)


class CommittedAssetsTest(unittest.TestCase):
    def test_generated_atlas_matches_the_current_catalog(self):
        # A catalog edit without a re-bake would ship lines whose glyphs are
        # missing; the generated file records the catalog it was baked from.
        catalog_sha, baked = bake_speech_glyphs.read_baked_header(
            GENERATED.read_text(encoding="utf-8"))
        self.assertEqual(catalog_sha, hashlib.sha256(CATALOG.read_bytes()).hexdigest())
        _, lines = bake_speech_glyphs.load_catalog(CATALOG)
        self.assertEqual(baked, bake_speech_glyphs.catalog_codepoints(lines))

    def test_every_content_remark_has_a_localized_line(self):
        _, lines = bake_speech_glyphs.load_catalog(CATALOG)
        keys = {line["key"] for line in lines}
        for content in CONTENT_FILES:
            remarks = json.loads(content.read_text(encoding="utf-8"))["residentPresentation"]["remarks"]
            self.assertTrue(set(remarks) <= keys, content.name)

    def test_font_provenance_and_license(self):
        source = json.loads(SOURCE.read_text(encoding="utf-8"))
        self.assertEqual(source["license"], "OFL-1.1")
        self.assertIn("github.com/google/fonts", source["repository"])
        files = {entry["file"]: entry for entry in source["files"]}
        self.assertEqual(files["OFL.txt"]["sha256"], hashlib.sha256(OFL.read_bytes()).hexdigest())
        self.assertIn("SIL Open Font License", OFL.read_text(encoding="utf-8"))
        # The TTF is referenced by hash but never committed.
        self.assertFalse((SOURCE.parent / "NotoSansJP[wght].ttf").exists())
        ttf_sha = files["NotoSansJP[wght].ttf"]["sha256"]
        self.assertIn(ttf_sha, GENERATED.read_text(encoding="utf-8"))

    def test_packages_ship_the_noto_sans_jp_notice(self):
        game = (ROOT / "tools/package_game.py").read_text(encoding="utf-8")
        web = (ROOT / "tools/package_web.py").read_text(encoding="utf-8")
        self.assertIn('"licenses/NotoSansJP-OFL.txt"] = root / "data/fonts/NotoSansJP/OFL.txt"', game)
        self.assertIn('"data/fonts/NotoSansJP/OFL.txt", "notices/NotoSansJP-OFL.txt"', web)


if __name__ == "__main__":
    unittest.main()
