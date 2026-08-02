# Design

## Preserve CPython Optimizations

Every release patch must preserve Python-to-Python inlining and the other
specializations that CPython enables for its default evaluator. Installing
`frame_eval` must not make CPython treat PEP 523 as an unknown custom evaluator
and fall back to deoptimized execution.

Patch every relevant `eval_frame` guard in the exact release, including guards
in generated interpreter cases, specialization code, and inlined frame-dispatch
macros. The guard must accept both CPython's default evaluator and `frame_eval`,
using `FRAME_EVAL_SPECIALIZATION_ALLOWED()` or
`FRAME_EVAL_SPECIALIZATION_ALLOWED_INTERP()` as appropriate. Preserve adjacent
function-vectorcall and slot checks by binding them to the corresponding host
runtime values when copied code cannot safely refer to private host symbols.

This is a correctness requirement, not an optional performance improvement. A
patch is incomplete if it compiles but disables Python-to-Python inlining or
other default-evaluator optimizations. Review each patch against its exact
CPython tag, because generated cases and specialization guards move between
patch releases.

Validation for every supported release must include:

1. Apply its patch to the exact CPython tag with zero fuzz.
2. Regenerate and compile the extracted closure against that exact host.
3. Exercise Python calls, generators, monitoring, and tracing through the
   installed evaluator.
4. Inspect all `eval_frame` checks reachable from the extracted evaluator and
   confirm that none reject `frame_eval` where the default evaluator is allowed.
