/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


PyDoc_STRVAR(list_insert__doc__,
"insert($self, index, object, /)\n"
"--\n"
"\n"
"Insert object before index.");

#define LIST_INSERT_METHODDEF    \
    {"insert", _PyCFunction_CAST(list_insert), METH_FASTCALL, list_insert__doc__},

static PyObject *
list_insert_impl(PyListObject *self, Py_ssize_t index, PyObject *object);



PyDoc_STRVAR(list_clear__doc__,
"clear($self, /)\n"
"--\n"
"\n"
"Remove all items from list.");

#define LIST_CLEAR_METHODDEF    \
    {"clear", (PyCFunction)list_clear, METH_NOARGS, list_clear__doc__},

static PyObject *
list_clear_impl(PyListObject *self);



PyDoc_STRVAR(list_copy__doc__,
"copy($self, /)\n"
"--\n"
"\n"
"Return a shallow copy of the list.");

#define LIST_COPY_METHODDEF    \
    {"copy", (PyCFunction)list_copy, METH_NOARGS, list_copy__doc__},

static PyObject *
list_copy_impl(PyListObject *self);



PyDoc_STRVAR(list_append__doc__,
"append($self, object, /)\n"
"--\n"
"\n"
"Append object to the end of the list.");

#define LIST_APPEND_METHODDEF    \
    {"append", (PyCFunction)list_append, METH_O, list_append__doc__},

PyDoc_STRVAR(list_extend__doc__,
"extend($self, iterable, /)\n"
"--\n"
"\n"
"Extend list by appending elements from the iterable.");

#define LIST_EXTEND_METHODDEF    \
    {"extend", (PyCFunction)list_extend, METH_O, list_extend__doc__},

PyDoc_STRVAR(list_pop__doc__,
"pop($self, index=-1, /)\n"
"--\n"
"\n"
"Remove and return item at index (default last).\n"
"\n"
"Raises IndexError if list is empty or index is out of range.");

#define LIST_POP_METHODDEF    \
    {"pop", _PyCFunction_CAST(list_pop), METH_FASTCALL, list_pop__doc__},

static PyObject *
list_pop_impl(PyListObject *self, Py_ssize_t index);



PyDoc_STRVAR(list_sort__doc__,
"sort($self, /, *, key=None, reverse=False)\n"
"--\n"
"\n"
"Sort the list in ascending order and return None.\n"
"\n"
"The sort is in-place (i.e. the list itself is modified) and stable (i.e. the\n"
"order of two equal elements is maintained).\n"
"\n"
"If a key function is given, apply it once to each list item and sort them,\n"
"ascending or descending, according to their function values.\n"
"\n"
"The reverse flag can be set to sort in descending order.");

#define LIST_SORT_METHODDEF    \
    {"sort", _PyCFunction_CAST(list_sort), METH_FASTCALL|METH_KEYWORDS, list_sort__doc__},

static PyObject *
list_sort_impl(PyListObject *self, PyObject *keyfunc, int reverse);



PyDoc_STRVAR(list_reverse__doc__,
"reverse($self, /)\n"
"--\n"
"\n"
"Reverse *IN PLACE*.");

#define LIST_REVERSE_METHODDEF    \
    {"reverse", (PyCFunction)list_reverse, METH_NOARGS, list_reverse__doc__},

static PyObject *
list_reverse_impl(PyListObject *self);



PyDoc_STRVAR(list_index__doc__,
"index($self, value, start=0, stop=sys.maxsize, /)\n"
"--\n"
"\n"
"Return first index of value.\n"
"\n"
"Raises ValueError if the value is not present.");

#define LIST_INDEX_METHODDEF    \
    {"index", _PyCFunction_CAST(list_index), METH_FASTCALL, list_index__doc__},

static PyObject *
list_index_impl(PyListObject *self, PyObject *value, Py_ssize_t start,
                Py_ssize_t stop);



PyDoc_STRVAR(list_count__doc__,
"count($self, value, /)\n"
"--\n"
"\n"
"Return number of occurrences of value.");

#define LIST_COUNT_METHODDEF    \
    {"count", (PyCFunction)list_count, METH_O, list_count__doc__},

PyDoc_STRVAR(list_remove__doc__,
"remove($self, value, /)\n"
"--\n"
"\n"
"Remove first occurrence of value.\n"
"\n"
"Raises ValueError if the value is not present.");

#define LIST_REMOVE_METHODDEF    \
    {"remove", (PyCFunction)list_remove, METH_O, list_remove__doc__},

PyDoc_STRVAR(list___init____doc__,
"list(iterable=(), /)\n"
"--\n"
"\n"
"Built-in mutable sequence.\n"
"\n"
"If no argument is given, the constructor creates a new empty list.\n"
"The argument must be an iterable if specified.");

static int
list___init___impl(PyListObject *self, PyObject *iterable);



PyDoc_STRVAR(list___sizeof____doc__,
"__sizeof__($self, /)\n"
"--\n"
"\n"
"Return the size of the list in memory, in bytes.");

#define LIST___SIZEOF___METHODDEF    \
    {"__sizeof__", (PyCFunction)list___sizeof__, METH_NOARGS, list___sizeof____doc__},

static PyObject *
list___sizeof___impl(PyListObject *self);



PyDoc_STRVAR(list___reversed____doc__,
"__reversed__($self, /)\n"
"--\n"
"\n"
"Return a reverse iterator over the list.");

#define LIST___REVERSED___METHODDEF    \
    {"__reversed__", (PyCFunction)list___reversed__, METH_NOARGS, list___reversed____doc__},

static PyObject *
list___reversed___impl(PyListObject *self);


/*[clinic end generated code: output=2ca109d8acc775bc input=a9049054013a1b77]*/
