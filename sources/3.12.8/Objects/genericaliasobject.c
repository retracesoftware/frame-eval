// types.GenericAlias -- used to represent e.g. list[int].

#include "Python.h"
#include "pycore_object.h"
#include "pycore_unionobject.h"   // _Py_union_type_or, _PyGenericAlias_Check
#include "structmember.h"         // PyMemberDef

#include <stdbool.h>

typedef struct {
    PyObject_HEAD
    PyObject *origin;
    PyObject *args;
    PyObject *parameters;
    PyObject *weakreflist;
    // Whether we're a starred type, e.g. *tuple[int].
    bool starred;
    vectorcallfunc vectorcall;
} gaobject;

typedef struct {
    PyObject_HEAD
    PyObject *obj;  /* Set to NULL when iterator is exhausted */
} gaiterobject;











// Index of item in self[:len], or -1 if not found (self is a tuple)
static Py_ssize_t
tuple_index(PyObject *self, Py_ssize_t len, PyObject *item)
{
    for (Py_ssize_t i = 0; i < len; i++) {
        if (PyTuple_GET_ITEM(self, i) == item) {
            return i;
        }
    }
    return -1;
}

static int
tuple_add(PyObject *self, Py_ssize_t len, PyObject *item)
{
    if (tuple_index(self, len, item) < 0) {
        PyTuple_SET_ITEM(self, len, Py_NewRef(item));
        return 1;
    }
    return 0;
}

static Py_ssize_t
tuple_extend(PyObject **dst, Py_ssize_t dstindex,
             PyObject **src, Py_ssize_t count)
{
    assert(count >= 0);
    if (_PyTuple_Resize(dst, PyTuple_GET_SIZE(*dst) + count - 1) != 0) {
        return -1;
    }
    assert(dstindex + count <= PyTuple_GET_SIZE(*dst));
    for (Py_ssize_t i = 0; i < count; ++i) {
        PyObject *item = src[i];
        PyTuple_SET_ITEM(*dst, dstindex + i, Py_NewRef(item));
    }
    return dstindex + count;
}

PyObject *
_Py_make_parameters(PyObject *args)
{
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);
    Py_ssize_t len = nargs;
    PyObject *parameters = PyTuple_New(len);
    if (parameters == NULL)
        return NULL;
    Py_ssize_t iparam = 0;
    for (Py_ssize_t iarg = 0; iarg < nargs; iarg++) {
        PyObject *t = PyTuple_GET_ITEM(args, iarg);
        PyObject *subst;
        // We don't want __parameters__ descriptor of a bare Python class.
        if (PyType_Check(t)) {
            continue;
        }
        if (_PyObject_LookupAttr(t, &_Py_ID(__typing_subst__), &subst) < 0) {
            Py_DECREF(parameters);
            return NULL;
        }
        if (subst) {
            iparam += tuple_add(parameters, iparam, t);
            Py_DECREF(subst);
        }
        else {
            PyObject *subparams;
            if (_PyObject_LookupAttr(t, &_Py_ID(__parameters__),
                                     &subparams) < 0) {
                Py_DECREF(parameters);
                return NULL;
            }
            if (subparams && PyTuple_Check(subparams)) {
                Py_ssize_t len2 = PyTuple_GET_SIZE(subparams);
                Py_ssize_t needed = len2 - 1 - (iarg - iparam);
                if (needed > 0) {
                    len += needed;
                    if (_PyTuple_Resize(&parameters, len) < 0) {
                        Py_DECREF(subparams);
                        Py_DECREF(parameters);
                        return NULL;
                    }
                }
                for (Py_ssize_t j = 0; j < len2; j++) {
                    PyObject *t2 = PyTuple_GET_ITEM(subparams, j);
                    iparam += tuple_add(parameters, iparam, t2);
                }
            }
            Py_XDECREF(subparams);
        }
    }
    if (iparam < len) {
        if (_PyTuple_Resize(&parameters, iparam) < 0) {
            Py_XDECREF(parameters);
            return NULL;
        }
    }
    return parameters;
}

/* If obj is a generic alias, substitute type variables params
   with substitutions argitems.  For example, if obj is list[T],
   params is (T, S), and argitems is (str, int), return list[str].
   If obj doesn't have a __parameters__ attribute or that's not
   a non-empty tuple, return a new reference to obj. */
static PyObject *
subs_tvars(PyObject *obj, PyObject *params,
           PyObject **argitems, Py_ssize_t nargs)
{
    PyObject *subparams;
    if (_PyObject_LookupAttr(obj, &_Py_ID(__parameters__), &subparams) < 0) {
        return NULL;
    }
    if (subparams && PyTuple_Check(subparams) && PyTuple_GET_SIZE(subparams)) {
        Py_ssize_t nparams = PyTuple_GET_SIZE(params);
        Py_ssize_t nsubargs = PyTuple_GET_SIZE(subparams);
        PyObject *subargs = PyTuple_New(nsubargs);
        if (subargs == NULL) {
            Py_DECREF(subparams);
            return NULL;
        }
        Py_ssize_t j = 0;
        for (Py_ssize_t i = 0; i < nsubargs; ++i) {
            PyObject *arg = PyTuple_GET_ITEM(subparams, i);
            Py_ssize_t iparam = tuple_index(params, nparams, arg);
            if (iparam >= 0) {
                PyObject *param = PyTuple_GET_ITEM(params, iparam);
                arg = argitems[iparam];
                if (Py_TYPE(param)->tp_iter && PyTuple_Check(arg)) {  // TypeVarTuple
                    j = tuple_extend(&subargs, j,
                                    &PyTuple_GET_ITEM(arg, 0),
                                    PyTuple_GET_SIZE(arg));
                    if (j < 0) {
                        return NULL;
                    }
                    continue;
                }
            }
            PyTuple_SET_ITEM(subargs, j, Py_NewRef(arg));
            j++;
        }
        assert(j == PyTuple_GET_SIZE(subargs));

        obj = PyObject_GetItem(obj, subargs);

        Py_DECREF(subargs);
    }
    else {
        Py_INCREF(obj);
    }
    Py_XDECREF(subparams);
    return obj;
}

static int
_is_unpacked_typevartuple(PyObject *arg)
{
    PyObject *tmp;
    if (PyType_Check(arg)) { // TODO: Add test
        return 0;
    }
    int res = _PyObject_LookupAttr(arg, &_Py_ID(__typing_is_unpacked_typevartuple__), &tmp);
    if (res > 0) {
        res = PyObject_IsTrue(tmp);
        Py_DECREF(tmp);
    }
    return res;
}

static PyObject *
_unpacked_tuple_args(PyObject *arg)
{
    PyObject *result;
    assert(!PyType_Check(arg));
    // Fast path
    if (_PyGenericAlias_Check(arg) &&
            ((gaobject *)arg)->starred &&
            ((gaobject *)arg)->origin == (PyObject *)&PyTuple_Type)
    {
        result = ((gaobject *)arg)->args;
        return Py_NewRef(result);
    }

    if (_PyObject_LookupAttr(arg, &_Py_ID(__typing_unpacked_tuple_args__), &result) > 0) {
        if (result == Py_None) {
            Py_DECREF(result);
            return NULL;
        }
        return result;
    }
    return NULL;
}

static PyObject *
_unpack_args(PyObject *item)
{
    PyObject *newargs = PyList_New(0);
    if (newargs == NULL) {
        return NULL;
    }
    int is_tuple = PyTuple_Check(item);
    Py_ssize_t nitems = is_tuple ? PyTuple_GET_SIZE(item) : 1;
    PyObject **argitems = is_tuple ? &PyTuple_GET_ITEM(item, 0) : &item;
    for (Py_ssize_t i = 0; i < nitems; i++) {
        item = argitems[i];
        if (!PyType_Check(item)) {
            PyObject *subargs = _unpacked_tuple_args(item);
            if (subargs != NULL &&
                PyTuple_Check(subargs) &&
                !(PyTuple_GET_SIZE(subargs) &&
                  PyTuple_GET_ITEM(subargs, PyTuple_GET_SIZE(subargs)-1) == Py_Ellipsis))
            {
                if (PyList_SetSlice(newargs, PY_SSIZE_T_MAX, PY_SSIZE_T_MAX, subargs) < 0) {
                    Py_DECREF(subargs);
                    Py_DECREF(newargs);
                    return NULL;
                }
                Py_DECREF(subargs);
                continue;
            }
            Py_XDECREF(subargs);
            if (PyErr_Occurred()) {
                Py_DECREF(newargs);
                return NULL;
            }
        }
        if (PyList_Append(newargs, item) < 0) {
            Py_DECREF(newargs);
            return NULL;
        }
    }
    Py_SETREF(newargs, PySequence_Tuple(newargs));
    return newargs;
}

PyObject *
_Py_subs_parameters(PyObject *self, PyObject *args, PyObject *parameters, PyObject *item)
{
    Py_ssize_t nparams = PyTuple_GET_SIZE(parameters);
    if (nparams == 0) {
        return PyErr_Format(PyExc_TypeError,
                            "%R is not a generic class",
                            self);
    }
    item = _unpack_args(item);
    for (Py_ssize_t i = 0; i < nparams; i++) {
        PyObject *param = PyTuple_GET_ITEM(parameters, i);
        PyObject *prepare, *tmp;
        if (_PyObject_LookupAttr(param, &_Py_ID(__typing_prepare_subst__), &prepare) < 0) {
            Py_DECREF(item);
            return NULL;
        }
        if (prepare && prepare != Py_None) {
            if (PyTuple_Check(item)) {
                tmp = PyObject_CallFunction(prepare, "OO", self, item);
            }
            else {
                tmp = PyObject_CallFunction(prepare, "O(O)", self, item);
            }
            Py_DECREF(prepare);
            Py_SETREF(item, tmp);
            if (item == NULL) {
                return NULL;
            }
        }
    }
    int is_tuple = PyTuple_Check(item);
    Py_ssize_t nitems = is_tuple ? PyTuple_GET_SIZE(item) : 1;
    PyObject **argitems = is_tuple ? &PyTuple_GET_ITEM(item, 0) : &item;
    if (nitems != nparams) {
        Py_DECREF(item);
        return PyErr_Format(PyExc_TypeError,
                            "Too %s arguments for %R; actual %zd, expected %zd",
                            nitems > nparams ? "many" : "few",
                            self, nitems, nparams);
    }
    /* Replace all type variables (specified by parameters)
       with corresponding values specified by argitems.
        t = list[T];          t[int]      -> newargs = [int]
        t = dict[str, T];     t[int]      -> newargs = [str, int]
        t = dict[T, list[S]]; t[str, int] -> newargs = [str, list[int]]
     */
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);
    PyObject *newargs = PyTuple_New(nargs);
    if (newargs == NULL) {
        Py_DECREF(item);
        return NULL;
    }
    for (Py_ssize_t iarg = 0, jarg = 0; iarg < nargs; iarg++) {
        PyObject *arg = PyTuple_GET_ITEM(args, iarg);
        if (PyType_Check(arg)) {
            PyTuple_SET_ITEM(newargs, jarg, Py_NewRef(arg));
            jarg++;
            continue;
        }

        int unpack = _is_unpacked_typevartuple(arg);
        if (unpack < 0) {
            Py_DECREF(newargs);
            Py_DECREF(item);
            return NULL;
        }
        PyObject *subst;
        if (_PyObject_LookupAttr(arg, &_Py_ID(__typing_subst__), &subst) < 0) {
            Py_DECREF(newargs);
            Py_DECREF(item);
            return NULL;
        }
        if (subst) {
            Py_ssize_t iparam = tuple_index(parameters, nparams, arg);
            assert(iparam >= 0);
            arg = PyObject_CallOneArg(subst, argitems[iparam]);
            Py_DECREF(subst);
        }
        else {
            arg = subs_tvars(arg, parameters, argitems, nitems);
        }
        if (arg == NULL) {
            Py_DECREF(newargs);
            Py_DECREF(item);
            return NULL;
        }
        if (unpack) {
            jarg = tuple_extend(&newargs, jarg,
                    &PyTuple_GET_ITEM(arg, 0), PyTuple_GET_SIZE(arg));
            Py_DECREF(arg);
            if (jarg < 0) {
                Py_DECREF(item);
                return NULL;
            }
        }
        else {
            PyTuple_SET_ITEM(newargs, jarg, arg);
            jarg++;
        }
    }

    Py_DECREF(item);
    return newargs;
}

PyDoc_STRVAR(genericalias__doc__,
"Represent a PEP 585 generic type\n"
"\n"
"E.g. for t = list[int], t.__origin__ is list and t.__args__ is (int,).");



;









;















;

;





;

/* A helper function to create GenericAlias' args tuple and set its attributes.
 * Returns 1 on success, 0 on failure.
 */




;











;

// gh-91632: _Py_GenericAliasIterType is exported  to be cleared
// in _PyTypes_FiniTypes.
;



// TODO:
// - argument clinic?
// - cache?
;


