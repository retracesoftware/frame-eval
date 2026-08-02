/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


PyDoc_STRVAR(tuple_index__doc__,
"index($self, value, start=0, stop=sys.maxsize, /)\n"
"--\n"
"\n"
"Return first index of value.\n"
"\n"
"Raises ValueError if the value is not present.");

#define TUPLE_INDEX_METHODDEF    \
    {"index", _PyCFunction_CAST(tuple_index), METH_FASTCALL, tuple_index__doc__},

static PyObject *
tuple_index_impl(PyTupleObject *self, PyObject *value, Py_ssize_t start,
                 Py_ssize_t stop);



PyDoc_STRVAR(tuple_count__doc__,
"count($self, value, /)\n"
"--\n"
"\n"
"Return number of occurrences of value.");

#define TUPLE_COUNT_METHODDEF    \
    {"count", (PyCFunction)tuple_count, METH_O, tuple_count__doc__},

PyDoc_STRVAR(tuple_new__doc__,
"tuple(iterable=(), /)\n"
"--\n"
"\n"
"Built-in immutable sequence.\n"
"\n"
"If no argument is given, the constructor returns an empty tuple.\n"
"If iterable is specified the tuple is initialized from iterable\'s items.\n"
"\n"
"If the argument is a tuple, the return value is the same object.");

static PyObject *
tuple_new_impl(PyTypeObject *type, PyObject *iterable);



PyDoc_STRVAR(tuple___getnewargs____doc__,
"__getnewargs__($self, /)\n"
"--\n"
"\n");

#define TUPLE___GETNEWARGS___METHODDEF    \
    {"__getnewargs__", (PyCFunction)tuple___getnewargs__, METH_NOARGS, tuple___getnewargs____doc__},

static PyObject *
tuple___getnewargs___impl(PyTupleObject *self);


/*[clinic end generated code: output=48a9e0834b300ac3 input=a9049054013a1b77]*/
