/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


static PyObject *
long_new_impl(PyTypeObject *type, PyObject *x, PyObject *obase);



PyDoc_STRVAR(int___getnewargs____doc__,
"__getnewargs__($self, /)\n"
"--\n"
"\n");

#define INT___GETNEWARGS___METHODDEF    \
    {"__getnewargs__", (PyCFunction)int___getnewargs__, METH_NOARGS, int___getnewargs____doc__},

static PyObject *
int___getnewargs___impl(PyObject *self);



PyDoc_STRVAR(int___format____doc__,
"__format__($self, format_spec, /)\n"
"--\n"
"\n"
"Convert to a string according to format_spec.");

#define INT___FORMAT___METHODDEF    \
    {"__format__", (PyCFunction)int___format__, METH_O, int___format____doc__},

static PyObject *
int___format___impl(PyObject *self, PyObject *format_spec);



PyDoc_STRVAR(int___round____doc__,
"__round__($self, ndigits=<unrepresentable>, /)\n"
"--\n"
"\n"
"Rounding an Integral returns itself.\n"
"\n"
"Rounding with an ndigits argument also returns an integer.");

#define INT___ROUND___METHODDEF    \
    {"__round__", _PyCFunction_CAST(int___round__), METH_FASTCALL, int___round____doc__},

static PyObject *
int___round___impl(PyObject *self, PyObject *o_ndigits);



PyDoc_STRVAR(int___sizeof____doc__,
"__sizeof__($self, /)\n"
"--\n"
"\n"
"Returns size in memory, in bytes.");

#define INT___SIZEOF___METHODDEF    \
    {"__sizeof__", (PyCFunction)int___sizeof__, METH_NOARGS, int___sizeof____doc__},

static Py_ssize_t
int___sizeof___impl(PyObject *self);



PyDoc_STRVAR(int_bit_length__doc__,
"bit_length($self, /)\n"
"--\n"
"\n"
"Number of bits necessary to represent self in binary.\n"
"\n"
">>> bin(37)\n"
"\'0b100101\'\n"
">>> (37).bit_length()\n"
"6");

#define INT_BIT_LENGTH_METHODDEF    \
    {"bit_length", (PyCFunction)int_bit_length, METH_NOARGS, int_bit_length__doc__},

static PyObject *
int_bit_length_impl(PyObject *self);



PyDoc_STRVAR(int_bit_count__doc__,
"bit_count($self, /)\n"
"--\n"
"\n"
"Number of ones in the binary representation of the absolute value of self.\n"
"\n"
"Also known as the population count.\n"
"\n"
">>> bin(13)\n"
"\'0b1101\'\n"
">>> (13).bit_count()\n"
"3");

#define INT_BIT_COUNT_METHODDEF    \
    {"bit_count", (PyCFunction)int_bit_count, METH_NOARGS, int_bit_count__doc__},

static PyObject *
int_bit_count_impl(PyObject *self);



PyDoc_STRVAR(int_as_integer_ratio__doc__,
"as_integer_ratio($self, /)\n"
"--\n"
"\n"
"Return a pair of integers, whose ratio is equal to the original int.\n"
"\n"
"The ratio is in lowest terms and has a positive denominator.\n"
"\n"
">>> (10).as_integer_ratio()\n"
"(10, 1)\n"
">>> (-10).as_integer_ratio()\n"
"(-10, 1)\n"
">>> (0).as_integer_ratio()\n"
"(0, 1)");

#define INT_AS_INTEGER_RATIO_METHODDEF    \
    {"as_integer_ratio", (PyCFunction)int_as_integer_ratio, METH_NOARGS, int_as_integer_ratio__doc__},

static PyObject *
int_as_integer_ratio_impl(PyObject *self);



PyDoc_STRVAR(int_to_bytes__doc__,
"to_bytes($self, /, length=1, byteorder=\'big\', *, signed=False)\n"
"--\n"
"\n"
"Return an array of bytes representing an integer.\n"
"\n"
"  length\n"
"    Length of bytes object to use.  An OverflowError is raised if the\n"
"    integer is not representable with the given number of bytes.  Default\n"
"    is length 1.\n"
"  byteorder\n"
"    The byte order used to represent the integer.  If byteorder is \'big\',\n"
"    the most significant byte is at the beginning of the byte array.  If\n"
"    byteorder is \'little\', the most significant byte is at the end of the\n"
"    byte array.  To request the native byte order of the host system, use\n"
"    `sys.byteorder\' as the byte order value.  Default is to use \'big\'.\n"
"  signed\n"
"    Determines whether two\'s complement is used to represent the integer.\n"
"    If signed is False and a negative integer is given, an OverflowError\n"
"    is raised.");

#define INT_TO_BYTES_METHODDEF    \
    {"to_bytes", _PyCFunction_CAST(int_to_bytes), METH_FASTCALL|METH_KEYWORDS, int_to_bytes__doc__},

static PyObject *
int_to_bytes_impl(PyObject *self, Py_ssize_t length, PyObject *byteorder,
                  int is_signed);



PyDoc_STRVAR(int_from_bytes__doc__,
"from_bytes($type, /, bytes, byteorder=\'big\', *, signed=False)\n"
"--\n"
"\n"
"Return the integer represented by the given array of bytes.\n"
"\n"
"  bytes\n"
"    Holds the array of bytes to convert.  The argument must either\n"
"    support the buffer protocol or be an iterable object producing bytes.\n"
"    Bytes and bytearray are examples of built-in objects that support the\n"
"    buffer protocol.\n"
"  byteorder\n"
"    The byte order used to represent the integer.  If byteorder is \'big\',\n"
"    the most significant byte is at the beginning of the byte array.  If\n"
"    byteorder is \'little\', the most significant byte is at the end of the\n"
"    byte array.  To request the native byte order of the host system, use\n"
"    `sys.byteorder\' as the byte order value.  Default is to use \'big\'.\n"
"  signed\n"
"    Indicates whether two\'s complement is used to represent the integer.");

#define INT_FROM_BYTES_METHODDEF    \
    {"from_bytes", _PyCFunction_CAST(int_from_bytes), METH_FASTCALL|METH_KEYWORDS|METH_CLASS, int_from_bytes__doc__},

static PyObject *
int_from_bytes_impl(PyTypeObject *type, PyObject *bytes_obj,
                    PyObject *byteorder, int is_signed);



PyDoc_STRVAR(int_is_integer__doc__,
"is_integer($self, /)\n"
"--\n"
"\n"
"Returns True. Exists for duck type compatibility with float.is_integer.");

#define INT_IS_INTEGER_METHODDEF    \
    {"is_integer", (PyCFunction)int_is_integer, METH_NOARGS, int_is_integer__doc__},

static PyObject *
int_is_integer_impl(PyObject *self);


/*[clinic end generated code: output=cfdf35d916158d4f input=a9049054013a1b77]*/
