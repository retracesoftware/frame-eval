import os
import sys

extension_dir = os.environ.get("FRAME_EVAL_EXTENSION")
if extension_dir is not None:
    try:
        sys.path.insert(0, extension_dir)
        import _frame_eval_test

        if not _frame_eval_test.installed():
            raise RuntimeError("generated frame evaluator was not installed")
    except Exception:
        import traceback

        traceback.print_exc()
        sys.stderr.write("frame-eval test bootstrap failed; aborting\n")
        os._exit(70)
