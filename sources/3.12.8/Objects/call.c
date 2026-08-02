#include "Python.h"
#include "pycore_call.h"          // _PyObject_CallNoArgsTstate()
#include "pycore_ceval.h"         // _Py_EnterRecursiveCallTstate()
#include "pycore_dict.h"          // _PyDict_FromItems()
#include "pycore_object.h"        // _PyCFunctionWithKeywords_TrampolineCall()
#include "pycore_pyerrors.h"      // _PyErr_Occurred()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_tuple.h"         // _PyTuple_ITEMS()











/* --- Core PyObject call functions ------------------------------- */

/* Call a callable Python object without any arguments */








































/* --- PyFunction call functions ---------------------------------- */



/* --- More complex call functions -------------------------------- */

/* External interface to call any callable object.
   The args must be a tuple or NULL.  The kwargs must be a dict or NULL. */






/* Call callable(obj, *args, **kwargs). */



/* --- Call with a format string ---------------------------------- */







/* PyEval_CallFunction is exact copy of PyObject_CallFunction.
 * This function is kept for backward compatibility.
 */











/* PyEval_CallMethod is exact copy of PyObject_CallMethod.
 * This function is kept for backward compatibility.
 */


















/* --- Call with "..." arguments ---------------------------------- */
















/* --- PyStack functions ------------------------------------------ */




/* Convert (args, nargs, kwargs: dict) into a (stack, nargs, kwnames: tuple).

   Allocate a new argument vector and keyword names tuple. Return the argument
   vector; return NULL with exception set on error. Return the keyword names
   tuple in *p_kwnames.

   This also checks that all keyword names are strings. If not, a TypeError is
   raised.

   The newly allocated argument vector supports PY_VECTORCALL_ARGUMENTS_OFFSET.

   When done, you must call _PyStack_UnpackDict_Free(stack, nargs, kwnames) */
PyObject *const *
_PyStack_UnpackDict(PyThreadState *tstate,
                    PyObject *const *args, Py_ssize_t nargs,
                    PyObject *kwargs, PyObject **p_kwnames)
{
    assert(nargs >= 0);
    assert(kwargs != NULL);
    assert(PyDict_Check(kwargs));

    Py_ssize_t nkwargs = PyDict_GET_SIZE(kwargs);
    /* Check for overflow in the PyMem_Malloc() call below. The subtraction
     * in this check cannot overflow: both maxnargs and nkwargs are
     * non-negative signed integers, so their difference fits in the type. */
    Py_ssize_t maxnargs = PY_SSIZE_T_MAX / sizeof(args[0]) - 1;
    if (nargs > maxnargs - nkwargs) {
        _PyErr_NoMemory(tstate);
        return NULL;
    }

    /* Add 1 to support PY_VECTORCALL_ARGUMENTS_OFFSET */
    PyObject **stack = PyMem_Malloc((1 + nargs + nkwargs) * sizeof(args[0]));
    if (stack == NULL) {
        _PyErr_NoMemory(tstate);
        return NULL;
    }

    PyObject *kwnames = PyTuple_New(nkwargs);
    if (kwnames == NULL) {
        PyMem_Free(stack);
        return NULL;
    }

    stack++;  /* For PY_VECTORCALL_ARGUMENTS_OFFSET */

    /* Copy positional arguments */
    for (Py_ssize_t i = 0; i < nargs; i++) {
        stack[i] = Py_NewRef(args[i]);
    }

    PyObject **kwstack = stack + nargs;
    /* This loop doesn't support lookup function mutating the dictionary
       to change its size. It's a deliberate choice for speed, this function is
       called in the performance critical hot code. */
    Py_ssize_t pos = 0, i = 0;
    PyObject *key, *value;
    unsigned long keys_are_strings = Py_TPFLAGS_UNICODE_SUBCLASS;
    while (PyDict_Next(kwargs, &pos, &key, &value)) {
        keys_are_strings &= Py_TYPE(key)->tp_flags;
        PyTuple_SET_ITEM(kwnames, i, Py_NewRef(key));
        kwstack[i] = Py_NewRef(value);
        i++;
    }

    /* keys_are_strings has the value Py_TPFLAGS_UNICODE_SUBCLASS if that
     * flag is set for all keys. Otherwise, keys_are_strings equals 0.
     * We do this check once at the end instead of inside the loop above
     * because it simplifies the deallocation in the failing case.
     * It happens to also make the loop above slightly more efficient. */
    if (!keys_are_strings) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "keywords must be strings");
        _PyStack_UnpackDict_Free(stack, nargs, kwnames);
        return NULL;
    }

    *p_kwnames = kwnames;
    return stack;
}

void
_PyStack_UnpackDict_Free(PyObject *const *stack, Py_ssize_t nargs,
                         PyObject *kwnames)
{
    Py_ssize_t n = PyTuple_GET_SIZE(kwnames) + nargs;
    for (Py_ssize_t i = 0; i < n; i++) {
        Py_DECREF(stack[i]);
    }
    _PyStack_UnpackDict_FreeNoDecRef(stack, kwnames);
}

void
_PyStack_UnpackDict_FreeNoDecRef(PyObject *const *stack, PyObject *kwnames)
{
    PyMem_Free((PyObject **)stack - 1);
    Py_DECREF(kwnames);
}

// Export for the stable ABI
#undef PyVectorcall_NARGS

