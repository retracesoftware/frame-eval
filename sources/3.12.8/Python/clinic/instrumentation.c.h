/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


PyDoc_STRVAR(monitoring_use_tool_id__doc__,
"use_tool_id($module, tool_id, name, /)\n"
"--\n"
"\n");

#define MONITORING_USE_TOOL_ID_METHODDEF    \
    {"use_tool_id", _PyCFunction_CAST(monitoring_use_tool_id), METH_FASTCALL, monitoring_use_tool_id__doc__},

static PyObject *
monitoring_use_tool_id_impl(PyObject *module, int tool_id, PyObject *name);



PyDoc_STRVAR(monitoring_free_tool_id__doc__,
"free_tool_id($module, tool_id, /)\n"
"--\n"
"\n");

#define MONITORING_FREE_TOOL_ID_METHODDEF    \
    {"free_tool_id", (PyCFunction)monitoring_free_tool_id, METH_O, monitoring_free_tool_id__doc__},

static PyObject *
monitoring_free_tool_id_impl(PyObject *module, int tool_id);



PyDoc_STRVAR(monitoring_get_tool__doc__,
"get_tool($module, tool_id, /)\n"
"--\n"
"\n");

#define MONITORING_GET_TOOL_METHODDEF    \
    {"get_tool", (PyCFunction)monitoring_get_tool, METH_O, monitoring_get_tool__doc__},

static PyObject *
monitoring_get_tool_impl(PyObject *module, int tool_id);



PyDoc_STRVAR(monitoring_register_callback__doc__,
"register_callback($module, tool_id, event, func, /)\n"
"--\n"
"\n");

#define MONITORING_REGISTER_CALLBACK_METHODDEF    \
    {"register_callback", _PyCFunction_CAST(monitoring_register_callback), METH_FASTCALL, monitoring_register_callback__doc__},

static PyObject *
monitoring_register_callback_impl(PyObject *module, int tool_id, int event,
                                  PyObject *func);



PyDoc_STRVAR(monitoring_get_events__doc__,
"get_events($module, tool_id, /)\n"
"--\n"
"\n");

#define MONITORING_GET_EVENTS_METHODDEF    \
    {"get_events", (PyCFunction)monitoring_get_events, METH_O, monitoring_get_events__doc__},

static int
monitoring_get_events_impl(PyObject *module, int tool_id);



PyDoc_STRVAR(monitoring_set_events__doc__,
"set_events($module, tool_id, event_set, /)\n"
"--\n"
"\n");

#define MONITORING_SET_EVENTS_METHODDEF    \
    {"set_events", _PyCFunction_CAST(monitoring_set_events), METH_FASTCALL, monitoring_set_events__doc__},

static PyObject *
monitoring_set_events_impl(PyObject *module, int tool_id, int event_set);



PyDoc_STRVAR(monitoring_get_local_events__doc__,
"get_local_events($module, tool_id, code, /)\n"
"--\n"
"\n");

#define MONITORING_GET_LOCAL_EVENTS_METHODDEF    \
    {"get_local_events", _PyCFunction_CAST(monitoring_get_local_events), METH_FASTCALL, monitoring_get_local_events__doc__},

static int
monitoring_get_local_events_impl(PyObject *module, int tool_id,
                                 PyObject *code);



PyDoc_STRVAR(monitoring_set_local_events__doc__,
"set_local_events($module, tool_id, code, event_set, /)\n"
"--\n"
"\n");

#define MONITORING_SET_LOCAL_EVENTS_METHODDEF    \
    {"set_local_events", _PyCFunction_CAST(monitoring_set_local_events), METH_FASTCALL, monitoring_set_local_events__doc__},

static PyObject *
monitoring_set_local_events_impl(PyObject *module, int tool_id,
                                 PyObject *code, int event_set);



PyDoc_STRVAR(monitoring_restart_events__doc__,
"restart_events($module, /)\n"
"--\n"
"\n");

#define MONITORING_RESTART_EVENTS_METHODDEF    \
    {"restart_events", (PyCFunction)monitoring_restart_events, METH_NOARGS, monitoring_restart_events__doc__},

static PyObject *
monitoring_restart_events_impl(PyObject *module);



PyDoc_STRVAR(monitoring__all_events__doc__,
"_all_events($module, /)\n"
"--\n"
"\n");

#define MONITORING__ALL_EVENTS_METHODDEF    \
    {"_all_events", (PyCFunction)monitoring__all_events, METH_NOARGS, monitoring__all_events__doc__},

static PyObject *
monitoring__all_events_impl(PyObject *module);


/*[clinic end generated code: output=11cc0803875b3ffa input=a9049054013a1b77]*/
