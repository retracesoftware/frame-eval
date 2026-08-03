import _frame_eval_test


def leaf(value):
    return len((value,))


def exercise_hooks():
    total = 0
    for value in range(12):
        total += leaf(value)
    return total


_frame_eval_test.install_instrumented()
try:
    _frame_eval_test.reset_hook_counters()
    assert exercise_hooks() == 12
finally:
    _frame_eval_test.install_default()

counters = _frame_eval_test.hook_counters()
for depth in ("eval_depth", "frame_depth", "native_call_depth"):
    assert counters[depth] == 0, counters

for before, after in (
    ("eval_enters", "eval_exits"),
    ("frame_pushes", "frame_pops"),
    ("native_call_pres", "native_call_posts"),
):
    assert counters[before] > 0, counters
    assert counters[before] == counters[after], counters

assert counters["instructions"] > 0, counters
assert counters["backward_jumps"] > 0, counters
assert counters["max_eval_depth"] > 0, counters
assert counters["max_frame_depth"] > 1, counters
assert counters["max_native_call_depth"] > 0, counters
assert counters["underflows"] == 0, counters