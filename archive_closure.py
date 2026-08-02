#!/usr/bin/env python3
"""Compute the static-archive member closure required by root symbols."""

import argparse
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
import platform
import subprocess
import sys
import tempfile


UNDEFINED_TYPES = {"U"}
WEAK_UNDEFINED_TYPES = {"v", "w"}


@dataclass(frozen=True)
class MemberSymbols:
    name: str
    defined: frozenset[str]
    undefined: frozenset[str]


def run(*args, cwd=None):
    return subprocess.run(
        args, cwd=cwd, check=True, capture_output=True, text=True
    ).stdout


def parse_nm(output):
    defined = set()
    undefined = set()
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        symbol_type = parts[-2]
        if len(symbol_type) != 1:
            continue
        symbol = parts[-1]
        if symbol_type in UNDEFINED_TYPES:
            undefined.add(symbol)
        elif symbol_type in WEAK_UNDEFINED_TYPES:
            continue
        else:
            defined.add(symbol)
    return frozenset(defined), frozenset(undefined)


def object_symbols(objects):
    defined = set()
    undefined = set()
    for path in objects:
        object_defined, object_undefined = parse_nm(run("nm", "-g", str(path)))
        defined.update(object_defined)
        undefined.update(object_undefined)
    return defined, undefined - defined


def host_exports(host, system=None):
    system = system or platform.system()
    flags = ("-gU",) if system == "Darwin" else (
        "-D", "-g", "--defined-only"
    )
    defined, _ = parse_nm(run("nm", *flags, str(host)))
    return defined


def archive_symbols(archive):
    archive = archive.resolve()
    names = [line for line in run("ar", "t", str(archive)).splitlines()
             if line not in {"__.SYMDEF", "__.SYMDEF SORTED"}]
    if len(names) != len(set(names)):
        raise ValueError("archive contains duplicate member names")

    members = []
    with tempfile.TemporaryDirectory() as temporary:
        run("ar", "x", str(archive), cwd=temporary)
        for name in names:
            defined, undefined = parse_nm(
                run("nm", "-g", str(Path(temporary) / name))
            )
            members.append(MemberSymbols(name, defined, undefined))
    return members


def make_source_map(source_tree):
    print_rule = (
        "print-retrace-library-objs:\n"
        "\t@printf '%s\\n' '$(LIBRARY_OBJS)'"
    )
    library_objects = set(run(
        "make", "-C", str(source_tree), "--no-print-directory",
        f"--eval={print_rule}", "print-retrace-library-objs",
    ).split())
    output = run("make", "-C", str(source_tree), "-pn")
    sources = {}
    for line in output.splitlines():
        if ":" not in line or line.startswith(("#", "\t")):
            continue
        target, prerequisites = line.split(":", 1)
        if target not in library_objects:
            continue
        candidates = [
            item for item in prerequisites.split()
            if Path(item).suffix in {".c", ".s", ".S"}
        ]
        if not candidates:
            continue
        source = candidates[0]
        member = Path(target).name
        previous = sources.get(member)
        if previous is not None and previous != source:
            raise ValueError(
                f"archive member {member} maps to both {previous} and {source}"
            )
        sources[member] = source
    return sources


def selected_sources(members, source_map):
    result = []
    for member in members:
        try:
            result.append(source_map[member.name])
        except KeyError as error:
            raise ValueError(
                f"no source path found for archive member {member.name}"
            ) from error
    return result


def closure(members, exports, roots, available_definitions=()):
    providers = defaultdict(list)
    for member in members:
        for symbol in member.defined:
            providers[symbol].append(member)

    pending = deque()
    selected = []
    selected_names = set()
    selected_definitions = set(available_definitions)
    host_bound = set()
    unresolved = set()

    for root in roots:
        candidates = providers.get(root)
        if not candidates:
            unresolved.add(root)
            continue
        member = candidates[0]
        if member.name not in selected_names:
            selected.append(member)
            selected_names.add(member.name)
            selected_definitions.update(member.defined)
            pending.extend(sorted(member.undefined))

    while pending:
        symbol = pending.popleft()
        if symbol in selected_definitions:
            continue
        if symbol in exports:
            host_bound.add(symbol)
            continue
        candidates = providers.get(symbol)
        if not candidates:
            unresolved.add(symbol)
            continue

        member = candidates[0]
        if member.name in selected_names:
            continue
        selected.append(member)
        selected_names.add(member.name)
        selected_definitions.update(member.defined)
        pending.extend(sorted(member.undefined))

    missed = {
        symbol
        for member in selected
        for symbol in member.undefined
        if symbol not in exports
        and symbol not in selected_definitions
        and symbol in providers
    }
    if missed:
        raise RuntimeError(
            "closure stopped before archive providers: "
            + ", ".join(sorted(missed))
        )
    return selected, host_bound, unresolved


def dependency_layers(members, exports, roots, available_definitions=()):
    providers = defaultdict(list)
    members_by_name = {member.name: member for member in members}
    for member in members:
        for symbol in member.defined:
            providers[symbol].append(member)

    available = set(available_definitions)
    selected = set()
    parents = {}
    pending = [(None, symbol, True) for symbol in sorted(roots)]
    layers = []

    while pending:
        layer = []
        for parent, symbol, force_provider in pending:
            if symbol in available or (symbol in exports and not force_provider):
                continue
            candidates = providers.get(symbol)
            if not candidates:
                continue
            member = candidates[0]
            if member.name in selected:
                continue
            selected.add(member.name)
            parents[member.name] = (parent, symbol)
            layer.append(member.name)

        if not layer:
            break
        layers.append(layer)
        for name in layer:
            available.update(members_by_name[name].defined)

        pending = []
        for name in layer:
            for symbol in sorted(members_by_name[name].undefined):
                pending.append((name, symbol, False))

    return layers, parents


def deepest_chain(layers, parents):
    if not layers:
        return []
    member = layers[-1][0]
    chain = []
    while member is not None:
        parent, symbol = parents[member]
        chain.append((member, symbol))
        member = parent
    chain.reverse()
    return chain


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--root", action="append", default=[])
    parser.add_argument("--root-object", action="append", default=[], type=Path)
    parser.add_argument("--members-out", type=Path)
    parser.add_argument("--source-tree", type=Path)
    parser.add_argument("--sources-out", type=Path)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if not args.root and not args.root_object:
        parser.error("at least one --root or --root-object is required")
    if bool(args.source_tree) != bool(args.sources_out):
        parser.error("--source-tree and --sources-out must be used together")

    members = archive_symbols(args.archive)
    exports = host_exports(args.host)
    root_definitions, object_roots = object_symbols(args.root_object)
    roots = set(args.root)
    roots.update(object_roots)
    selected, host_bound, unresolved = closure(
        members, exports, roots, root_definitions
    )
    layers, parents = dependency_layers(
        members, exports, roots, root_definitions
    )

    if args.members_out:
        args.members_out.write_text(
            "".join(f"{member.name}\n" for member in selected)
        )
    if args.sources_out:
        source_map = make_source_map(args.source_tree)
        sources = selected_sources(selected, source_map)
        args.sources_out.write_text("".join(f"{source}\n" for source in sources))

    print(f"archive members: {len(members)}")
    print(f"host exports: {len(exports)}")
    print(f"root symbols: {len(roots)}")
    print(f"selected members: {len(selected)}")
    print(f"dependency passes: {len(layers)}")
    print(f"host-bound symbols: {len(host_bound)}")
    print(f"unresolved external symbols: {len(unresolved)}")
    if layers:
        print("members per pass: " + ", ".join(str(len(layer)) for layer in layers))
        print("deepest chain: " + " -> ".join(
            f"{member}[{symbol}]"
            for member, symbol in deepest_chain(layers, parents)
        ))
    if args.verbose:
        print("\ndependency passes:")
        for index, layer in enumerate(layers, 1):
            print(f"pass {index}: " + ", ".join(layer))
        print("\nselected archive members:")
        for member in selected:
            print(member.name)
    if args.verbose and unresolved:
        print("\nunresolved external symbols:")
        for symbol in sorted(unresolved):
            print(symbol)
    return 0


if __name__ == "__main__":
    sys.exit(main())
