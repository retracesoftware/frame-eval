#!/usr/bin/env python3
"""Extract a minimal CPython source closure rooted at one global symbol."""

import argparse
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

from clang import cindex


DEFINITION_KINDS = {
    cindex.CursorKind.FUNCTION_DECL,
    cindex.CursorKind.VAR_DECL,
}
REFERENCE_KINDS = {
    cindex.CursorKind.FUNCTION_DECL,
    cindex.CursorKind.VAR_DECL,
}
UNDEFINED_TYPES = {"U", "u", "v", "w"}


@dataclass
class Unit:
    source: Path
    translation_unit: object
    definitions_by_usr: dict
    definitions_by_name: dict
    owned_definitions: list


def run(*args, cwd=None):
    return subprocess.run(
        args, cwd=cwd, check=True, capture_output=True, text=True
    ).stdout


def parse_nm(output, definitions_only=False):
    result = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 2 or len(fields[-2]) != 1:
            continue
        symbol_type = fields[-2]
        if definitions_only and symbol_type in UNDEFINED_TYPES:
            continue
        result[fields[-1]] = symbol_type
    return result


def host_exports(host):
    flags = ("-gU",) if platform.system() == "Darwin" else (
        "-D", "-g", "--defined-only"
    )
    return set(parse_nm(run("nm", *flags, str(host)), definitions_only=True))


def archive_index(archive):
    providers = {}
    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        run("ar", "x", str(archive.resolve()), cwd=temporary)
        for member in run("ar", "t", str(archive)).splitlines():
            if member in {"__.SYMDEF", "__.SYMDEF SORTED"}:
                continue
            symbols = parse_nm(
                run("nm", "-g", "--defined-only", str(temporary / member)),
                definitions_only=True,
            )
            for symbol in symbols:
                providers.setdefault(symbol, member)
    return providers


def make_source_map(source_tree):
    rule = "print-retrace-library-objs:\n\t@printf '%s\\n' '$(LIBRARY_OBJS)'"
    objects = set(run(
        "make", "-C", str(source_tree), "--no-print-directory",
        f"--eval={rule}", "print-retrace-library-objs",
    ).split())
    database = run("make", "-C", str(source_tree), "-pn")
    result = {}
    for line in database.splitlines():
        if ":" not in line or line.startswith(("#", "\t")):
            continue
        target, prerequisites = line.split(":", 1)
        if target not in objects:
            continue
        candidates = [
            Path(item) for item in prerequisites.split()
            if Path(item).suffix in {".c", ".s", ".S"}
        ]
        if candidates:
            result[Path(target).name] = candidates[0]
    return result


def make_variable(source_tree, name):
    rule = f"print-retrace-variable:\n\t@printf '%s\\n' '$({name})'"
    return run(
        "make", "-C", str(source_tree), "--no-print-directory", "-s",
        f"--eval={rule}", "print-retrace-variable",
    ).strip()


def cursor_file(cursor):
    location = cursor.location
    if location.file is None:
        return None
    return Path(location.file.name).resolve()


def cursor_key(cursor):
    extent = cursor.extent
    start_file = extent.start.file
    end_file = extent.end.file
    if start_file is None or end_file is None:
        return None
    if Path(start_file.name).resolve() != Path(end_file.name).resolve():
        return None
    return (
        Path(start_file.name).resolve(),
        extent.start.offset,
        extent.end.offset,
        cursor.kind,
        cursor.spelling,
    )


def is_top_level_definition(cursor):
    return (
        cursor.kind in DEFINITION_KINDS
        and cursor.is_definition()
        and cursor.semantic_parent is not None
        and cursor.semantic_parent.kind == cindex.CursorKind.TRANSLATION_UNIT
        and cursor_key(cursor) is not None
    )


def is_owned_file(path, source_tree):
    try:
        relative = path.relative_to(source_tree)
    except ValueError:
        return False
    return not relative.parts or relative.parts[0] != "Include"


def walk(cursor):
    yield cursor
    for child in cursor.get_children():
        yield from walk(child)


class Extractor:
    def __init__(self, source_tree, providers, source_map, exports, clang_args):
        self.source_tree = source_tree.resolve()
        self.providers = providers
        self.source_map = source_map
        self.exports = exports
        self.clang_args = clang_args
        self.index = cindex.Index.create()
        self.units = {}
        self.selected = set()
        self.selected_units = set()
        self.layers = []
        self.external_dependencies = set()
        self.unresolved = set()

    def load_unit(self, source):
        source = source.resolve()
        cached = self.units.get(source)
        if cached is not None:
            return cached
        translation_unit = self.index.parse(
            str(source),
            args=self.clang_args,
            options=cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD,
        )
        errors = [
            diagnostic for diagnostic in translation_unit.diagnostics
            if diagnostic.severity >= cindex.Diagnostic.Error
        ]
        if errors:
            details = "\n".join(str(error) for error in errors[:10])
            raise RuntimeError(f"Clang failed to parse {source}:\n{details}")

        definitions_by_usr = {}
        definitions_by_name = defaultdict(list)
        owned_definitions = []
        for cursor in walk(translation_unit.cursor):
            if not is_top_level_definition(cursor):
                continue
            path = cursor_file(cursor)
            if path is None:
                continue
            usr = cursor.get_usr()
            if usr:
                definitions_by_usr[usr] = cursor
            definitions_by_name[cursor.spelling].append(cursor)
            if is_owned_file(path, self.source_tree):
                owned_definitions.append(cursor)
        unit = Unit(
            source,
            translation_unit,
            definitions_by_usr,
            definitions_by_name,
            owned_definitions,
        )
        self.units[source] = unit
        return unit

    def source_for_symbol(self, symbol):
        member = self.providers.get(symbol)
        if member is None:
            return None
        relative = self.source_map.get(member)
        if relative is None:
            raise ValueError(f"no source path for provider {member} of {symbol}")
        return self.source_tree / relative

    def definition_for_symbol(self, symbol):
        source = self.source_for_symbol(symbol)
        if source is None:
            return None, None
        unit = self.load_unit(source)
        candidates = unit.definitions_by_name.get(symbol, ())
        if not candidates:
            raise ValueError(f"Clang found no definition of {symbol} in {source}")
        return unit, candidates[0]

    def local_definition(self, unit, referenced):
        usr = referenced.get_usr()
        if usr:
            definition = unit.definitions_by_usr.get(usr)
            if definition is not None:
                return definition
        definition = referenced.get_definition()
        if definition is not None and is_top_level_definition(definition):
            return definition
        return None

    def referenced_definitions(self, unit, definition):
        local = []
        external = set()
        for cursor in walk(definition):
            referenced = cursor.referenced
            if referenced is None or referenced.kind not in REFERENCE_KINDS:
                continue
            name = referenced.spelling
            if (
                name in self.exports
                and referenced.linkage == cindex.LinkageKind.EXTERNAL
            ):
                external.add(name)
                continue
            local_definition = self.local_definition(unit, referenced)
            if local_definition is not None:
                key = cursor_key(local_definition)
                if key is not None:
                    local.append(local_definition)
                continue
            if name:
                external.add(name)
        return local, external

    def extract(self, root):
        _, root_definition = self.definition_for_symbol(root)
        if root_definition is None:
            raise ValueError(f"no archive provider for root symbol {root}")
        pending = [root_definition]
        while pending:
            next_definitions = []
            layer = []
            for definition in pending:
                key = cursor_key(definition)
                if key is None or key in self.selected:
                    continue
                self.selected.add(key)
                layer.append(definition)
                unit = self.unit_for_definition(definition)
                self.selected_units.add(unit.source)
                local, external = self.referenced_definitions(unit, definition)
                next_definitions.extend(local)
                for symbol in external:
                    if symbol in self.exports:
                        self.external_dependencies.add(symbol)
                        continue
                    _, provider = self.definition_for_symbol(symbol)
                    if provider is None:
                        self.unresolved.add(symbol)
                    else:
                        next_definitions.append(provider)
            if layer:
                self.layers.append(layer)
            pending = next_definitions

    def unit_for_definition(self, definition):
        path = cursor_file(definition)
        for unit in self.units.values():
            if path == unit.source or definition.get_usr() in unit.definitions_by_usr:
                return unit
        raise ValueError(f"definition is not owned by a loaded unit: {definition.spelling}")

    def write(self, output):
        files = defaultdict(list)
        selected_files = {key[0] for key in self.selected}
        selected_ranges = {
            (key[0], key[1], key[2]) for key in self.selected
        }
        for unit in self.units.values():
            if unit.source in self.selected_units:
                files[unit.source]
                for definition in unit.owned_definitions:
                    files[cursor_file(definition)].append(definition)
            for definition in unit.owned_definitions:
                path = cursor_file(definition)
                if path in selected_files:
                    files[path].append(definition)

        output.mkdir(parents=True)
        manifest = []
        for path, definitions in sorted(files.items()):
            relative = path.relative_to(self.source_tree)
            content = path.read_bytes()
            removals = set()
            for definition in definitions:
                key = cursor_key(definition)
                if key is None:
                    continue
                source_range = (key[0], key[1], key[2])
                if source_range in selected_ranges:
                    continue
                removals.add((key[1], key[2]))
            merged = []
            for start, end in sorted(removals):
                if merged and start <= merged[-1][1]:
                    merged[-1] = (merged[-1][0], max(merged[-1][1], end))
                else:
                    merged.append((start, end))
            for start, end in reversed(merged):
                content = content[:start] + content[end:]
            destination = output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(content)
            if path.suffix in {".c", ".s", ".S"}:
                manifest.append(relative)
        (output / "sources.txt").write_text(
            "".join(f"{source}\n" for source in sorted(manifest))
        )
        (output / "selected-definitions.tsv").write_text("".join(
            f"{key[0].relative_to(self.source_tree)}\t{key[4]}\n"
            for key in sorted(self.selected, key=lambda item: (str(item[0]), item[1]))
        ))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--source-tree", required=True, type=Path)
    parser.add_argument("--root", required=True)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--libclang")
    args = parser.parse_args()

    if args.libclang:
        cindex.Config.set_library_file(args.libclang)

    source_tree = args.source_tree.resolve()
    version_parts = args.version.removeprefix("v").split(".")
    if len(version_parts) != 3 or not all(part.isdigit() for part in version_parts):
        parser.error("--version must be an exact CPython release such as 3.12.8")
    major, minor, _ = version_parts
    python_version = f"{major}.{minor}"
    soabi = make_variable(source_tree, "SOABI")
    clang_args = [
        "-x", "c", "-std=c11", "-DNDEBUG", "-DPy_BUILD_CORE",
        f"-I{source_tree / 'Include/internal'}",
        f"-I{source_tree}",
        f"-I{source_tree / 'Include'}",
        f'-DSOABI="{soabi}"',
        '-DPYTHONPATH=""',
        '-DPREFIX="/usr/local"',
        '-DEXEC_PREFIX="/usr/local"',
        f'-DVERSION="{python_version}"',
        '-DVPATH=""',
        '-DPLATLIBDIR="lib"',
        '-DPYTHONFRAMEWORK=""',
    ]
    extractor = Extractor(
        source_tree,
        archive_index(args.archive),
        make_source_map(source_tree),
        host_exports(args.host),
        clang_args,
    )
    extractor.extract(args.root)

    output = args.out.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
        generated = Path(temporary) / "internal-clang-src"
        extractor.write(generated)
        if output.exists():
            shutil.rmtree(output)
        generated.rename(output)

    print(f"definition passes: {len(extractor.layers)}")
    print("definitions per pass: " + ", ".join(
        str(len(layer)) for layer in extractor.layers
    ))
    print(f"selected definitions: {len(extractor.selected)}")
    print(f"source files: {len(extractor.selected_units)}")
    print(f"host dependencies: {len(extractor.external_dependencies)}")
    print(f"unresolved non-CPython dependencies: {len(extractor.unresolved)}")
    print(f"output: {output}")


if __name__ == "__main__":
    main()
