/* Abstract Object Interface (many thanks to Jim Fulton) */

#include "Python.h"
#include "pycore_abstract.h"      // _PyIndex_Check()
#include "pycore_call.h"          // _PyObject_CallNoArgs()
#include "pycore_ceval.h"         // _Py_EnterRecursiveCallTstate()
#include "pycore_object.h"        // _Py_CheckSlotResult()
#include "pycore_long.h"          // _Py_IsNegative
#include "pycore_pyerrors.h"      // _PyErr_Occurred()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_unionobject.h"   // _PyUnion_Check()
#include <ctype.h>
#include <stddef.h>               // offsetof()



/* Shorthands to return certain errors */





/* Operations on any object */





#undef PyObject_Length

#define PyObject_Length PyObject_Size



/* The length hint function returns a non-negative value from o.__len__()
   or o.__length_hint__(). If those methods aren't found the defaultvalue is
   returned.  If one of the calls fails with an exception other than TypeError
   this function returns -1.
*/












/* Return 1 if the getbuffer function is available, otherwise return 0. */



/* We release the buffer right after use of this function which could
   cause issues later on.  Don't use these functions in new code.
 */










/* Buffer C-API for Python 3.0 */






























/* Operations on numbers */



/* Binary operators */

#define NB_SLOT(x) offsetof(PyNumberMethods, x)
#define NB_BINOP(nb_methods, slot) \
        (*(binaryfunc*)(& ((char*)nb_methods)[slot]))
#define NB_TERNOP(nb_methods, slot) \
        (*(ternaryfunc*)(& ((char*)nb_methods)[slot]))

/*
  Calling scheme used for binary operations:

  Order operations are tried until either a valid result or error:
    w.op(v,w)[*], v.op(v,w), w.op(v,w)

  [*] only when Py_TYPE(v) != Py_TYPE(w) && Py_TYPE(w) is a subclass of
      Py_TYPE(v)
 */



#ifdef NDEBUG
#  define BINARY_OP1(v, w, op_slot, op_name) binary_op1(v, w, op_slot)
#else
#  define BINARY_OP1(v, w, op_slot, op_name) binary_op1(v, w, op_slot, op_name)
#endif






/*
  Calling scheme used for ternary operations:

  Order operations are tried until either a valid result or error:
    v.op(v,w,z), w.op(v,w,z), z.op(v,w,z)
 */



#define BINARY_FUNC(func, op, op_name) \
    PyObject * \
    func(PyObject *v, PyObject *w) { \
        return binary_op(v, w, NB_SLOT(op), op_name); \
    }

























PyObject *
_PyNumber_PowerNoMod(PyObject *lhs, PyObject *rhs)
{
    return PyNumber_Power(lhs, rhs, Py_None);
}

/* Binary in-place operators */

/* The in-place operators are defined to fall back to the 'normal',
   non in-place operations, if the in-place methods are not in place.

   - If the left hand object has the appropriate struct members, and
     they are filled, call the appropriate function and return the
     result.  No coercion is done on the arguments; the left-hand object
     is the one the operation is performed on, and it's up to the
     function to deal with the right-hand object.

   - Otherwise, in-place modification is not supported. Handle it exactly as
     a non in-place operation of the same kind.

   */



#ifdef NDEBUG
#  define BINARY_IOP1(v, w, iop_slot, op_slot, op_name) binary_iop1(v, w, iop_slot, op_slot)
#else
#  define BINARY_IOP1(v, w, iop_slot, op_slot, op_name) binary_iop1(v, w, iop_slot, op_slot, op_name)
#endif





#define INPLACE_BINOP(func, iop, op, op_name) \
    PyObject * \
    func(PyObject *v, PyObject *w) { \
        return binary_iop(v, w, NB_SLOT(iop), NB_SLOT(op), op_name); \
    }


















PyObject *
_PyNumber_InPlacePowerNoMod(PyObject *lhs, PyObject *rhs)
{
    return PyNumber_InPlacePower(lhs, rhs, Py_None);
}


/* Unary operators and functions */













/* Return a Python int from the object item.
   Can return an instance of int subclass.
   Raise TypeError if the result is not an int
   or if the object cannot be interpreted as an index.
*/


/* Return an exact Python int from the object item.
   Raise TypeError if the result is not an int
   or if the object cannot be interpreted as an index.
*/


/* Return an error on Overflow only if err is not NULL*/












/* Operations on sequences */





#undef PySequence_Length

#define PySequence_Length PySequence_Size



























/* Iterate over seq.  Result depends on the operation:
   PY_ITERSEARCH_COUNT:  -1 if error, else # of times obj appears in seq.
   PY_ITERSEARCH_INDEX:  0-based index of first occurrence of obj in seq;
    set ValueError and return -1 if none found; also return -1 on error.
   Py_ITERSEARCH_CONTAINS:  return 1 if obj in seq, else 0; -1 on error.
*/


/* Return # of times o appears in s. */


/* Return -1 if error; 1 if ob in seq; 0 if ob not in seq.
 * Use sq_contains if possible, else defer to _PySequence_IterSearch().
 */


/* Backwards compatibility */
#undef PySequence_In




/* Operations on mappings */





#undef PyMapping_Length

#define PyMapping_Length PyMapping_Size









/* This function is quite similar to PySequence_Fast(), but specialized to be
   a helper for PyMapping_Keys(), PyMapping_Items() and PyMapping_Values().
 */








/* isinstance(), issubclass() */

/* abstract_get_bases() has logically 4 return states:
 *
 * 1. getattr(cls, '__bases__') could raise an AttributeError
 * 2. getattr(cls, '__bases__') could raise some other exception
 * 3. getattr(cls, '__bases__') could return a tuple
 * 4. getattr(cls, '__bases__') could return something other than a tuple
 *
 * Only state #3 is a non-error state and only it returns a non-NULL object
 * (it returns the retrieved tuple).
 *
 * Any raised AttributeErrors are masked by clearing the exception and
 * returning NULL.  If an object other than a tuple comes out of __bases__,
 * then again, the return value is NULL.  So yes, these two situations
 * produce exactly the same results: NULL is returned and no error is set.
 *
 * If some exception other than AttributeError is raised, then NULL is also
 * returned, but the exception is not cleared.  That's because we want the
 * exception to be propagated along.
 *
 * Callers are expected to test for PyErr_Occurred() when the return value
 * is NULL to decide whether a valid exception should be propagated or not.
 * When there's no exception to propagate, it's customary for the caller to
 * set a TypeError.
 */




































/* Return next item.
 * If an error occurs, return NULL.  PyErr_Occurred() will be true.
 * If the iteration terminates normally, return NULL and clear the
 * PyExc_StopIteration exception (if it was set).  PyErr_Occurred()
 * will be false.
 * Else return the next object.  PyErr_Occurred() will be false.
 */




/*
 * Flatten a sequence of bytes() objects into a C array of
 * NULL terminated string pointers with a NULL char* terminating the array.
 * (ie: an argv or env list)
 *
 * Memory allocated for the returned list is allocated using PyMem_Malloc()
 * and MUST be freed by _Py_FreeCharPArray().
 */



/* Free's a NULL terminated char** array of C strings. */

