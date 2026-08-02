import importlib.machinery
import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "archive_closure.py"
loader = importlib.machinery.SourceFileLoader("archive_closure", str(SCRIPT))
spec = importlib.util.spec_from_loader(loader.name, loader)
archive_closure = importlib.util.module_from_spec(spec)
loader.exec_module(archive_closure)


class ArchiveClosureTests(unittest.TestCase):
    def test_parse_nm_ignores_weak_undefined_symbols(self):
        defined, undefined = archive_closure.parse_nm(
            "         U strong_reference\n"
            "         w weak_reference\n"
            "00000000 u unique_definition\n"
        )

        self.assertEqual(defined, {"unique_definition"})
        self.assertEqual(undefined, {"strong_reference"})

    def test_stops_at_host_exports_and_follows_hidden_symbols(self):
        member = archive_closure.MemberSymbols
        members = [
            member("root.o", frozenset({"root"}),
                   frozenset({"host_api", "hidden_a", "system_api"})),
            member("a.o", frozenset({"hidden_a"}),
                   frozenset({"hidden_b", "host_data"})),
            member("b.o", frozenset({"hidden_b"}), frozenset()),
            member("unused.o", frozenset({"unused"}), frozenset()),
        ]

        selected, host_bound, unresolved = archive_closure.closure(
            members, {"root", "host_api", "host_data"}, ["root"]
        )

        self.assertEqual(
            [item.name for item in selected],
            ["root.o", "a.o", "b.o"],
        )
        self.assertEqual(host_bound, {"host_api", "host_data"})
        self.assertEqual(unresolved, {"system_api"})

    def test_root_object_definition_overrides_archive_provider(self):
        member = archive_closure.MemberSymbols
        members = [
            member("root.o", frozenset({"root"}), frozenset({"override"})),
            member("stock.o", frozenset({"override"}), frozenset()),
        ]

        selected, _, _ = archive_closure.closure(
            members, set(), ["root"], {"override"}
        )

        self.assertEqual([item.name for item in selected], ["root.o"])

    def test_dependency_layers_batch_each_unresolved_round(self):
        member = archive_closure.MemberSymbols
        members = [
            member("root.o", frozenset({"root"}),
                   frozenset({"left", "right", "host_api"})),
            member("left.o", frozenset({"left"}), frozenset({"leaf"})),
            member("right.o", frozenset({"right"}), frozenset({"leaf"})),
            member("leaf.o", frozenset({"leaf"}), frozenset()),
        ]

        layers, parents = archive_closure.dependency_layers(
            members, {"host_api"}, ["root"]
        )

        self.assertEqual(
            layers,
            [["root.o"], ["left.o", "right.o"], ["leaf.o"]],
        )
        self.assertEqual(
            archive_closure.deepest_chain(layers, parents),
            [("root.o", "root"), ("left.o", "left"), ("leaf.o", "leaf")],
        )

    def test_dependency_layers_force_host_exported_root_provider(self):
        member = archive_closure.MemberSymbols
        members = [member("root.o", frozenset({"root"}), frozenset())]

        layers, _ = archive_closure.dependency_layers(
            members, {"root"}, ["root"]
        )

        self.assertEqual(layers, [["root.o"]])

    def test_selected_sources_maps_archive_members(self):
        member = archive_closure.MemberSymbols
        members = [
            member("ceval.o", frozenset(), frozenset()),
            member("dictobject.o", frozenset(), frozenset()),
        ]

        sources = archive_closure.selected_sources(
            members,
            {
                "ceval.o": "Python/ceval.c",
                "dictobject.o": "Objects/dictobject.c",
            },
        )

        self.assertEqual(sources, ["Python/ceval.c", "Objects/dictobject.c"])


if __name__ == "__main__":
    unittest.main()
