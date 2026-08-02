from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PatchTests(unittest.TestCase):
    def test_all_patches_parse(self):
        for patch in sorted((ROOT / "patches").glob("*.patch")):
            with self.subTest(patch=patch.name):
                subprocess.run(
                    ["git", "apply", "--numstat", "--", str(patch)],
                    cwd=ROOT,
                    check=True,
                    stdout=subprocess.DEVNULL,
                )
