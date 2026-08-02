/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


PyDoc_STRVAR(dict_fromkeys__doc__,
"fromkeys($type, iterable, value=None, /)\n"
"--\n"
"\n"
"Create a new dictionary with keys from iterable and values set to value.");

#define DICT_FROMKEYS_METHODDEF    \
    {"fromkeys", _PyCFunction_CAST(dict_fromkeys), METH_FASTCALL|METH_CLASS, dict_fromkeys__doc__},

static PyObject *
dict_fromkeys_impl(PyTypeObject *type, PyObject *iterable, PyObject *value);



PyDoc_STRVAR(dict___contains____doc__,
"__contains__($self, key, /)\n"
"--\n"
"\n"
"True if the dictionary has the specified key, else False.");

#define DICT___CONTAINS___METHODDEF    \
    {"__contains__", (PyCFunction)dict___contains__, METH_O|METH_COEXIST, dict___contains____doc__},

PyDoc_STRVAR(dict_get__doc__,
"get($self, key, default=None, /)\n"
"--\n"
"\n"
"Return the value for key if key is in the dictionary, else default.");

#define DICT_GET_METHODDEF    \
    {"get", _PyCFunction_CAST(dict_get), METH_FASTCALL, dict_get__doc__},

static PyObject *
dict_get_impl(PyDictObject *self, PyObject *key, PyObject *default_value);



PyDoc_STRVAR(dict_setdefault__doc__,
"setdefault($self, key, default=None, /)\n"
"--\n"
"\n"
"Insert key with a value of default if key is not in the dictionary.\n"
"\n"
"Return the value for key if key is in the dictionary, else default.");

#define DICT_SETDEFAULT_METHODDEF    \
    {"setdefault", _PyCFunction_CAST(dict_setdefault), METH_FASTCALL, dict_setdefault__doc__},

static PyObject *
dict_setdefault_impl(PyDictObject *self, PyObject *key,
                     PyObject *default_value);



PyDoc_STRVAR(dict_pop__doc__,
"pop($self, key, default=<unrepresentable>, /)\n"
"--\n"
"\n"
"D.pop(k[,d]) -> v, remove specified key and return the corresponding value.\n"
"\n"
"If the key is not found, return the default if given; otherwise,\n"
"raise a KeyError.");

#define DICT_POP_METHODDEF    \
    {"pop", _PyCFunction_CAST(dict_pop), METH_FASTCALL, dict_pop__doc__},

static PyObject *
dict_pop_impl(PyDictObject *self, PyObject *key, PyObject *default_value);



PyDoc_STRVAR(dict_popitem__doc__,
"popitem($self, /)\n"
"--\n"
"\n"
"Remove and return a (key, value) pair as a 2-tuple.\n"
"\n"
"Pairs are returned in LIFO (last-in, first-out) order.\n"
"Raises KeyError if the dict is empty.");

#define DICT_POPITEM_METHODDEF    \
    {"popitem", (PyCFunction)dict_popitem, METH_NOARGS, dict_popitem__doc__},

static PyObject *
dict_popitem_impl(PyDictObject *self);



PyDoc_STRVAR(dict___reversed____doc__,
"__reversed__($self, /)\n"
"--\n"
"\n"
"Return a reverse iterator over the dict keys.");

#define DICT___REVERSED___METHODDEF    \
    {"__reversed__", (PyCFunction)dict___reversed__, METH_NOARGS, dict___reversed____doc__},

static PyObject *
dict___reversed___impl(PyDictObject *self);


/*[clinic end generated code: output=c0064abbea6091c5 input=a9049054013a1b77]*/
