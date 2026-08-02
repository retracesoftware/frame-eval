import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BOOTSTRAP = ROOT / "tests" / "bootstrap"


class BootstrapTests(unittest.TestCase):
    def test_allows_child_without_extension_environment(self):
        environment = os.environ.copy()
        environment["PYTHONPATH"] = str(BOOTSTRAP)
        environment.pop("FRAME_EVAL_EXTENSION", None)

        subprocess.run(
            [sys.executable, "-c", "pass"],
            check=True,
            env=environment,
        )

    def test_loads_extension_outside_pythonpath(self):
        with tempfile.TemporaryDirectory() as extension_dir:
            extension = Path(extension_dir) / "_frame_eval_test.py"
            extension.write_text("def installed():\n    return True\n")
            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(BOOTSTRAP)
            environment["FRAME_EVAL_EXTENSION"] = extension_dir

            subprocess.run(
                [sys.executable, "-c", "import _frame_eval_test"],
                check=True,
                env=environment,
            )


if __name__ == "__main__":
    unittest.main()