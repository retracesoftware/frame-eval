/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


PyDoc_STRVAR(float_is_integer__doc__,
"is_integer($self, /)\n"
"--\n"
"\n"
"Return True if the float is an integer.");

#define FLOAT_IS_INTEGER_METHODDEF    \
    {"is_integer", (PyCFunction)float_is_integer, METH_NOARGS, float_is_integer__doc__},

static PyObject *
float_is_integer_impl(PyObject *self);



PyDoc_STRVAR(float___trunc____doc__,
"__trunc__($self, /)\n"
"--\n"
"\n"
"Return the Integral closest to x between 0 and x.");

#define FLOAT___TRUNC___METHODDEF    \
    {"__trunc__", (PyCFunction)float___trunc__, METH_NOARGS, float___trunc____doc__},

static PyObject *
float___trunc___impl(PyObject *self);



PyDoc_STRVAR(float___floor____doc__,
"__floor__($self, /)\n"
"--\n"
"\n"
"Return the floor as an Integral.");

#define FLOAT___FLOOR___METHODDEF    \
    {"__floor__", (PyCFunction)float___floor__, METH_NOARGS, float___floor____doc__},

static PyObject *
float___floor___impl(PyObject *self);



PyDoc_STRVAR(float___ceil____doc__,
"__ceil__($self, /)\n"
"--\n"
"\n"
"Return the ceiling as an Integral.");

#define FLOAT___CEIL___METHODDEF    \
    {"__ceil__", (PyCFunction)float___ceil__, METH_NOARGS, float___ceil____doc__},

static PyObject *
float___ceil___impl(PyObject *self);



PyDoc_STRVAR(float___round____doc__,
"__round__($self, ndigits=None, /)\n"
"--\n"
"\n"
"Return the Integral closest to x, rounding half toward even.\n"
"\n"
"When an argument is passed, work like built-in round(x, ndigits).");

#define FLOAT___ROUND___METHODDEF    \
    {"__round__", _PyCFunction_CAST(float___round__), METH_FASTCALL, float___round____doc__},

static PyObject *
float___round___impl(PyObject *self, PyObject *o_ndigits);



PyDoc_STRVAR(float_conjugate__doc__,
"conjugate($self, /)\n"
"--\n"
"\n"
"Return self, the complex conjugate of any float.");

#define FLOAT_CONJUGATE_METHODDEF    \
    {"conjugate", (PyCFunction)float_conjugate, METH_NOARGS, float_conjugate__doc__},

static PyObject *
float_conjugate_impl(PyObject *self);



PyDoc_STRVAR(float_hex__doc__,
"hex($self, /)\n"
"--\n"
"\n"
"Return a hexadecimal representation of a floating-point number.\n"
"\n"
">>> (-0.1).hex()\n"
"\'-0x1.999999999999ap-4\'\n"
">>> 3.14159.hex()\n"
"\'0x1.921f9f01b866ep+1\'");

#define FLOAT_HEX_METHODDEF    \
    {"hex", (PyCFunction)float_hex, METH_NOARGS, float_hex__doc__},

static PyObject *
float_hex_impl(PyObject *self);



PyDoc_STRVAR(float_fromhex__doc__,
"fromhex($type, string, /)\n"
"--\n"
"\n"
"Create a floating-point number from a hexadecimal string.\n"
"\n"
">>> float.fromhex(\'0x1.ffffp10\')\n"
"2047.984375\n"
">>> float.fromhex(\'-0x1p-1074\')\n"
"-5e-324");

#define FLOAT_FROMHEX_METHODDEF    \
    {"fromhex", (PyCFunction)float_fromhex, METH_O|METH_CLASS, float_fromhex__doc__},

PyDoc_STRVAR(float_as_integer_ratio__doc__,
"as_integer_ratio($self, /)\n"
"--\n"
"\n"
"Return a pair of integers, whose ratio is exactly equal to the original float.\n"
"\n"
"The ratio is in lowest terms and has a positive denominator.  Raise\n"
"OverflowError on infinities and a ValueError on NaNs.\n"
"\n"
">>> (10.0).as_integer_ratio()\n"
"(10, 1)\n"
">>> (0.0).as_integer_ratio()\n"
"(0, 1)\n"
">>> (-.25).as_integer_ratio()\n"
"(-1, 4)");

#define FLOAT_AS_INTEGER_RATIO_METHODDEF    \
    {"as_integer_ratio", (PyCFunction)float_as_integer_ratio, METH_NOARGS, float_as_integer_ratio__doc__},

static PyObject *
float_as_integer_ratio_impl(PyObject *self);



PyDoc_STRVAR(float_new__doc__,
"float(x=0, /)\n"
"--\n"
"\n"
"Convert a string or number to a floating-point number, if possible.");

static PyObject *
float_new_impl(PyTypeObject *type, PyObject *x);



PyDoc_STRVAR(float___getnewargs____doc__,
"__getnewargs__($self, /)\n"
"--\n"
"\n");

#define FLOAT___GETNEWARGS___METHODDEF    \
    {"__getnewargs__", (PyCFunction)float___getnewargs__, METH_NOARGS, float___getnewargs____doc__},

static PyObject *
float___getnewargs___impl(PyObject *self);



PyDoc_STRVAR(float___getformat____doc__,
"__getformat__($type, typestr, /)\n"
"--\n"
"\n"
"You probably don\'t want to use this function.\n"
"\n"
"  typestr\n"
"    Must be \'double\' or \'float\'.\n"
"\n"
"It exists mainly to be used in Python\'s test suite.\n"
"\n"
"This function returns whichever of \'unknown\', \'IEEE, big-endian\' or \'IEEE,\n"
"little-endian\' best describes the format of floating-point numbers used by the\n"
"C type named by typestr.");

#define FLOAT___GETFORMAT___METHODDEF    \
    {"__getformat__", (PyCFunction)float___getformat__, METH_O|METH_CLASS, float___getformat____doc__},

static PyObject *
float___getformat___impl(PyTypeObject *type, const char *typestr);



PyDoc_STRVAR(float___format____doc__,
"__format__($self, format_spec, /)\n"
"--\n"
"\n"
"Formats the float according to format_spec.");

#define FLOAT___FORMAT___METHODDEF    \
    {"__format__", (PyCFunction)float___format__, METH_O, float___format____doc__},

static PyObject *
float___format___impl(PyObject *self, PyObject *format_spec);


/*[clinic end generated code: output=e6e3f5f833b37eba input=a9049054013a1b77]*/
