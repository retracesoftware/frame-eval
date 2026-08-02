/*
Written by Jim Hugunin and Chris Chase.

This includes both the singular ellipsis object and slice objects.

Guido, feel free to do whatever you want in the way of copyrights
for this file.
*/

/*
Py_Ellipsis encodes the '...' rubber index token. It is similar to
the Py_NoneStruct in that there is no way to create other objects of
this type and there is exactly one in existence.
*/

#include "Python.h"
#include "pycore_abstract.h"      // _PyIndex_Check()
#include "pycore_long.h"          // _PyLong_GetZero()
#include "pycore_object.h"        // _PyObject_GC_TRACK()
#include "structmember.h"         // PyMemberDef









;

;

;


/* Slice object implementation */




/* start, stop, and step are python objects with None indicating no
   index is present.
*/

static PySliceObject *
_PyBuildSlice_Consume2(PyObject *start, PyObject *stop, PyObject *step)
{
    assert(start != NULL && stop != NULL && step != NULL);

    PyInterpreterState *interp = _PyInterpreterState_GET();
    PySliceObject *obj;
    if (interp->slice_cache != NULL) {
        obj = interp->slice_cache;
        interp->slice_cache = NULL;
        _Py_NewReference((PyObject *)obj);
    }
    else {
        obj = PyObject_GC_New(PySliceObject, &PySlice_Type);
        if (obj == NULL) {
            goto error;
        }
    }

    obj->start = start;
    obj->stop = stop;
    obj->step = Py_NewRef(step);

    _PyObject_GC_TRACK(obj);
    return obj;
error:
    Py_DECREF(start);
    Py_DECREF(stop);
    return NULL;
}



PyObject *
_PyBuildSlice_ConsumeRefs(PyObject *start, PyObject *stop)
{
    assert(start != NULL && stop != NULL);
    return (PyObject *)_PyBuildSlice_Consume2(start, stop, Py_None);
}









#undef PySlice_GetIndicesEx





PyDoc_STRVAR(slice_doc,
"slice(stop)\n\
slice(start, stop[, step])\n\
\n\
Create a slice object.  This is used for extended slicing (e.g. a[0:10:2]).");





;

/* Helper function to convert a slice argument to a PyLong, and raise TypeError
   with a suitable message on failure. */



/* Compute slice indices given a slice and length.  Return -1 on failure.  Used
   by slice.indices and rangeobject slicing.  Assumes that `len` is a
   nonnegative instance of PyLong. */



/* Implementation of slice.indices. */



PyDoc_STRVAR(slice_indices_doc,
"S.indices(len) -> (start, stop, stride)\n\
\n\
Assuming a sequence of length len, calculate the start and stop\n\
indices, and the stride length of the extended slice described by\n\
S. Out of bounds indices are clipped in a manner consistent with the\n\
handling of normal slices.");



PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");

;





/* code based on tuplehash() of Objects/tupleobject.c */
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



;
