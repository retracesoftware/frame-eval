/* Float object implementation */

/* XXX There should be overflow checks here, but it's hard to check
   for any kind of float exception without losing portability. */

#include "Python.h"
#include "pycore_dtoa.h"          // _Py_dg_dtoa()
#include "pycore_floatobject.h"   // _PyFloat_FormatAdvancedWriter()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_interp.h"        // _PyInterpreterState.float_state
#include "pycore_long.h"          // _PyLong_GetOne()
#include "pycore_object.h"        // _PyObject_Init()
#include "pycore_pymath.h"        // _PY_SHORT_FLOAT_REPR
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_structseq.h"     // _PyStructSequence_FiniBuiltin()

#include <ctype.h>
#include <float.h>
#include <stdlib.h>               // strtol()

/*[clinic input]
class float "PyObject *" "&PyFloat_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=dd0003f68f144284]*/

#include "clinic/floatobject.c.h"

#ifndef PyFloat_MAXFREELIST
#  define PyFloat_MAXFREELIST   100
#endif


#if PyFloat_MAXFREELIST > 0
static struct _Py_float_state *
get_float_state(void)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    return &interp->float_state;
}
#endif






static PyTypeObject FloatInfoType;

PyDoc_STRVAR(floatinfo__doc__,
"sys.float_info\n\
\n\
A named tuple holding information about the float type. It contains low level\n\
information about the precision and internal representation. Please study\n\
your system's :file:`float.h` for more information.");

;

;









void
_PyFloat_ExactDealloc(PyObject *obj)
{
    assert(PyFloat_CheckExact(obj));
    PyFloatObject *op = (PyFloatObject *)obj;
#if PyFloat_MAXFREELIST > 0
    struct _Py_float_state *state = get_float_state();
#ifdef Py_DEBUG
    // float_dealloc() must not be called after _PyFloat_Fini()
    assert(state->numfree != -1);
#endif
    if (state->numfree >= PyFloat_MAXFREELIST)  {
        PyObject_Free(op);
        return;
    }
    state->numfree++;
    Py_SET_TYPE(op, (PyTypeObject *)state->free_list);
    state->free_list = op;
    OBJECT_STAT_INC(to_freelist);
#else
    PyObject_Free(op);
#endif
}





/* Macro and helper that convert PyObject obj to a C double and store
   the value in dbl.  If conversion to double raises an exception, obj is
   set to NULL, and the function invoking this macro returns NULL.  If
   obj is not of float or int type, Py_NotImplemented is incref'ed,
   stored in obj, and returned from the function invoking this macro.
*/
#define CONVERT_TO_DOUBLE(obj, dbl)                     \
    if (PyFloat_Check(obj))                             \
        dbl = PyFloat_AS_DOUBLE(obj);                   \
    else if (convert_to_double(&(obj), &(dbl)) < 0)     \
        return obj;

/* Methods */





/* Comparison is pretty much a nightmare.  When comparing float to float,
 * we do it as straightforwardly (and long-windedly) as conceivable, so
 * that, e.g., Python x == y delivers the same result as the platform
 * C x == y when x and/or y is a NaN.
 * When mixing float with an integer type, there's no good *uniform* approach.
 * Converting the double to an integer obviously doesn't work, since we
 * may lose info from fractional bits.  Converting the integer to a double
 * also has two failure modes:  (1) an int may trigger overflow (too
 * large to fit in the dynamic range of a C double); (2) even a C long may have
 * more bits than fit in a C double (e.g., on a 64-bit box long may have
 * 63 bits of precision, but a C double probably has only 53), and then
 * we can falsely claim equality when low-order integer bits are lost by
 * coercion to double.  So this part is painful too.
 */





















/* determine whether x is an odd integer or not;  assumes that
   x is not an infinity or nan. */
#define DOUBLE_IS_ODD_INTEGER(x) (fmod(fabs(x), 2.0) == 1.0)



#undef DOUBLE_IS_ODD_INTEGER







/*[clinic input]
float.is_integer

Return True if the float is an integer.
[clinic start generated code]*/



/*[clinic input]
float.__trunc__

Return the Integral closest to x between 0 and x.
[clinic start generated code]*/



/*[clinic input]
float.__floor__

Return the floor as an Integral.
[clinic start generated code]*/



/*[clinic input]
float.__ceil__

Return the ceiling as an Integral.
[clinic start generated code]*/



/* double_round: rounds a finite double to the closest multiple of
   10**-ndigits; here ndigits is within reasonable bounds (typically, -308 <=
   ndigits <= 323).  Returns a Python float, or sets a Python error and
   returns NULL on failure (OverflowError and memory errors are possible). */

#if _PY_SHORT_FLOAT_REPR == 1
/* version of double_round that uses the correctly-rounded string<->double
   conversions from Python/dtoa.c */



#else  // _PY_SHORT_FLOAT_REPR == 0

/* fallback version, to be used when correctly rounded binary<->decimal
   conversions aren't available */

static PyObject *
double_round(double x, int ndigits) {
    double pow1, pow2, y, z;
    if (ndigits >= 0) {
        if (ndigits > 22) {
            /* pow1 and pow2 are each safe from overflow, but
               pow1*pow2 ~= pow(10.0, ndigits) might overflow */
            pow1 = pow(10.0, (double)(ndigits-22));
            pow2 = 1e22;
        }
        else {
            pow1 = pow(10.0, (double)ndigits);
            pow2 = 1.0;
        }
        y = (x*pow1)*pow2;
        /* if y overflows, then rounded value is exactly x */
        if (!Py_IS_FINITE(y))
            return PyFloat_FromDouble(x);
    }
    else {
        pow1 = pow(10.0, (double)-ndigits);
        pow2 = 1.0; /* unused; silences a gcc compiler warning */
        y = x / pow1;
    }

    z = round(y);
    if (fabs(y-z) == 0.5)
        /* halfway between two integers; use round-half-even */
        z = 2.0*round(y/2.0);

    if (ndigits >= 0)
        z = (z / pow2) / pow1;
    else
        z *= pow1;

    /* if computation resulted in overflow, raise OverflowError */
    if (!Py_IS_FINITE(z)) {
        PyErr_SetString(PyExc_OverflowError,
                        "overflow occurred during round");
        return NULL;
    }

    return PyFloat_FromDouble(z);
}

#endif  // _PY_SHORT_FLOAT_REPR == 0

/* round a Python float v to the closest multiple of 10**-ndigits */

/*[clinic input]
float.__round__

    ndigits as o_ndigits: object = None
    /

Return the Integral closest to x, rounding half toward even.

When an argument is passed, work like built-in round(x, ndigits).
[clinic start generated code]*/





/*[clinic input]
float.conjugate

Return self, the complex conjugate of any float.
[clinic start generated code]*/



/* turn ASCII hex characters into integer values and vice versa */





/* convert a float to a hexadecimal string */

/* TOHEX_NBITS is DBL_MANT_DIG rounded up to the next integer
   of the form 4k+1. */
#define TOHEX_NBITS DBL_MANT_DIG + 3 - (DBL_MANT_DIG+2)%4

/*[clinic input]
float.hex

Return a hexadecimal representation of a floating-point number.

>>> (-0.1).hex()
'-0x1.999999999999ap-4'
>>> 3.14159.hex()
'0x1.921f9f01b866ep+1'
[clinic start generated code]*/



/* Convert a hexadecimal string to a float. */

/*[clinic input]
@classmethod
float.fromhex

    string: object
    /

Create a floating-point number from a hexadecimal string.

>>> float.fromhex('0x1.ffffp10')
2047.984375
>>> float.fromhex('-0x1p-1074')
-5e-324
[clinic start generated code]*/



/*[clinic input]
float.as_integer_ratio

Return a pair of integers, whose ratio is exactly equal to the original float.

The ratio is in lowest terms and has a positive denominator.  Raise
OverflowError on infinities and a ValueError on NaNs.

>>> (10.0).as_integer_ratio()
(10, 1)
>>> (0.0).as_integer_ratio()
(0, 1)
>>> (-.25).as_integer_ratio()
(-1, 4)
[clinic start generated code]*/



static PyObject *
float_subtype_new(PyTypeObject *type, PyObject *x);

/*[clinic input]
@classmethod
float.__new__ as float_new
    x: object(c_default="NULL") = 0
    /

Convert a string or number to a floating-point number, if possible.
[clinic start generated code]*/



/* Wimpy, slow approach to tp_new calls for subtypes of float:
   first create a regular float from whatever arguments we got,
   then allocate a subtype instance and initialize its ob_fval
   from the regular float.  The regular float is then thrown away.
*/





/*[clinic input]
float.__getnewargs__
[clinic start generated code]*/



/* this is for the benefit of the pack/unpack routines below */
typedef enum _py_float_format_type float_format_type;
#define unknown_format _py_float_format_unknown
#define ieee_big_endian_format _py_float_format_ieee_big_endian
#define ieee_little_endian_format _py_float_format_ieee_little_endian

#define float_format (_PyRuntime.float_state.float_format)
#define double_format (_PyRuntime.float_state.double_format)


/*[clinic input]
@classmethod
float.__getformat__

    typestr: str
        Must be 'double' or 'float'.
    /

You probably don't want to use this function.

It exists mainly to be used in Python's test suite.

This function returns whichever of 'unknown', 'IEEE, big-endian' or 'IEEE,
little-endian' best describes the format of floating-point numbers used by the
C type named by typestr.
[clinic start generated code]*/








/*[clinic input]
float.__format__

  format_spec: unicode
  /

Formats the float according to format_spec.
[clinic start generated code]*/



;

;


;

;







void
_PyFloat_ClearFreeList(PyInterpreterState *interp)
{
#if PyFloat_MAXFREELIST > 0
    struct _Py_float_state *state = &interp->float_state;
    PyFloatObject *f = state->free_list;
    while (f != NULL) {
        PyFloatObject *next = (PyFloatObject*) Py_TYPE(f);
        PyObject_Free(f);
        f = next;
    }
    state->free_list = NULL;
    state->numfree = 0;
#endif
}





/* Print summary info about the state of the optimized allocator */



/*----------------------------------------------------------------------------
 * PyFloat_{Pack,Unpack}{2,4,8}.  See floatobject.h.
 * To match the NPY_HALF_ROUND_TIES_TO_EVEN behavior in:
 * https://github.com/numpy/numpy/blob/master/numpy/core/src/npymath/halffloat.c
 * We use:
 *       bits = (unsigned short)f;    Note the truncation
 *       if ((f - bits > 0.5) || (f - bits == 0.5 && bits % 2)) {
 *           bits++;
 *       }
 */












