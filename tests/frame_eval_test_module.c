#define Py_BUILD_CORE_MODULE
#include "Python.h"
#include "frame_eval.h"

PyObject *frame_eval_test(PyThreadState *, struct _PyInterpreterFrame *, int);

static PyObject *
installed(PyObject *Py_UNUSED(module), PyObject *Py_UNUSED(ignored))
{
    _PyFrameEvalFunction current =
        _PyInterpreterState_GetEvalFrameFunc(PyInterpreterState_Get());
    return PyBool_FromLong(current == frame_eval_test);
}

static PyMethodDef methods[] = {
    {"installed", installed, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "_frame_eval_test",
    NULL,
    -1,
    methods,
};

PyMODINIT_FUNC
PyInit__frame_eval_test(void)
{
    if (frame_eval_init() < 0) {
        return NULL;
    }
    PyObject *result = PyModule_Create(&module);
    if (result == NULL) {
        return NULL;
    }
    _PyInterpreterState_SetEvalFrameFunc(PyInterpreterState_Get(),
                                         frame_eval_test);
    return result;
}
