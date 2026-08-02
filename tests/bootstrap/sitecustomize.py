import os
import sys

try:
    sys.path.insert(0, os.environ["FRAME_EVAL_EXTENSION"])
    import _frame_eval_test

    if not _frame_eval_test.installed():
        raise RuntimeError("generated frame evaluator was not installed")
except Exception:
    import traceback

    traceback.print_exc()
    sys.stderr.write("frame-eval test bootstrap failed; aborting\n")
    os._exit(70)
