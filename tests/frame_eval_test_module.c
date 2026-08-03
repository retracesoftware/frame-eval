#define Py_BUILD_CORE_MODULE
#include "Python.h"
#include "frame_eval.h"

PyObject *frame_eval_test(PyThreadState *, struct _PyInterpreterFrame *, int);

#define HOOK_STATE_KEY "_frame_eval_test.hook_state"

typedef struct {
    unsigned long long eval_depth;
    unsigned long long frame_depth;
    unsigned long long native_call_depth;
    unsigned long long max_eval_depth;
    unsigned long long max_frame_depth;
    unsigned long long max_native_call_depth;
    unsigned long long eval_enters;
    unsigned long long eval_exits;
    unsigned long long frame_pushes;
    unsigned long long frame_pops;
    unsigned long long native_call_pres;
    unsigned long long native_call_posts;
    unsigned long long instructions;
    unsigned long long backward_jumps;
    unsigned long long underflows;
} HookState;

static void
hook_state_free(PyObject *capsule)
{
    PyMem_Free(PyCapsule_GetPointer(capsule, HOOK_STATE_KEY));
}

static HookState *
hook_state_get(int create)
{
    PyObject *thread_dict = PyThreadState_GetDict();
    if (thread_dict == NULL) {
        return NULL;
    }
    PyObject *capsule = PyDict_GetItemString(thread_dict, HOOK_STATE_KEY);
    if (capsule != NULL) {
        return PyCapsule_GetPointer(capsule, HOOK_STATE_KEY);
    }
    if (!create) {
        return NULL;
    }
    HookState *state = PyMem_Calloc(1, sizeof(*state));
    if (state == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    capsule = PyCapsule_New(state, HOOK_STATE_KEY, hook_state_free);
    if (capsule == NULL) {
        PyMem_Free(state);
        return NULL;
    }
    if (PyDict_SetItemString(thread_dict, HOOK_STATE_KEY, capsule) < 0) {
        Py_DECREF(capsule);
        return NULL;
    }
    Py_DECREF(capsule);
    return state;
}

static HookState *
hook_state_for_callback(void)
{
    PyObject *type;
    PyObject *value;
    PyObject *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    HookState *state = hook_state_get(1);
    if (state == NULL) {
        PyErr_Clear();
    }
    PyErr_Restore(type, value, traceback);
    return state;
}

static void
increment_depth(unsigned long long *depth, unsigned long long *maximum)
{
    (*depth)++;
    if (*depth > *maximum) {
        *maximum = *depth;
    }
}

static void
decrement_depth(HookState *state, unsigned long long *depth)
{
    if (*depth == 0) {
        state->underflows++;
        return;
    }
    (*depth)--;
}

void
frame_eval_test_eval_enter(void)
{
    HookState *state = hook_state_for_callback();
    if (state != NULL) {
        state->eval_enters++;
        increment_depth(&state->eval_depth, &state->max_eval_depth);
    }
}

void
frame_eval_test_eval_exit(void)
{
    HookState *state = hook_state_for_callback();
    if (state != NULL) {
        state->eval_exits++;
        decrement_depth(state, &state->eval_depth);
    }
}

void
frame_eval_test_instruction(void)
{
    HookState *state = hook_state_for_callback();
    if (state != NULL) {
        state->instructions++;
    }
}

void
frame_eval_test_backward_jump(void)
{
    HookState *state = hook_state_for_callback();
    if (state != NULL) {
        state->backward_jumps++;
    }
}

void
frame_eval_test_frame_push(void)
{
    HookState *state = hook_state_for_callback();
    if (state != NULL) {
        state->frame_pushes++;
        increment_depth(&state->frame_depth, &state->max_frame_depth);
    }
}

void
frame_eval_test_frame_pop(void)
{
    HookState *state = hook_state_for_callback();
    if (state != NULL) {
        state->frame_pops++;
        decrement_depth(state, &state->frame_depth);
    }
}

void
frame_eval_test_native_call_pre(void)
{
    HookState *state = hook_state_for_callback();
    if (state != NULL) {
        state->native_call_pres++;
        increment_depth(&state->native_call_depth,
                        &state->max_native_call_depth);
    }
}

void
frame_eval_test_native_call_post(void)
{
    HookState *state = hook_state_for_callback();
    if (state != NULL) {
        state->native_call_posts++;
        decrement_depth(state, &state->native_call_depth);
    }
}

static PyObject *
installed(PyObject *Py_UNUSED(module), PyObject *Py_UNUSED(ignored))
{
    _PyFrameEvalFunction current =
        _PyInterpreterState_GetEvalFrameFunc(PyInterpreterState_Get());
    return PyBool_FromLong(current == frame_eval || current == frame_eval_test);
}

static PyObject *
install_instrumented(PyObject *Py_UNUSED(module), PyObject *Py_UNUSED(ignored))
{
    if (hook_state_get(1) == NULL) {
        return NULL;
    }
    _PyInterpreterState_SetEvalFrameFunc(PyInterpreterState_Get(),
                                        frame_eval_test);
    Py_RETURN_NONE;
}

static PyObject *
install_default(PyObject *Py_UNUSED(module), PyObject *Py_UNUSED(ignored))
{
    _PyInterpreterState_SetEvalFrameFunc(PyInterpreterState_Get(), frame_eval);
    Py_RETURN_NONE;
}

static PyObject *
hook_counters(PyObject *Py_UNUSED(module), PyObject *Py_UNUSED(ignored))
{
    HookState *state = hook_state_get(1);
    if (state == NULL) {
        return NULL;
    }
    return Py_BuildValue(
        "{s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K}",
        "eval_depth", state->eval_depth,
        "frame_depth", state->frame_depth,
        "native_call_depth", state->native_call_depth,
        "max_eval_depth", state->max_eval_depth,
        "max_frame_depth", state->max_frame_depth,
        "max_native_call_depth", state->max_native_call_depth,
        "eval_enters", state->eval_enters,
        "eval_exits", state->eval_exits,
        "frame_pushes", state->frame_pushes,
        "frame_pops", state->frame_pops,
        "native_call_pres", state->native_call_pres,
        "native_call_posts", state->native_call_posts,
        "instructions", state->instructions,
        "backward_jumps", state->backward_jumps,
        "underflows", state->underflows);
}

static PyObject *
reset_hook_counters(PyObject *Py_UNUSED(module), PyObject *Py_UNUSED(ignored))
{
    HookState *state = hook_state_get(1);
    if (state == NULL) {
        return NULL;
    }
    if (state->eval_depth != 0 || state->frame_depth != 0 ||
        state->native_call_depth != 0) {
        PyErr_SetString(PyExc_RuntimeError,
                        "cannot reset active frame-eval hook counters");
        return NULL;
    }
    memset(state, 0, sizeof(*state));
    Py_RETURN_NONE;
}

static PyMethodDef methods[] = {
    {"installed", installed, METH_NOARGS, NULL},
    {"install_instrumented", install_instrumented, METH_NOARGS, NULL},
    {"install_default", install_default, METH_NOARGS, NULL},
    {"hook_counters", hook_counters, METH_NOARGS, NULL},
    {"reset_hook_counters", reset_hook_counters, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL},
};

static int
module_exec(PyObject *Py_UNUSED(module))
{
    if (frame_eval_init() < 0) {
        return -1;
    }
    _PyInterpreterState_SetEvalFrameFunc(PyInterpreterState_Get(), frame_eval);
    return 0;
}

static PyModuleDef_Slot slots[] = {
    {Py_mod_exec, module_exec},
#if PY_VERSION_HEX >= 0x030C0000
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
#endif
    {0, NULL},
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "_frame_eval_test",
    NULL,
    0,
    methods,
    slots,
};

PyMODINIT_FUNC
PyInit__frame_eval_test(void)
{
    return PyModuleDef_Init(&module);
}
