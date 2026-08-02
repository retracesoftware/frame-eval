#define Py_BUILD_CORE_MODULE
#include "Python.h"
#include "frame_eval.h"

static PyObject *
installed(PyObject *Py_UNUSED(module), PyObject *Py_UNUSED(ignored))
{
    _PyFrameEvalFunction current =
        _PyInterpreterState_GetEvalFrameFunc(PyInterpreterState_Get());
    return PyBool_FromLong(current == frame_eval);
}

static PyMethodDef methods[] = {
    {"installed", installed, METH_NOARGS, NULL},
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
