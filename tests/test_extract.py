import importlib.machinery
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "extract.py"
loader = importlib.machinery.SourceFileLoader("extract", str(SCRIPT))
spec = importlib.util.spec_from_loader(loader.name, loader)
extract = importlib.util.module_from_spec(spec)
loader.exec_module(extract)


class ExtractTests(unittest.TestCase):
    def test_write_copies_transitive_local_headers_of_included_c(self):
        with tempfile.TemporaryDirectory() as temporary:
            source_tree = Path(temporary) / "cpython"
            source = source_tree / "Python" / "ceval.c"
            generated_header = source_tree / "Python" / "opcode_targets.h"
            included_c = source_tree / "Python" / "bytecodes.c"
            nested_header = source_tree / "Python" / "optimizer.h"
            indirect_header = source_tree / "Python" / "indirect.h"
            remote_header = source_tree / "Objects" / "remote.h"
            public_header = source_tree / "Include" / "Python.h"
            for path in (
                source,
                generated_header,
                included_c,
                nested_header,
                indirect_header,
                remote_header,
                public_header,
            ):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(f"/* {path.name} */\n")

            inclusions = [
                SimpleNamespace(
                    source=SimpleNamespace(name=str(includer)),
                    include=SimpleNamespace(name=str(included)),
                )
                for includer, included in (
                    (source, generated_header),
                    (source, included_c),
                    (included_c, nested_header),
                    (nested_header, indirect_header),
                    (included_c, remote_header),
                    (source, public_header),
                )
            ]
            translation_unit = SimpleNamespace(
                get_includes=lambda: inclusions,
            )
            unit = extract.Unit(source, translation_unit, {}, {}, [])
            extractor = extract.Extractor.__new__(extract.Extractor)
            extractor.source_tree = source_tree
            extractor.units = {source: unit}
            extractor.selected_units = {source}
            extractor.selected = set()

            output = Path(temporary) / "output"
            extractor.write(output)

            self.assertTrue((output / "Python" / "opcode_targets.h").is_file())
            self.assertTrue((output / "Python" / "bytecodes.c").is_file())
            self.assertTrue((output / "Python" / "optimizer.h").is_file())
            self.assertTrue((output / "Python" / "indirect.h").is_file())
            self.assertFalse((output / "Objects" / "remote.h").exists())
            self.assertFalse((output / "Include" / "Python.h").exists())
            self.assertEqual(
                (output / "sources.txt").read_text(),
                "Python/bytecodes.c\nPython/ceval.c\n",
            )


if __name__ == "__main__":
    unittest.main()
