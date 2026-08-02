
/* Tuple object implementation */

#include "Python.h"
#include "pycore_abstract.h"      // _PyIndex_Check()
#include "pycore_gc.h"            // _PyObject_GC_IS_TRACKED()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_object.h"        // _PyObject_GC_TRACK(), _Py_FatalRefcountError()

/*[clinic input]
class tuple "PyTupleObject *" "&PyTuple_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=f051ba3cfdf9a189]*/

#include "clinic/tupleobject.c.h"


static inline PyTupleObject * maybe_freelist_pop(Py_ssize_t);
static inline int maybe_freelist_push(PyTupleObject *);


/* Allocate an uninitialized tuple object. Before making it public, following
   steps must be done:

   - Initialize its items.
   - Call _PyObject_GC_TRACK() on it.

   Because the empty tuple is always reused and it's already tracked by GC,
   this function must not be called with size == 0 (unless from PyTuple_New()
   which wraps this function).
*/
static PyTupleObject *
tuple_alloc(Py_ssize_t size)
{
    if (size < 0) {
        PyErr_BadInternalCall();
        return NULL;
    }
#ifdef Py_DEBUG
    assert(size != 0);    // The empty tuple is statically allocated.
#endif

    PyTupleObject *op = maybe_freelist_pop(size);
    if (op == NULL) {
        /* Check for overflow */
        if ((size_t)size > ((size_t)PY_SSIZE_T_MAX - (sizeof(PyTupleObject) -
                    sizeof(PyObject *))) / sizeof(PyObject *)) {
            return (PyTupleObject *)PyErr_NoMemory();
        }
        op = PyObject_GC_NewVar(PyTupleObject, &PyTuple_Type, size);
        if (op == NULL)
            return NULL;
    }
    return op;
}

// The empty tuple singleton is not tracked by the GC.
// It does not contain any Python object.
// Note that tuple subclasses have their own empty instances.

static inline PyObject *
tuple_get_empty(void)
{
    return Py_NewRef(&_Py_SINGLETON(tuple_empty));
}














/* Methods */






/* Hash for tuples. This is a slightly simplified version of the xxHash
   non-cryptographic hash:
   - we do not use any parallelism, there is only 1 accumulator.
   - we drop the final mixing since this is just a permutation of the
     output space: it does not help against collisions.
   - at the end, we mangle the length with a single constant.
   For the xxHash specification, see
   https://github.com/Cyan4973/xxHash/blob/master/doc/xxhash_spec.md

   Below are the official constants from the xxHash specification. Optimizing
   compilers should emit a single "rotate" instruction for the
   _PyHASH_XXROTATE() expansion. If that doesn't happen for some important
   platform, the macro could be changed to expand to a platform-specific rotate
   spelling instead.
*/
#if SIZEOF_PY_UHASH_T > 4
#define _PyHASH_XXPRIME_1 ((Py_uhash_t)11400714785074694791ULL)
#define _PyHASH_XXPRIME_2 ((Py_uhash_t)14029467366897019727ULL)
#define _PyHASH_XXPRIME_5 ((Py_uhash_t)2870177450012600261ULL)
#define _PyHASH_XXROTATE(x) ((x << 31) | (x >> 33))  /* Rotate left 31 bits */
#else
#define _PyHASH_XXPRIME_1 ((Py_uhash_t)2654435761UL)
#define _PyHASH_XXPRIME_2 ((Py_uhash_t)2246822519UL)
#define _PyHASH_XXPRIME_5 ((Py_uhash_t)374761393UL)
#define _PyHASH_XXROTATE(x) ((x << 13) | (x >> 19))  /* Rotate left 13 bits */
#endif

/* Tests have shown that it's not worth to cache the hash value, see
   https://bugs.python.org/issue9685 */








PyObject *
_PyTuple_FromArray(PyObject *const *src, Py_ssize_t n)
{
    if (n == 0) {
        return tuple_get_empty();
    }

    PyTupleObject *tuple = tuple_alloc(n);
    if (tuple == NULL) {
        return NULL;
    }
    PyObject **dst = tuple->ob_item;
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *item = src[i];
        dst[i] = Py_NewRef(item);
    }
    _PyObject_GC_TRACK(tuple);
    return (PyObject *)tuple;
}

PyObject *
_PyTuple_FromArraySteal(PyObject *const *src, Py_ssize_t n)
{
    if (n == 0) {
        return tuple_get_empty();
    }
    PyTupleObject *tuple = tuple_alloc(n);
    if (tuple == NULL) {
        for (Py_ssize_t i = 0; i < n; i++) {
            Py_DECREF(src[i]);
        }
        return NULL;
    }
    PyObject **dst = tuple->ob_item;
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *item = src[i];
        dst[i] = item;
    }
    _PyObject_GC_TRACK(tuple);
    return (PyObject *)tuple;
}









/*[clinic input]
tuple.index

    value: object
    start: slice_index(accept={int}) = 0
    stop: slice_index(accept={int}, c_default="PY_SSIZE_T_MAX") = sys.maxsize
    /

Return first index of value.

Raises ValueError if the value is not present.
[clinic start generated code]*/



/*[clinic input]
tuple.count

     value: object
     /

Return number of occurrences of value.
[clinic start generated code]*/







static PyObject *
tuple_subtype_new(PyTypeObject *type, PyObject *iterable);

/*[clinic input]
@classmethod
tuple.__new__ as tuple_new
    iterable: object(c_default="NULL") = ()
    /

Built-in immutable sequence.

If no argument is given, the constructor returns an empty tuple.
If iterable is specified the tuple is initialized from iterable's items.

If the argument is a tuple, the return value is the same object.
[clinic start generated code]*/







;



/*[clinic input]
tuple.__getnewargs__
[clinic start generated code]*/



;

;

static PyObject *tuple_iter(PyObject *seq);

;

/* The following function breaks the notion that tuples are immutable:
   it changes the size of a tuple.  We get away with this only if there
   is only one module referencing the object.  You can also think of it
   as creating a new tuple object and destroying the old one, only more
   efficiently.  In any case, don't use this if the tuple may already be
   known to some other part of the code. */




static void maybe_freelist_clear(PyInterpreterState *, int);



void
_PyTuple_ClearFreeList(PyInterpreterState *interp)
{
    maybe_freelist_clear(interp, 0);
}

/*********************** Tuple Iterator **************************/










PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");





PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");
PyDoc_STRVAR(setstate_doc, "Set state information for unpickling.");

;

;




/*************
 * freelists *
 *************/

#define STATE (interp->tuple)
#define FREELIST_FINALIZED (STATE.numfree[0] < 0)

static inline PyTupleObject *
maybe_freelist_pop(Py_ssize_t size)
{
#if PyTuple_NFREELISTS > 0
    PyInterpreterState *interp = _PyInterpreterState_GET();
#ifdef Py_DEBUG
    /* maybe_freelist_pop() must not be called after maybe_freelist_fini(). */
    assert(!FREELIST_FINALIZED);
#endif
    if (size == 0) {
        return NULL;
    }
    assert(size > 0);
    if (size <= PyTuple_MAXSAVESIZE) {
        Py_ssize_t index = size - 1;
        PyTupleObject *op = STATE.free_list[index];
        if (op != NULL) {
            /* op is the head of a linked list, with the first item
               pointing to the next node.  Here we pop off the old head. */
            STATE.free_list[index] = (PyTupleObject *) op->ob_item[0];
            STATE.numfree[index]--;
            /* Inlined _PyObject_InitVar() without _PyType_HasFeature() test */
#ifdef Py_TRACE_REFS
            /* maybe_freelist_push() ensures these were already set. */
            // XXX Can we drop these?  See commit 68055ce6fe01 (GvR, Dec 1998).
            Py_SET_SIZE(op, size);
            Py_SET_TYPE(op, &PyTuple_Type);
#endif
            _Py_NewReference((PyObject *)op);
            /* END inlined _PyObject_InitVar() */
            OBJECT_STAT_INC(from_freelist);
            return op;
        }
    }
#endif
    return NULL;
}



static void
maybe_freelist_clear(PyInterpreterState *interp, int fini)
{
#if PyTuple_NFREELISTS > 0
    for (Py_ssize_t i = 0; i < PyTuple_NFREELISTS; i++) {
        PyTupleObject *p = STATE.free_list[i];
        STATE.free_list[i] = NULL;
        STATE.numfree[i] = fini ? -1 : 0;
        while (p) {
            PyTupleObject *q = p;
            p = (PyTupleObject *)(p->ob_item[0]);
            PyObject_GC_Del(q);
        }
    }
#endif
}

/* Print summary info about the state of the optimized allocator */


#undef STATE
#undef FREELIST_FINALIZED
