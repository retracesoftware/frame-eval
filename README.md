# Frame Eval

This repository owns the reproducible source closure rooted at
`_PyEval_EvalFrameDefault` for each exact supported CPython release.

## Layout

- `regenerate`: complete exact-release update workflow.
- `extract.py`: libclang AST closure and source-range extraction.
- `validate`: isolated compilation and host-export symbol audit.
- `patches/<version>.patch`: changes applied after building vanilla CPython but
  before extraction. These redirect process-owned state and shape the closure.
- `templates/`: generated-tree support headers.
- `archive_closure.py`: archive-level closure diagnostic used by unit tests.

Generated source sets are workflow artifacts consumed and checked in by
`retrace-eval`. They retain upstream relative paths so later Retrace patches
can operate on recognizable CPython source.

## Regenerate a release

```bash
./regenerate 3.12.8
```

The command:

1. Clones the exact `v3.12.8` CPython tag into
   `build/v3.12.8/cpython`.
2. Configures and builds the unmodified interpreter and `libpython.a`.
3. Applies `patches/3.12.8.patch` to the source checkout. The already-built
   executable and archive remain the authoritative vanilla host contract.
4. Extracts the source closure rooted at `_PyEval_EvalFrameDefault`.
5. Compiles the staged closure and verifies that every remaining CPython
   dependency is exported by the exact host.
6. Replaces `build/workflow/sources/3.12.8` only after validation succeeds.

The snapshot's `.generated` file records the exact CPython commit and SHA-256
hashes of every transformation input. Generate it locally with:

```bash
make frame-eval-sources VERSION=3.12.8
```

That command regenerates only when an input is newer than the local artifact.
To deliberately regenerate from unchanged inputs, use `make
regenerate-frame-eval VERSION=3.12.8`.

`CPYTHON_REPO_URL`, `JOBS`, `CC`, and `LIBCLANG` may override the repository,
parallelism, compiler, and libclang shared library. The workflow creates a
private Python environment under `build/.venv` using
`requirements.txt`.

A version is supported only when its exact patch exists. This intentionally
prevents silently applying one patch release's assumptions to another.

The `Build source set` workflow accepts an immutable `frame-eval` ref and an
exact CPython tag. It generates and validates that release, then uploads
`build/workflow/frame-eval-<version>.tar.gz` containing the source set and
CPython license. It never modifies a repository.

## Validate an existing snapshot

`validate` accepts its build inputs through environment variables. Regeneration
sets these automatically. For an already configured source and matching host:

```bash
SOURCE_TREE=/path/to/cpython \
HOST_PYTHON=/path/to/python \
LIBPYTHON_ARCHIVE=/path/to/libpython.a \
./validate 3.12.8
```

The validator rejects `_Py_tss_tstate`, `_PyThreadState_SwapNoGIL`, and any
external CPython symbol not exported by the matching executable.

Copied CPython files are distributed under [LICENSE](LICENSE).
