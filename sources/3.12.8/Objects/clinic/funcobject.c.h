/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


PyDoc_STRVAR(func_new__doc__,
"function(code, globals, name=None, argdefs=None, closure=None)\n"
"--\n"
"\n"
"Create a function object.\n"
"\n"
"  code\n"
"    a code object\n"
"  globals\n"
"    the globals dictionary\n"
"  name\n"
"    a string that overrides the name from the code object\n"
"  argdefs\n"
"    a tuple that specifies the default argument values\n"
"  closure\n"
"    a tuple that supplies the bindings for free variables");

static PyObject *
func_new_impl(PyTypeObject *type, PyCodeObject *code, PyObject *globals,
              PyObject *name, PyObject *defaults, PyObject *closure);


/*[clinic end generated code: output=777cead7b1f6fad3 input=a9049054013a1b77]*/
