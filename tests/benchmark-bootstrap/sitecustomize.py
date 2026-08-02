import os
import sys


if "PYPERFORMANCE_RUNID" in os.environ:
    sys.path.insert(0, os.environ["FRAME_EVAL_EXTENSION"])
    try:
        import _frame_eval_test

        if not _frame_eval_test.installed():
            raise RuntimeError("generated frame evaluator was not installed")
    except Exception:
        import traceback

        traceback.print_exc()
        sys.stderr.write("frame-eval benchmark bootstrap failed; aborting\n")
        os._exit(70)