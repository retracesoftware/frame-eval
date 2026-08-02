/* Long (arbitrary precision) integer object implementation */

/* XXX The functional organization of this file is terrible */

#include "Python.h"
#include "pycore_bitutils.h"      // _Py_popcount32()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_long.h"          // _Py_SmallInts
#include "pycore_object.h"        // _PyObject_Init()
#include "pycore_runtime.h"       // _PY_NSMALLPOSINTS
#include "pycore_structseq.h"     // _PyStructSequence_FiniBuiltin()

#include <ctype.h>
#include <float.h>
#include <stddef.h>
#include <stdlib.h>               // abs()

#include "clinic/longobject.c.h"
/*[clinic input]
class int "PyObject *" "&PyLong_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=ec0275e3422a36e3]*/

#define medium_value(x) ((stwodigits)_PyLong_CompactValue(x))

#define IS_SMALL_INT(ival) (-_PY_NSMALLNEGINTS <= (ival) && (ival) < _PY_NSMALLPOSINTS)
#define IS_SMALL_UINT(ival) ((ival) < _PY_NSMALLPOSINTS)

#define _MAX_STR_DIGITS_ERROR_FMT_TO_INT "Exceeds the limit (%d digits) for integer string conversion: value has %zd digits; use sys.set_int_max_str_digits() to increase the limit"
#define _MAX_STR_DIGITS_ERROR_FMT_TO_STR "Exceeds the limit (%d digits) for integer string conversion; use sys.set_int_max_str_digits() to increase the limit"

/* If defined, use algorithms from the _pylong.py module */
#define WITH_PYLONG_MODULE 1

static inline void
_Py_DECREF_INT(PyLongObject *op)
{
    assert(PyLong_CheckExact(op));
    _Py_DECREF_SPECIALIZED((PyObject *)op, (destructor)PyObject_Free);
}

static inline int
is_medium_int(stwodigits x)
{
    /* Take care that we are comparing unsigned values. */
    twodigits x_plus_mask = ((twodigits)x) + PyLong_MASK;
    return x_plus_mask < ((twodigits)PyLong_MASK) + PyLong_BASE;
}

static PyObject *
get_small_int(sdigit ival)
{
    assert(IS_SMALL_INT(ival));
    return (PyObject *)&_PyLong_SMALL_INTS[_PY_NSMALLNEGINTS + ival];
}

static PyLongObject *
maybe_small_long(PyLongObject *v)
{
    if (v && _PyLong_IsCompact(v)) {
        stwodigits ival = medium_value(v);
        if (IS_SMALL_INT(ival)) {
            _Py_DECREF_INT(v);
            return (PyLongObject *)get_small_int((sdigit)ival);
        }
    }
    return v;
}

/* For int multiplication, use the O(N**2) school algorithm unless
 * both operands contain more than KARATSUBA_CUTOFF digits (this
 * being an internal Python int digit, in base BASE).
 */
#define KARATSUBA_CUTOFF 70
#define KARATSUBA_SQUARE_CUTOFF (2 * KARATSUBA_CUTOFF)

/* For exponentiation, use the binary left-to-right algorithm unless the
 ^ exponent contains more than HUGE_EXP_CUTOFF bits.  In that case, do
 * (no more than) EXP_WINDOW_SIZE bits at a time.  The potential drawback is
 * that a table of 2**(EXP_WINDOW_SIZE - 1) intermediate results is
 * precomputed.
 */
#define EXP_WINDOW_SIZE 5
#define EXP_TABLE_LEN (1 << (EXP_WINDOW_SIZE - 1))
/* Suppose the exponent has bit length e. All ways of doing this
 * need e squarings. The binary method also needs a multiply for
 * each bit set. In a k-ary method with window width w, a multiply
 * for each non-zero window, so at worst (and likely!)
 * ceiling(e/w). The k-ary sliding window method has the same
 * worst case, but the window slides so it can sometimes skip
 * over an all-zero window that the fixed-window method can't
 * exploit. In addition, the windowing methods need multiplies
 * to precompute a table of small powers.
 *
 * For the sliding window method with width 5, 16 precomputation
 * multiplies are needed. Assuming about half the exponent bits
 * are set, then, the binary method needs about e/2 extra mults
 * and the window method about 16 + e/5.
 *
 * The latter is smaller for e > 53 1/3. We don't have direct
 * access to the bit length, though, so call it 60, which is a
 * multiple of a long digit's max bit length (15 or 30 so far).
 */
#define HUGE_EXP_CUTOFF 60

#define SIGCHECK(PyTryBlock)                    \
    do {                                        \
        if (PyErr_CheckSignals()) PyTryBlock    \
    } while(0)

/* Normalize (remove leading zeros from) an int object.
   Doesn't attempt to free the storage--in most cases, due to the nature
   of the algorithms used, this could save at most be one word anyway. */

static PyLongObject *
long_normalize(PyLongObject *v)
{
    Py_ssize_t j = _PyLong_DigitCount(v);
    Py_ssize_t i = j;

    while (i > 0 && v->long_value.ob_digit[i-1] == 0)
        --i;
    if (i != j) {
        if (i == 0) {
            _PyLong_SetSignAndDigitCount(v, 0, 0);
        }
        else {
            _PyLong_SetDigitCount(v, i);
        }
    }
    return v;
}

/* Allocate a new int object with size digits.
   Return NULL and set exception if we run out of memory. */

#define MAX_LONG_DIGITS \
    ((PY_SSIZE_T_MAX - offsetof(PyLongObject, long_value.ob_digit))/sizeof(digit))







static PyObject *
_PyLong_FromMedium(sdigit x)
{
    assert(!IS_SMALL_INT(x));
    assert(is_medium_int(x));
    /* We could use a freelist here */
    PyLongObject *v = PyObject_Malloc(sizeof(PyLongObject));
    if (v == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    digit abs_x = x < 0 ? -x : x;
    _PyLong_SetSignAndDigitCount(v, x<0?-1:1, 1);
    _PyObject_Init((PyObject*)v, &PyLong_Type);
    v->long_value.ob_digit[0] = abs_x;
    return (PyObject*)v;
}

static PyObject *
_PyLong_FromLarge(stwodigits ival)
{
    twodigits abs_ival;
    int sign;
    assert(!is_medium_int(ival));

    if (ival < 0) {
        /* negate: can't write this as abs_ival = -ival since that
           invokes undefined behaviour when ival is LONG_MIN */
        abs_ival = 0U-(twodigits)ival;
        sign = -1;
    }
    else {
        abs_ival = (twodigits)ival;
        sign = 1;
    }
    /* Must be at least two digits */
    assert(abs_ival >> PyLong_SHIFT != 0);
    twodigits t = abs_ival >> (PyLong_SHIFT * 2);
    Py_ssize_t ndigits = 2;
    while (t) {
        ++ndigits;
        t >>= PyLong_SHIFT;
    }
    PyLongObject *v = _PyLong_New(ndigits);
    if (v != NULL) {
        digit *p = v->long_value.ob_digit;
        _PyLong_SetSignAndDigitCount(v, sign, ndigits);
        t = abs_ival;
        while (t) {
            *p++ = Py_SAFE_DOWNCAST(
                t & PyLong_MASK, twodigits, digit);
            t >>= PyLong_SHIFT;
        }
    }
    return (PyObject *)v;
}

/* Create a new int object from a C word-sized int */
static inline PyObject *
_PyLong_FromSTwoDigits(stwodigits x)
{
    if (IS_SMALL_INT(x)) {
        return get_small_int((sdigit)x);
    }
    assert(x != 0);
    if (is_medium_int(x)) {
        return _PyLong_FromMedium((sdigit)x);
    }
    return _PyLong_FromLarge(x);
}

/* If a freshly-allocated int is already shared, it must
   be a small integer, so negating it must go to PyLong_FromLong */
Py_LOCAL_INLINE(void)
_PyLong_Negate(PyLongObject **x_p)
{
    PyLongObject *x;

    x = (PyLongObject *)*x_p;
    if (Py_REFCNT(x) == 1) {
         _PyLong_FlipSign(x);
        return;
    }

    *x_p = (PyLongObject *)_PyLong_FromSTwoDigits(-medium_value(x));
    Py_DECREF(x);
}

/* Create a new int object from a C long int */



#define PYLONG_FROM_UINT(INT_TYPE, ival) \
    do { \
        if (IS_SMALL_UINT(ival)) { \
            return get_small_int((sdigit)(ival)); \
        } \
        /* Count the number of Python digits. */ \
        Py_ssize_t ndigits = 0; \
        INT_TYPE t = (ival); \
        while (t) { \
            ++ndigits; \
            t >>= PyLong_SHIFT; \
        } \
        PyLongObject *v = _PyLong_New(ndigits); \
        if (v == NULL) { \
            return NULL; \
        } \
        digit *p = v->long_value.ob_digit; \
        while ((ival)) { \
            *p++ = (digit)((ival) & PyLong_MASK); \
            (ival) >>= PyLong_SHIFT; \
        } \
        return (PyObject *)v; \
    } while(0)

/* Create a new int object from a C unsigned long int */



/* Create a new int object from a C unsigned long long int. */



/* Create a new int object from a C size_t. */



/* Create a new int object from a C double */



/* Checking for overflow in PyLong_AsLong is a PITA since C doesn't define
 * anything about what happens when a signed integer operation overflows,
 * and some compilers think they're doing you a favor by being "clever"
 * then.  The bit pattern for the largest positive signed long is
 * (unsigned long)LONG_MAX, and for the smallest negative signed long
 * it is abs(LONG_MIN), which we could write -(unsigned long)LONG_MIN.
 * However, some other compilers warn about applying unary minus to an
 * unsigned operand.  Hence the weird "0-".
 */
#define PY_ABS_LONG_MIN         (0-(unsigned long)LONG_MIN)
#define PY_ABS_SSIZE_T_MIN      (0-(size_t)PY_SSIZE_T_MIN)

/* Get a C long int from an int object or any object that has an __index__
   method.

   On overflow, return -1 and set *overflow to 1 or -1 depending on the sign of
   the result.  Otherwise *overflow is 0.

   For other errors (e.g., TypeError), return -1 and set an error condition.
   In this case *overflow will be 0.
*/



/* Get a C long int from an int object or any object that has an __index__
   method.  Return -1 and set an error if overflow occurs. */



/* Get a C int from an int object or any object that has an __index__
   method.  Return -1 and set an error if overflow occurs. */



/* Get a Py_ssize_t from an int object.
   Returns -1 and sets an error condition if overflow occurs. */



/* Get a C unsigned long int from an int object.
   Returns -1 and sets an error condition if overflow occurs. */



/* Get a C size_t from an int object. Returns (size_t)-1 and sets
   an error condition if overflow occurs. */



/* Get a C unsigned long int from an int object, ignoring the high bits.
   Returns -1 and sets an error condition if an error occurs. */















/* Create a new int object from a C pointer */



/* Get a C pointer from an int object. */



/* Initial long long support by Chris Herborth (chrish@qnx.com), later
 * rewritten to use the newer PyLong_{As,From}ByteArray API.
 */

#define PY_ABS_LLONG_MIN (0-(unsigned long long)LLONG_MIN)

/* Create a new int object from a C long long int. */



/* Create a new int object from a C Py_ssize_t. */



/* Get a C long long int from an int object or any object that has an
   __index__ method.  Return -1 and set an error if overflow occurs. */



/* Get a C unsigned long long int from an int object.
   Return -1 and set an error if overflow occurs. */



/* Get a C unsigned long int from an int object, ignoring the high bits.
   Returns -1 and sets an error condition if an error occurs. */





/* Get a C long long int from an int object or any object that has an
   __index__ method.

   On overflow, return -1 and set *overflow to 1 or -1 depending on the sign of
   the result.  Otherwise *overflow is 0.

   For other errors (e.g., TypeError), return -1 and set an error condition.
   In this case *overflow will be 0.
*/














#define CHECK_BINOP(v,w)                                \
    do {                                                \
        if (!PyLong_Check(v) || !PyLong_Check(w))       \
            Py_RETURN_NOTIMPLEMENTED;                   \
    } while(0)

/* x[0:m] and y[0:n] are digit vectors, LSD first, m >= n required.  x[0:n]
 * is modified in place, by adding y to it.  Carries are propagated as far as
 * x[m-1], and the remaining carry (0 or 1) is returned.
 */
static digit
v_iadd(digit *x, Py_ssize_t m, digit *y, Py_ssize_t n)
{
    Py_ssize_t i;
    digit carry = 0;

    assert(m >= n);
    for (i = 0; i < n; ++i) {
        carry += x[i] + y[i];
        x[i] = carry & PyLong_MASK;
        carry >>= PyLong_SHIFT;
        assert((carry & 1) == carry);
    }
    for (; carry && i < m; ++i) {
        carry += x[i];
        x[i] = carry & PyLong_MASK;
        carry >>= PyLong_SHIFT;
        assert((carry & 1) == carry);
    }
    return carry;
}

/* x[0:m] and y[0:n] are digit vectors, LSD first, m >= n required.  x[0:n]
 * is modified in place, by subtracting y from it.  Borrows are propagated as
 * far as x[m-1], and the remaining borrow (0 or 1) is returned.
 */
static digit
v_isub(digit *x, Py_ssize_t m, digit *y, Py_ssize_t n)
{
    Py_ssize_t i;
    digit borrow = 0;

    assert(m >= n);
    for (i = 0; i < n; ++i) {
        borrow = x[i] - y[i] - borrow;
        x[i] = borrow & PyLong_MASK;
        borrow >>= PyLong_SHIFT;
        borrow &= 1;            /* keep only 1 sign bit */
    }
    for (; borrow && i < m; ++i) {
        borrow = x[i] - borrow;
        x[i] = borrow & PyLong_MASK;
        borrow >>= PyLong_SHIFT;
        borrow &= 1;
    }
    return borrow;
}

/* Shift digit vector a[0:m] d bits left, with 0 <= d < PyLong_SHIFT.  Put
 * result in z[0:m], and return the d bits shifted out of the top.
 */


/* Shift digit vector a[0:m] d bits right, with 0 <= d < PyLong_SHIFT.  Put
 * result in z[0:m], and return the d bits shifted out of the bottom.
 */


/* Divide long pin, w/ size digits, by non-zero digit n, storing quotient
   in pout, and returning the remainder.  pin and pout point at the LSD.
   It's OK for pin == pout on entry, which saves oodles of mallocs/frees in
   _PyLong_Format, but that should be done with great care since ints are
   immutable.

   This version of the code can be 20% faster than the pre-2022 version
   on todays compilers on architectures like amd64.  It evolved from Mark
   Dickinson observing that a 128:64 divide instruction was always being
   generated by the compiler despite us working with 30-bit digit values.
   See the thread for full context:

     https://mail.python.org/archives/list/python-dev@python.org/thread/ZICIMX5VFCX4IOFH5NUPVHCUJCQ4Q7QM/#NEUNFZU3TQU4CPTYZNF3WCN7DOJBBTK5

   If you ever want to change this code, pay attention to performance using
   different compilers, optimization levels, and cpu architectures. Beware of
   PGO/FDO builds doing value specialization such as a fast path for //10. :)

   Verify that 17 isn't specialized and this works as a quick test:
     python -m timeit -s 'x = 10**1000; r=x//10; assert r == 10**999, r' 'x//17'
*/



/* Divide an integer by a digit, returning both the quotient
   (as function result) and the remainder (through *prem).
   The sign of a is ignored; n should not be zero. */



/* Remainder of long pin, w/ size digits, by non-zero digit n,
   returning the remainder. pin points at the LSD. */



/* Get the remainder of an integer divided by a digit, returning
   the remainder as the result of the function. The sign of a is
   ignored; n should not be zero. */



#ifdef WITH_PYLONG_MODULE
/* asymptotically faster long_to_decimal_string, using _pylong.py */

#endif /* WITH_PYLONG_MODULE */

/* Convert an integer to a base 10 string.  Returns a new non-shared
   string.  (Return value is non-shared so that callers can modify the
   returned value if necessary.) */





/* Convert an int object to a string, using a given conversion base,
   which should be one of 2, 8 or 16.  Return a string object.
   If base is 2, 8 or 16, add the proper prefix '0b', '0o' or '0x'
   if alternate is nonzero. */









/* Table of digit values for 8-bit string -> integer conversion.
 * '0' maps to 0, ..., '9' maps to 9.
 * 'a' and 'A' map to 10, ..., 'z' and 'Z' map to 35.
 * All other indices map to 37.
 * Note that when converting a base B string, a char c is a legitimate
 * base B digit iff _PyLong_DigitValue[Py_CHARPyLong_MASK(c)] < B.
 */
;

/* `start` and `end` point to the start and end of a string of base `base`
 * digits.  base is a power of 2 (2, 4, 8, 16, or 32). An unnormalized int is
 * returned in *res. The string should be already validated by the caller and
 * consists only of valid digit characters and underscores. `digits` gives the
 * number of digit characters.
 *
 * The point to this routine is that it takes time linear in the
 * number of string characters.
 *
 * Return values:
 *   -1 on syntax error (exception needs to be set, *res is untouched)
 *   0 else (exception may be set, in that case *res is set to NULL)
 */


static PyObject *long_neg(PyLongObject *v);

#ifdef WITH_PYLONG_MODULE
/* asymptotically faster str-to-long conversion for base 10, using _pylong.py */

#endif /* WITH_PYLONG_MODULE */

/***
long_from_non_binary_base: parameters and return values are the same as
long_from_binary_base.

Binary bases can be converted in time linear in the number of digits, because
Python's representation base is binary.  Other bases (including decimal!) use
the simple quadratic-time algorithm below, complicated by some speed tricks.

First some math:  the largest integer that can be expressed in N base-B digits
is B**N-1.  Consequently, if we have an N-digit input in base B, the worst-
case number of Python digits needed to hold it is the smallest integer n s.t.

    BASE**n-1 >= B**N-1  [or, adding 1 to both sides]
    BASE**n >= B**N      [taking logs to base BASE]
    n >= log(B**N)/log(BASE) = N * log(B)/log(BASE)

The static array log_base_BASE[base] == log(base)/log(BASE) so we can compute
this quickly.  A Python int with that much space is reserved near the start,
and the result is computed into it.

The input string is actually treated as being in base base**i (i.e., i digits
are processed at a time), where two more static arrays hold:

    convwidth_base[base] = the largest integer i such that base**i <= BASE
    convmultmax_base[base] = base ** convwidth_base[base]

The first of these is the largest i such that i consecutive input digits
must fit in a single Python digit.  The second is effectively the input
base we're really using.

Viewing the input as a sequence <c0, c1, ..., c_n-1> of digits in base
convmultmax_base[base], the result is "simply"

   (((c0*B + c1)*B + c2)*B + c3)*B + ... ))) + c_n-1

where B = convmultmax_base[base].

Error analysis:  as above, the number of Python digits `n` needed is worst-
case

    n >= N * log(B)/log(BASE)

where `N` is the number of input digits in base `B`.  This is computed via

    size_z = (Py_ssize_t)((scan - str) * log_base_BASE[base]) + 1;

below.  Two numeric concerns are how much space this can waste, and whether
the computed result can be too small.  To be concrete, assume BASE = 2**15,
which is the default (and it's unlikely anyone changes that).

Waste isn't a problem:  provided the first input digit isn't 0, the difference
between the worst-case input with N digits and the smallest input with N
digits is about a factor of B, but B is small compared to BASE so at most
one allocated Python digit can remain unused on that count.  If
N*log(B)/log(BASE) is mathematically an exact integer, then truncating that
and adding 1 returns a result 1 larger than necessary.  However, that can't
happen:  whenever B is a power of 2, long_from_binary_base() is called
instead, and it's impossible for B**i to be an integer power of 2**15 when
B is not a power of 2 (i.e., it's impossible for N*log(B)/log(BASE) to be
an exact integer when B is not a power of 2, since B**i has a prime factor
other than 2 in that case, but (2**15)**j's only prime factor is 2).

The computed result can be too small if the true value of N*log(B)/log(BASE)
is a little bit larger than an exact integer, but due to roundoff errors (in
computing log(B), log(BASE), their quotient, and/or multiplying that by N)
yields a numeric result a little less than that integer.  Unfortunately, "how
close can a transcendental function get to an integer over some range?"
questions are generally theoretically intractable.  Computer analysis via
continued fractions is practical:  expand log(B)/log(BASE) via continued
fractions, giving a sequence i/j of "the best" rational approximations.  Then
j*log(B)/log(BASE) is approximately equal to (the integer) i.  This shows that
we can get very close to being in trouble, but very rarely.  For example,
76573 is a denominator in one of the continued-fraction approximations to
log(10)/log(2**15), and indeed:

    >>> log(10)/log(2**15)*76573
    16958.000000654003

is very close to an integer.  If we were working with IEEE single-precision,
rounding errors could kill us.  Finding worst cases in IEEE double-precision
requires better-than-double-precision log() functions, and Tim didn't bother.
Instead the code checks to see whether the allocated space is enough as each
new Python digit is added, and copies the whole thing to a larger int if not.
This should happen extremely rarely, and in fact I don't have a test case
that triggers it(!).  Instead the code was tested by artificially allocating
just 1 digit at the start, so that the copying code was exercised for every
digit beyond the first.
***/


/* *str points to the first digit in a string of base `base` digits. base is an
 * integer from 2 to 36 inclusive. Here we don't need to worry about prefixes
 * like 0x or leading +- signs. The string should be null terminated consisting
 * of ASCII digits and separating underscores possibly with trailing whitespace
 * but we have to validate all of those points here.
 *
 * If base is a power of 2 then the complexity is linear in the number of
 * characters in the string. Otherwise a quadratic algorithm is used for
 * non-binary bases.
 *
 * Return values:
 *
 *   - Returns -1 on syntax error (exception needs to be set, *res is untouched)
 *   - Returns 0 and sets *res to NULL for MemoryError, OverflowError, or
 *     _pylong.int_from_string() errors.
 *   - Returns 0 and sets *res to an unsigned, unnormalized PyLong (success!).
 *
 * Afterwards *str is set to point to the first non-digit (which may be *str!).
 */


/* Parses an int from a bytestring. Leading and trailing whitespace will be
 * ignored.
 *
 * If successful, a PyLong object will be returned and 'pend' will be pointing
 * to the first unused byte unless it's NULL.
 *
 * If unsuccessful, NULL will be returned.
 */


/* Since PyLong_FromString doesn't have a length parameter,
 * check here for possible NULs in the string.
 *
 * Reports an invalid literal as a bytes object.
 */




/* forward */
static PyLongObject *x_divrem
    (PyLongObject *, PyLongObject *, PyLongObject **);
static PyObject *long_long(PyObject *v);

/* Int division with remainder, top-level routine */



/* Int remainder, top-level routine */



/* Unsigned int division with remainder -- the algorithm.  The arguments v1
   and w1 should satisfy 2 <= _PyLong_DigitCount(w1) <= _PyLong_DigitCount(v1). */



/* For a nonzero PyLong a, express a in the form x * 2**e, with 0.5 <=
   abs(x) < 1.0 and e >= 0; return x and put e in *e.  Here x is
   rounded to DBL_MANT_DIG significant bits using round-half-to-even.
   If a == 0, return 0.0 and set *e = 0.  If the resulting exponent
   e is larger than PY_SSIZE_T_MAX, raise OverflowError and return
   -1.0. */

/* attempt to define 2.0**DBL_MANT_DIG as a compile-time constant */
#if DBL_MANT_DIG == 53
#define EXP2_DBL_MANT_DIG 9007199254740992.0
#else
#define EXP2_DBL_MANT_DIG (ldexp(1.0, DBL_MANT_DIG))
#endif



/* Get a C double from an int object.  Rounds to the nearest double,
   using the round-half-to-even rule in the case of a tie. */



/* Methods */

/* if a < b, return a negative number
   if a == b, return 0
   if a > b, return a positive number */










/* Add the absolute values of two integers. */

static PyLongObject *
x_add(PyLongObject *a, PyLongObject *b)
{
    Py_ssize_t size_a = _PyLong_DigitCount(a), size_b = _PyLong_DigitCount(b);
    PyLongObject *z;
    Py_ssize_t i;
    digit carry = 0;

    /* Ensure a is the larger of the two: */
    if (size_a < size_b) {
        { PyLongObject *temp = a; a = b; b = temp; }
        { Py_ssize_t size_temp = size_a;
            size_a = size_b;
            size_b = size_temp; }
    }
    z = _PyLong_New(size_a+1);
    if (z == NULL)
        return NULL;
    for (i = 0; i < size_b; ++i) {
        carry += a->long_value.ob_digit[i] + b->long_value.ob_digit[i];
        z->long_value.ob_digit[i] = carry & PyLong_MASK;
        carry >>= PyLong_SHIFT;
    }
    for (; i < size_a; ++i) {
        carry += a->long_value.ob_digit[i];
        z->long_value.ob_digit[i] = carry & PyLong_MASK;
        carry >>= PyLong_SHIFT;
    }
    z->long_value.ob_digit[i] = carry;
    return long_normalize(z);
}

/* Subtract the absolute values of two integers. */

static PyLongObject *
x_sub(PyLongObject *a, PyLongObject *b)
{
    Py_ssize_t size_a = _PyLong_DigitCount(a), size_b = _PyLong_DigitCount(b);
    PyLongObject *z;
    Py_ssize_t i;
    int sign = 1;
    digit borrow = 0;

    /* Ensure a is the larger of the two: */
    if (size_a < size_b) {
        sign = -1;
        { PyLongObject *temp = a; a = b; b = temp; }
        { Py_ssize_t size_temp = size_a;
            size_a = size_b;
            size_b = size_temp; }
    }
    else if (size_a == size_b) {
        /* Find highest digit where a and b differ: */
        i = size_a;
        while (--i >= 0 && a->long_value.ob_digit[i] == b->long_value.ob_digit[i])
            ;
        if (i < 0)
            return (PyLongObject *)PyLong_FromLong(0);
        if (a->long_value.ob_digit[i] < b->long_value.ob_digit[i]) {
            sign = -1;
            { PyLongObject *temp = a; a = b; b = temp; }
        }
        size_a = size_b = i+1;
    }
    z = _PyLong_New(size_a);
    if (z == NULL)
        return NULL;
    for (i = 0; i < size_b; ++i) {
        /* The following assumes unsigned arithmetic
           works module 2**N for some N>PyLong_SHIFT. */
        borrow = a->long_value.ob_digit[i] - b->long_value.ob_digit[i] - borrow;
        z->long_value.ob_digit[i] = borrow & PyLong_MASK;
        borrow >>= PyLong_SHIFT;
        borrow &= 1; /* Keep only one sign bit */
    }
    for (; i < size_a; ++i) {
        borrow = a->long_value.ob_digit[i] - borrow;
        z->long_value.ob_digit[i] = borrow & PyLong_MASK;
        borrow >>= PyLong_SHIFT;
        borrow &= 1; /* Keep only one sign bit */
    }
    assert(borrow == 0);
    if (sign < 0) {
        _PyLong_FlipSign(z);
    }
    return maybe_small_long(long_normalize(z));
}

PyObject *
_PyLong_Add(PyLongObject *a, PyLongObject *b)
{
    if (_PyLong_BothAreCompact(a, b)) {
        return _PyLong_FromSTwoDigits(medium_value(a) + medium_value(b));
    }

    PyLongObject *z;
    if (_PyLong_IsNegative(a)) {
        if (_PyLong_IsNegative(b)) {
            z = x_add(a, b);
            if (z != NULL) {
                /* x_add received at least one multiple-digit int,
                   and thus z must be a multiple-digit int.
                   That also means z is not an element of
                   small_ints, so negating it in-place is safe. */
                assert(Py_REFCNT(z) == 1);
                _PyLong_FlipSign(z);
            }
        }
        else
            z = x_sub(b, a);
    }
    else {
        if (_PyLong_IsNegative(b))
            z = x_sub(a, b);
        else
            z = x_add(a, b);
    }
    return (PyObject *)z;
}



PyObject *
_PyLong_Subtract(PyLongObject *a, PyLongObject *b)
{
    PyLongObject *z;

    if (_PyLong_BothAreCompact(a, b)) {
        return _PyLong_FromSTwoDigits(medium_value(a) - medium_value(b));
    }
    if (_PyLong_IsNegative(a)) {
        if (_PyLong_IsNegative(b)) {
            z = x_sub(b, a);
        }
        else {
            z = x_add(a, b);
            if (z != NULL) {
                assert(_PyLong_IsZero(z) || Py_REFCNT(z) == 1);
                _PyLong_FlipSign(z);
            }
        }
    }
    else {
        if (_PyLong_IsNegative(b))
            z = x_add(a, b);
        else
            z = x_sub(a, b);
    }
    return (PyObject *)z;
}



/* Grade school multiplication, ignoring the signs.
 * Returns the absolute value of the product, or NULL if error.
 */
static PyLongObject *
x_mul(PyLongObject *a, PyLongObject *b)
{
    PyLongObject *z;
    Py_ssize_t size_a = _PyLong_DigitCount(a);
    Py_ssize_t size_b = _PyLong_DigitCount(b);
    Py_ssize_t i;

    z = _PyLong_New(size_a + size_b);
    if (z == NULL)
        return NULL;

    memset(z->long_value.ob_digit, 0, _PyLong_DigitCount(z) * sizeof(digit));
    if (a == b) {
        /* Efficient squaring per HAC, Algorithm 14.16:
         * http://www.cacr.math.uwaterloo.ca/hac/about/chap14.pdf
         * Gives slightly less than a 2x speedup when a == b,
         * via exploiting that each entry in the multiplication
         * pyramid appears twice (except for the size_a squares).
         */
        digit *paend = a->long_value.ob_digit + size_a;
        for (i = 0; i < size_a; ++i) {
            twodigits carry;
            twodigits f = a->long_value.ob_digit[i];
            digit *pz = z->long_value.ob_digit + (i << 1);
            digit *pa = a->long_value.ob_digit + i + 1;

            SIGCHECK({
                    Py_DECREF(z);
                    return NULL;
                });

            carry = *pz + f * f;
            *pz++ = (digit)(carry & PyLong_MASK);
            carry >>= PyLong_SHIFT;
            assert(carry <= PyLong_MASK);

            /* Now f is added in twice in each column of the
             * pyramid it appears.  Same as adding f<<1 once.
             */
            f <<= 1;
            while (pa < paend) {
                carry += *pz + *pa++ * f;
                *pz++ = (digit)(carry & PyLong_MASK);
                carry >>= PyLong_SHIFT;
                assert(carry <= (PyLong_MASK << 1));
            }
            if (carry) {
                /* See comment below. pz points at the highest possible
                 * carry position from the last outer loop iteration, so
                 * *pz is at most 1.
                 */
                assert(*pz <= 1);
                carry += *pz;
                *pz = (digit)(carry & PyLong_MASK);
                carry >>= PyLong_SHIFT;
                if (carry) {
                    /* If there's still a carry, it must be into a position
                     * that still holds a 0. Where the base
                     ^ B is 1 << PyLong_SHIFT, the last add was of a carry no
                     * more than 2*B - 2 to a stored digit no more than 1.
                     * So the sum was no more than 2*B - 1, so the current
                     * carry no more than floor((2*B - 1)/B) = 1.
                     */
                    assert(carry == 1);
                    assert(pz[1] == 0);
                    pz[1] = (digit)carry;
                }
            }
        }
    }
    else {      /* a is not the same as b -- gradeschool int mult */
        for (i = 0; i < size_a; ++i) {
            twodigits carry = 0;
            twodigits f = a->long_value.ob_digit[i];
            digit *pz = z->long_value.ob_digit + i;
            digit *pb = b->long_value.ob_digit;
            digit *pbend = b->long_value.ob_digit + size_b;

            SIGCHECK({
                    Py_DECREF(z);
                    return NULL;
                });

            while (pb < pbend) {
                carry += *pz + *pb++ * f;
                *pz++ = (digit)(carry & PyLong_MASK);
                carry >>= PyLong_SHIFT;
                assert(carry <= PyLong_MASK);
            }
            if (carry)
                *pz += (digit)(carry & PyLong_MASK);
            assert((carry >> PyLong_SHIFT) == 0);
        }
    }
    return long_normalize(z);
}

/* A helper for Karatsuba multiplication (k_mul).
   Takes an int "n" and an integer "size" representing the place to
   split, and sets low and high such that abs(n) == (high << size) + low,
   viewing the shift as being by digits.  The sign bit is ignored, and
   the return values are >= 0.
   Returns 0 on success, -1 on failure.
*/
static int
kmul_split(PyLongObject *n,
           Py_ssize_t size,
           PyLongObject **high,
           PyLongObject **low)
{
    PyLongObject *hi, *lo;
    Py_ssize_t size_lo, size_hi;
    const Py_ssize_t size_n = _PyLong_DigitCount(n);

    size_lo = Py_MIN(size_n, size);
    size_hi = size_n - size_lo;

    if ((hi = _PyLong_New(size_hi)) == NULL)
        return -1;
    if ((lo = _PyLong_New(size_lo)) == NULL) {
        Py_DECREF(hi);
        return -1;
    }

    memcpy(lo->long_value.ob_digit, n->long_value.ob_digit, size_lo * sizeof(digit));
    memcpy(hi->long_value.ob_digit, n->long_value.ob_digit + size_lo, size_hi * sizeof(digit));

    *high = long_normalize(hi);
    *low = long_normalize(lo);
    return 0;
}

static PyLongObject *k_lopsided_mul(PyLongObject *a, PyLongObject *b);

/* Karatsuba multiplication.  Ignores the input signs, and returns the
 * absolute value of the product (or NULL if error).
 * See Knuth Vol. 2 Chapter 4.3.3 (Pp. 294-295).
 */
static PyLongObject *
k_mul(PyLongObject *a, PyLongObject *b)
{
    Py_ssize_t asize = _PyLong_DigitCount(a);
    Py_ssize_t bsize = _PyLong_DigitCount(b);
    PyLongObject *ah = NULL;
    PyLongObject *al = NULL;
    PyLongObject *bh = NULL;
    PyLongObject *bl = NULL;
    PyLongObject *ret = NULL;
    PyLongObject *t1, *t2, *t3;
    Py_ssize_t shift;           /* the number of digits we split off */
    Py_ssize_t i;

    /* (ah*X+al)(bh*X+bl) = ah*bh*X*X + (ah*bl + al*bh)*X + al*bl
     * Let k = (ah+al)*(bh+bl) = ah*bl + al*bh  + ah*bh + al*bl
     * Then the original product is
     *     ah*bh*X*X + (k - ah*bh - al*bl)*X + al*bl
     * By picking X to be a power of 2, "*X" is just shifting, and it's
     * been reduced to 3 multiplies on numbers half the size.
     */

    /* We want to split based on the larger number; fiddle so that b
     * is largest.
     */
    if (asize > bsize) {
        t1 = a;
        a = b;
        b = t1;

        i = asize;
        asize = bsize;
        bsize = i;
    }

    /* Use gradeschool math when either number is too small. */
    i = a == b ? KARATSUBA_SQUARE_CUTOFF : KARATSUBA_CUTOFF;
    if (asize <= i) {
        if (asize == 0)
            return (PyLongObject *)PyLong_FromLong(0);
        else
            return x_mul(a, b);
    }

    /* If a is small compared to b, splitting on b gives a degenerate
     * case with ah==0, and Karatsuba may be (even much) less efficient
     * than "grade school" then.  However, we can still win, by viewing
     * b as a string of "big digits", each of the same width as a. That
     * leads to a sequence of balanced calls to k_mul.
     */
    if (2 * asize <= bsize)
        return k_lopsided_mul(a, b);

    /* Split a & b into hi & lo pieces. */
    shift = bsize >> 1;
    if (kmul_split(a, shift, &ah, &al) < 0) goto fail;
    assert(_PyLong_IsPositive(ah));        /* the split isn't degenerate */

    if (a == b) {
        bh = (PyLongObject*)Py_NewRef(ah);
        bl = (PyLongObject*)Py_NewRef(al);
    }
    else if (kmul_split(b, shift, &bh, &bl) < 0) goto fail;

    /* The plan:
     * 1. Allocate result space (asize + bsize digits:  that's always
     *    enough).
     * 2. Compute ah*bh, and copy into result at 2*shift.
     * 3. Compute al*bl, and copy into result at 0.  Note that this
     *    can't overlap with #2.
     * 4. Subtract al*bl from the result, starting at shift.  This may
     *    underflow (borrow out of the high digit), but we don't care:
     *    we're effectively doing unsigned arithmetic mod
     *    BASE**(sizea + sizeb), and so long as the *final* result fits,
     *    borrows and carries out of the high digit can be ignored.
     * 5. Subtract ah*bh from the result, starting at shift.
     * 6. Compute (ah+al)*(bh+bl), and add it into the result starting
     *    at shift.
     */

    /* 1. Allocate result space. */
    ret = _PyLong_New(asize + bsize);
    if (ret == NULL) goto fail;
#ifdef Py_DEBUG
    /* Fill with trash, to catch reference to uninitialized digits. */
    memset(ret->long_value.ob_digit, 0xDF, _PyLong_DigitCount(ret) * sizeof(digit));
#endif

    /* 2. t1 <- ah*bh, and copy into high digits of result. */
    if ((t1 = k_mul(ah, bh)) == NULL) goto fail;
    assert(!_PyLong_IsNegative(t1));
    assert(2*shift + _PyLong_DigitCount(t1) <= _PyLong_DigitCount(ret));
    memcpy(ret->long_value.ob_digit + 2*shift, t1->long_value.ob_digit,
           _PyLong_DigitCount(t1) * sizeof(digit));

    /* Zero-out the digits higher than the ah*bh copy. */
    i = _PyLong_DigitCount(ret) - 2*shift - _PyLong_DigitCount(t1);
    if (i)
        memset(ret->long_value.ob_digit + 2*shift + _PyLong_DigitCount(t1), 0,
               i * sizeof(digit));

    /* 3. t2 <- al*bl, and copy into the low digits. */
    if ((t2 = k_mul(al, bl)) == NULL) {
        Py_DECREF(t1);
        goto fail;
    }
    assert(!_PyLong_IsNegative(t2));
    assert(_PyLong_DigitCount(t2) <= 2*shift); /* no overlap with high digits */
    memcpy(ret->long_value.ob_digit, t2->long_value.ob_digit, _PyLong_DigitCount(t2) * sizeof(digit));

    /* Zero out remaining digits. */
    i = 2*shift - _PyLong_DigitCount(t2);          /* number of uninitialized digits */
    if (i)
        memset(ret->long_value.ob_digit + _PyLong_DigitCount(t2), 0, i * sizeof(digit));

    /* 4 & 5. Subtract ah*bh (t1) and al*bl (t2).  We do al*bl first
     * because it's fresher in cache.
     */
    i = _PyLong_DigitCount(ret) - shift;  /* # digits after shift */
    (void)v_isub(ret->long_value.ob_digit + shift, i, t2->long_value.ob_digit, _PyLong_DigitCount(t2));
    _Py_DECREF_INT(t2);

    (void)v_isub(ret->long_value.ob_digit + shift, i, t1->long_value.ob_digit, _PyLong_DigitCount(t1));
    _Py_DECREF_INT(t1);

    /* 6. t3 <- (ah+al)(bh+bl), and add into result. */
    if ((t1 = x_add(ah, al)) == NULL) goto fail;
    _Py_DECREF_INT(ah);
    _Py_DECREF_INT(al);
    ah = al = NULL;

    if (a == b) {
        t2 = (PyLongObject*)Py_NewRef(t1);
    }
    else if ((t2 = x_add(bh, bl)) == NULL) {
        Py_DECREF(t1);
        goto fail;
    }
    _Py_DECREF_INT(bh);
    _Py_DECREF_INT(bl);
    bh = bl = NULL;

    t3 = k_mul(t1, t2);
    _Py_DECREF_INT(t1);
    _Py_DECREF_INT(t2);
    if (t3 == NULL) goto fail;
    assert(!_PyLong_IsNegative(t3));

    /* Add t3.  It's not obvious why we can't run out of room here.
     * See the (*) comment after this function.
     */
    (void)v_iadd(ret->long_value.ob_digit + shift, i, t3->long_value.ob_digit, _PyLong_DigitCount(t3));
    _Py_DECREF_INT(t3);

    return long_normalize(ret);

  fail:
    Py_XDECREF(ret);
    Py_XDECREF(ah);
    Py_XDECREF(al);
    Py_XDECREF(bh);
    Py_XDECREF(bl);
    return NULL;
}

/* (*) Why adding t3 can't "run out of room" above.

Let f(x) mean the floor of x and c(x) mean the ceiling of x.  Some facts
to start with:

1. For any integer i, i = c(i/2) + f(i/2).  In particular,
   bsize = c(bsize/2) + f(bsize/2).
2. shift = f(bsize/2)
3. asize <= bsize
4. Since we call k_lopsided_mul if asize*2 <= bsize, asize*2 > bsize in this
   routine, so asize > bsize/2 >= f(bsize/2) in this routine.

We allocated asize + bsize result digits, and add t3 into them at an offset
of shift.  This leaves asize+bsize-shift allocated digit positions for t3
to fit into, = (by #1 and #2) asize + f(bsize/2) + c(bsize/2) - f(bsize/2) =
asize + c(bsize/2) available digit positions.

bh has c(bsize/2) digits, and bl at most f(size/2) digits.  So bh+hl has
at most c(bsize/2) digits + 1 bit.

If asize == bsize, ah has c(bsize/2) digits, else ah has at most f(bsize/2)
digits, and al has at most f(bsize/2) digits in any case.  So ah+al has at
most (asize == bsize ? c(bsize/2) : f(bsize/2)) digits + 1 bit.

The product (ah+al)*(bh+bl) therefore has at most

    c(bsize/2) + (asize == bsize ? c(bsize/2) : f(bsize/2)) digits + 2 bits

and we have asize + c(bsize/2) available digit positions.  We need to show
this is always enough.  An instance of c(bsize/2) cancels out in both, so
the question reduces to whether asize digits is enough to hold
(asize == bsize ? c(bsize/2) : f(bsize/2)) digits + 2 bits.  If asize < bsize,
then we're asking whether asize digits >= f(bsize/2) digits + 2 bits.  By #4,
asize is at least f(bsize/2)+1 digits, so this in turn reduces to whether 1
digit is enough to hold 2 bits.  This is so since PyLong_SHIFT=15 >= 2.  If
asize == bsize, then we're asking whether bsize digits is enough to hold
c(bsize/2) digits + 2 bits, or equivalently (by #1) whether f(bsize/2) digits
is enough to hold 2 bits.  This is so if bsize >= 2, which holds because
bsize >= KARATSUBA_CUTOFF >= 2.

Note that since there's always enough room for (ah+al)*(bh+bl), and that's
clearly >= each of ah*bh and al*bl, there's always enough room to subtract
ah*bh and al*bl too.
*/

/* b has at least twice the digits of a, and a is big enough that Karatsuba
 * would pay off *if* the inputs had balanced sizes.  View b as a sequence
 * of slices, each with the same number of digits as a, and multiply the
 * slices by a, one at a time.  This gives k_mul balanced inputs to work with,
 * and is also cache-friendly (we compute one double-width slice of the result
 * at a time, then move on, never backtracking except for the helpful
 * single-width slice overlap between successive partial sums).
 */
static PyLongObject *
k_lopsided_mul(PyLongObject *a, PyLongObject *b)
{
    const Py_ssize_t asize = _PyLong_DigitCount(a);
    Py_ssize_t bsize = _PyLong_DigitCount(b);
    Py_ssize_t nbdone;          /* # of b digits already multiplied */
    PyLongObject *ret;
    PyLongObject *bslice = NULL;

    assert(asize > KARATSUBA_CUTOFF);
    assert(2 * asize <= bsize);

    /* Allocate result space, and zero it out. */
    ret = _PyLong_New(asize + bsize);
    if (ret == NULL)
        return NULL;
    memset(ret->long_value.ob_digit, 0, _PyLong_DigitCount(ret) * sizeof(digit));

    /* Successive slices of b are copied into bslice. */
    bslice = _PyLong_New(asize);
    if (bslice == NULL)
        goto fail;

    nbdone = 0;
    while (bsize > 0) {
        PyLongObject *product;
        const Py_ssize_t nbtouse = Py_MIN(bsize, asize);

        /* Multiply the next slice of b by a. */
        memcpy(bslice->long_value.ob_digit, b->long_value.ob_digit + nbdone,
               nbtouse * sizeof(digit));
        assert(nbtouse >= 0);
        _PyLong_SetSignAndDigitCount(bslice, 1, nbtouse);
        product = k_mul(a, bslice);
        if (product == NULL)
            goto fail;

        /* Add into result. */
        (void)v_iadd(ret->long_value.ob_digit + nbdone, _PyLong_DigitCount(ret) - nbdone,
                     product->long_value.ob_digit, _PyLong_DigitCount(product));
        _Py_DECREF_INT(product);

        bsize -= nbtouse;
        nbdone += nbtouse;
    }

    _Py_DECREF_INT(bslice);
    return long_normalize(ret);

  fail:
    Py_DECREF(ret);
    Py_XDECREF(bslice);
    return NULL;
}

PyObject *
_PyLong_Multiply(PyLongObject *a, PyLongObject *b)
{
    PyLongObject *z;

    /* fast path for single-digit multiplication */
    if (_PyLong_BothAreCompact(a, b)) {
        stwodigits v = medium_value(a) * medium_value(b);
        return _PyLong_FromSTwoDigits(v);
    }

    z = k_mul(a, b);
    /* Negate if exactly one of the inputs is negative. */
    if (!_PyLong_SameSign(a, b) && z) {
        _PyLong_Negate(&z);
        if (z == NULL)
            return NULL;
    }
    return (PyObject *)z;
}



/* Fast modulo division for single-digit longs. */


/* Fast floor division for single-digit longs. */


#ifdef WITH_PYLONG_MODULE
/* asymptotically faster divmod, using _pylong.py */

#endif /* WITH_PYLONG_MODULE */

/* The / and % operators are now defined in terms of divmod().
   The expression a mod b has the value a - b*floor(a/b).
   The long_divrem function gives the remainder after division of
   |a| by |b|, with the sign of a.  This is also expressed
   as a - b*trunc(a/b), if trunc truncates towards zero.
   Some examples:
     a           b      a rem b         a mod b
     13          10      3               3
    -13          10     -3               7
     13         -10      3              -7
    -13         -10     -3              -3
   So, to get from rem to mod, we have to add b if a and b
   have different signs.  We then subtract one from the 'div'
   part of the outcome to keep the invariant intact. */

/* Compute
 *     *pdiv, *pmod = divmod(v, w)
 * NULL can be passed for pdiv or pmod, in which case that part of
 * the result is simply thrown away.  The caller owns a reference to
 * each of these it requests (does not pass NULL for).
 */


/* Compute
 *     *pmod = v % w
 * pmod cannot be NULL. The caller owns a reference to pmod.
 */




/* PyLong/PyLong -> float, with correctly rounded result. */

#define MANT_DIG_DIGITS (DBL_MANT_DIG / PyLong_SHIFT)
#define MANT_DIG_BITS (DBL_MANT_DIG % PyLong_SHIFT)








/* Compute an inverse to a modulo n, or raise ValueError if a is not
   invertible modulo n. Assumes n is positive. The inverse returned
   is whatever falls out of the extended Euclidean algorithm: it may
   be either positive or negative, but will be smaller than n in
   absolute value.

   Pure Python equivalent for long_invmod:

        def invmod(a, n):
            b, c = 1, 0
            while n:
                q, r = divmod(a, n)
                a, b, c, n = n, c, b - q*c, r

            # at this point a is the gcd of the original inputs
            if a == 1:
                return b
            raise ValueError("Not invertible")
*/




/* pow(v, w, x) */










/* wordshift, remshift = divmod(shiftby, PyLong_SHIFT) */


/* Inner function for both long_rshift and _PyLong_Rshift, shifting an
   integer right by PyLong_SHIFT*wordshift + remshift bits.
   wordshift should be nonnegative. */





/* Return a >> shiftby. */






/* Return a << shiftby. */


/* Compute two's complement of digit vector a[0:m], writing result to
   z[0:m].  The digit vector a need not be normalized, but should not
   be entirely zero.  a and z may point to the same digit vector. */



/* Bitwise and/xor/or operations */















static PyObject *
long_subtype_new(PyTypeObject *type, PyObject *x, PyObject *obase);

/*[clinic input]
@classmethod
int.__new__ as long_new
    x: object(c_default="NULL") = 0
    /
    base as obase: object(c_default="NULL") = 10
[clinic start generated code]*/



/* Wimpy, slow approach to tp_new calls for subtypes of int:
   first create a regular int from whatever arguments we got,
   then allocate a subtype instance and initialize it from
   the regular int.  The regular int is then thrown away.
*/


/*[clinic input]
int.__getnewargs__
[clinic start generated code]*/







/*[clinic input]
int.__format__

    format_spec: unicode
    /

Convert to a string according to format_spec.
[clinic start generated code]*/



/* Return a pair (q, r) such that a = b * q + r, and
   abs(r) <= abs(b)/2, with equality possible only if q is even.
   In other words, q == a / b, rounded to the nearest integer using
   round-half-to-even. */



/*[clinic input]
int.__round__

    ndigits as o_ndigits: object = NULL
    /

Rounding an Integral returns itself.

Rounding with an ndigits argument also returns an integer.
[clinic start generated code]*/



/*[clinic input]
int.__sizeof__ -> Py_ssize_t

Returns size in memory, in bytes.
[clinic start generated code]*/



/*[clinic input]
int.bit_length

Number of bits necessary to represent self in binary.

>>> bin(37)
'0b100101'
>>> (37).bit_length()
6
[clinic start generated code]*/





/*[clinic input]
int.bit_count

Number of ones in the binary representation of the absolute value of self.

Also known as the population count.

>>> bin(13)
'0b1101'
>>> (13).bit_count()
3
[clinic start generated code]*/



/*[clinic input]
int.as_integer_ratio

Return a pair of integers, whose ratio is equal to the original int.

The ratio is in lowest terms and has a positive denominator.

>>> (10).as_integer_ratio()
(10, 1)
>>> (-10).as_integer_ratio()
(-10, 1)
>>> (0).as_integer_ratio()
(0, 1)
[clinic start generated code]*/



/*[clinic input]
int.to_bytes

    length: Py_ssize_t = 1
        Length of bytes object to use.  An OverflowError is raised if the
        integer is not representable with the given number of bytes.  Default
        is length 1.
    byteorder: unicode(c_default="NULL") = "big"
        The byte order used to represent the integer.  If byteorder is 'big',
        the most significant byte is at the beginning of the byte array.  If
        byteorder is 'little', the most significant byte is at the end of the
        byte array.  To request the native byte order of the host system, use
        `sys.byteorder' as the byte order value.  Default is to use 'big'.
    *
    signed as is_signed: bool = False
        Determines whether two's complement is used to represent the integer.
        If signed is False and a negative integer is given, an OverflowError
        is raised.

Return an array of bytes representing an integer.
[clinic start generated code]*/



/*[clinic input]
@classmethod
int.from_bytes

    bytes as bytes_obj: object
        Holds the array of bytes to convert.  The argument must either
        support the buffer protocol or be an iterable object producing bytes.
        Bytes and bytearray are examples of built-in objects that support the
        buffer protocol.
    byteorder: unicode(c_default="NULL") = "big"
        The byte order used to represent the integer.  If byteorder is 'big',
        the most significant byte is at the beginning of the byte array.  If
        byteorder is 'little', the most significant byte is at the end of the
        byte array.  To request the native byte order of the host system, use
        `sys.byteorder' as the byte order value.  Default is to use 'big'.
    *
    signed as is_signed: bool = False
        Indicates whether two's complement is used to represent the integer.

Return the integer represented by the given array of bytes.
[clinic start generated code]*/





/*[clinic input]
int.is_integer

Returns True. Exists for duck type compatibility with float.is_integer.
[clinic start generated code]*/



;

;

PyDoc_STRVAR(long_doc,
"int([x]) -> integer\n\
int(x, base=10) -> integer\n\
\n\
Convert a number or string to an integer, or return 0 if no arguments\n\
are given.  If x is a number, return x.__int__().  For floating-point\n\
numbers, this truncates towards zero.\n\
\n\
If x is not a number or if base is given, then x must be a string,\n\
bytes, or bytearray instance representing an integer literal in the\n\
given base.  The literal can be preceded by '+' or '-' and be surrounded\n\
by whitespace.  The base defaults to 10.  Valid bases are 0 and 2-36.\n\
Base 0 means to interpret the base from the string as an integer literal.\n\
>>> int('0b100', base=0)\n\
4");

;

;

static PyTypeObject Int_InfoType;

PyDoc_STRVAR(int_info__doc__,
"sys.int_info\n\
\n\
A named tuple that holds information about Python's\n\
internal representation of integers.  The attributes are read only.");

;

;




/* runtime lifecycle */






#undef PyUnstable_Long_IsCompact



#undef PyUnstable_Long_CompactValue


