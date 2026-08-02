#include "frame_eval.h"

PyDictKeysObject *frame_eval_empty_keys = NULL;
PyObject *frame_eval_monitoring_disable = NULL;
PyObject *frame_eval_monitoring_missing = NULL;
PyTypeObject *frame_eval_typealias_type = NULL;
PyTypeObject *frame_eval_union_type = NULL;
getattrofunc frame_eval_slot_tp_getattr_hook = NULL;
getattrofunc frame_eval_slot_tp_getattro = NULL;
vectorcallfunc frame_eval_function_vectorcall = NULL;

enum FrameEvalStateStatus {
    FRAME_EVAL_STATE_UNINITIALIZED,
    FRAME_EVAL_STATE_INITIALIZING,
    FRAME_EVAL_STATE_INITIALIZED,
};

static enum FrameEvalStateStatus frame_eval_state_status =
    FRAME_EVAL_STATE_UNINITIALIZED;

static PyObject *
frame_eval_import_attr(const char *module_name, const char *attr_name)
{
    PyObject *module = PyImport_ImportModule(module_name);
    if (module == NULL) {
        return NULL;
    }
    PyObject *value = PyObject_GetAttrString(module, attr_name);
    Py_DECREF(module);
    return value;
}

static PyObject *
frame_eval_monitoring_attr(const char *attr_name)
{
    PyObject *monitoring = frame_eval_import_attr("sys", "monitoring");
    if (monitoring == NULL) {
        return NULL;
    }
    PyObject *value = PyObject_GetAttrString(monitoring, attr_name);
    Py_DECREF(monitoring);
    return value;
}

static int
frame_eval_capture_type(const char *module_name, const char *attr_name,
                        PyTypeObject **destination)
{
    PyObject *value = frame_eval_import_attr(module_name, attr_name);
    if (value == NULL) {
        return -1;
    }
    if (!PyType_Check(value)) {
        Py_DECREF(value);
        PyErr_Format(PyExc_RuntimeError, "%s.%s is not a type",
                     module_name, attr_name);
        return -1;
    }
    *destination = (PyTypeObject *)value;
    return 0;
}

static int
frame_eval_capture_call_slots(getattrofunc *getattr_hook,
                              getattrofunc *getattro,
                              vectorcallfunc *function_vectorcall)
{
    PyObject *globals = PyDict_New();
    if (globals == NULL) {
        return -1;
    }
    PyObject *result = PyRun_String(
        "class _FrameEvalGetattr:\n"
        "    def __getattr__(self, name): return None\n"
        "class _FrameEvalGetattribute:\n"
        "    def __getattribute__(self, name): return None\n"
        "def _frame_eval_function(): pass\n"
        "_FrameEvalGetattribute()._frame_eval_probe\n",
        Py_file_input, globals, globals);
    if (result == NULL) {
        Py_DECREF(globals);
        return -1;
    }
    Py_DECREF(result);
    PyObject *getattr_type = PyDict_GetItemString(globals, "_FrameEvalGetattr");
    PyObject *getattribute_type = PyDict_GetItemString(
        globals, "_FrameEvalGetattribute");
    PyObject *function = PyDict_GetItemString(globals, "_frame_eval_function");
    if (!PyType_Check(getattr_type) || !PyType_Check(getattribute_type) ||
        !PyFunction_Check(function)) {
        Py_DECREF(globals);
        PyErr_SetString(PyExc_RuntimeError,
                        "failed to create frame-eval slot probe types");
        return -1;
    }
    *getattr_hook = ((PyTypeObject *)getattr_type)->tp_getattro;
    *getattro = ((PyTypeObject *)getattribute_type)->tp_getattro;
    *function_vectorcall = ((PyFunctionObject *)function)->vectorcall;
    Py_DECREF(globals);
    return 0;
}

int
frame_eval_init(void)
{
    if (frame_eval_state_status == FRAME_EVAL_STATE_INITIALIZED) {
        return 0;
    }
    if (frame_eval_state_status == FRAME_EVAL_STATE_INITIALIZING) {
        PyErr_SetString(PyExc_RuntimeError,
                        "recursive frame-eval initialization");
        return -1;
    }
    frame_eval_state_status = FRAME_EVAL_STATE_INITIALIZING;

    PyObject *monitoring_disable = NULL;
    PyObject *monitoring_missing = NULL;
    PyTypeObject *typealias_type = NULL;
    PyTypeObject *union_type = NULL;
    getattrofunc slot_tp_getattr_hook = NULL;
    getattrofunc slot_tp_getattro = NULL;
    vectorcallfunc function_vectorcall = NULL;

    PyObject *empty_dict = PyDict_New();
    if (empty_dict == NULL) {
        goto fail;
    }
    PyDictKeysObject *empty_keys = ((PyDictObject *)empty_dict)->ma_keys;
    Py_DECREF(empty_dict);

#if PY_VERSION_HEX >= 0x030C0000
    monitoring_disable = frame_eval_monitoring_attr("DISABLE");
    if (monitoring_disable == NULL) {
        goto fail;
    }
    monitoring_missing = frame_eval_monitoring_attr("MISSING");
    if (monitoring_missing == NULL) {
        goto fail;
    }
    if (frame_eval_capture_type("_typing", "TypeAliasType",
                                &typealias_type) < 0) {
        goto fail;
    }
#endif
    if (frame_eval_capture_type("types", "UnionType", &union_type) < 0) {
        goto fail;
    }
    if (frame_eval_capture_call_slots(&slot_tp_getattr_hook,
                                      &slot_tp_getattro,
                                      &function_vectorcall) < 0) {
        goto fail;
    }

    frame_eval_empty_keys = empty_keys;
    frame_eval_monitoring_disable = monitoring_disable;
    frame_eval_monitoring_missing = monitoring_missing;
    frame_eval_typealias_type = typealias_type;
    frame_eval_union_type = union_type;
    frame_eval_slot_tp_getattr_hook = slot_tp_getattr_hook;
    frame_eval_slot_tp_getattro = slot_tp_getattro;
    frame_eval_function_vectorcall = function_vectorcall;
    frame_eval_state_status = FRAME_EVAL_STATE_INITIALIZED;
    return 0;

fail:
    Py_XDECREF(monitoring_disable);
    Py_XDECREF(monitoring_missing);
    Py_XDECREF((PyObject *)typealias_type);
    Py_XDECREF((PyObject *)union_type);
    frame_eval_state_status = FRAME_EVAL_STATE_UNINITIALIZED;
    return -1;
}