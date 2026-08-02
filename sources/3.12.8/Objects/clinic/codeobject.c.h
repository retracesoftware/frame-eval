/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


PyDoc_STRVAR(code_new__doc__,
"code(argcount, posonlyargcount, kwonlyargcount, nlocals, stacksize,\n"
"     flags, codestring, constants, names, varnames, filename, name,\n"
"     qualname, firstlineno, linetable, exceptiontable, freevars=(),\n"
"     cellvars=(), /)\n"
"--\n"
"\n"
"Create a code object.  Not for the faint of heart.");

static PyObject *
code_new_impl(PyTypeObject *type, int argcount, int posonlyargcount,
              int kwonlyargcount, int nlocals, int stacksize, int flags,
              PyObject *code, PyObject *consts, PyObject *names,
              PyObject *varnames, PyObject *filename, PyObject *name,
              PyObject *qualname, int firstlineno, PyObject *linetable,
              PyObject *exceptiontable, PyObject *freevars,
              PyObject *cellvars);



PyDoc_STRVAR(code_replace__doc__,
"replace($self, /, **changes)\n"
"--\n"
"\n"
"Return a copy of the code object with new values for the specified fields.");

#define CODE_REPLACE_METHODDEF    \
    {"replace", _PyCFunction_CAST(code_replace), METH_FASTCALL|METH_KEYWORDS, code_replace__doc__},

static PyObject *
code_replace_impl(PyCodeObject *self, int co_argcount,
                  int co_posonlyargcount, int co_kwonlyargcount,
                  int co_nlocals, int co_stacksize, int co_flags,
                  int co_firstlineno, PyObject *co_code, PyObject *co_consts,
                  PyObject *co_names, PyObject *co_varnames,
                  PyObject *co_freevars, PyObject *co_cellvars,
                  PyObject *co_filename, PyObject *co_name,
                  PyObject *co_qualname, PyObject *co_linetable,
                  PyObject *co_exceptiontable);



PyDoc_STRVAR(code__varname_from_oparg__doc__,
"_varname_from_oparg($self, /, oparg)\n"
"--\n"
"\n"
"(internal-only) Return the local variable name for the given oparg.\n"
"\n"
"WARNING: this method is for internal use only and may change or go away.");

#define CODE__VARNAME_FROM_OPARG_METHODDEF    \
    {"_varname_from_oparg", _PyCFunction_CAST(code__varname_from_oparg), METH_FASTCALL|METH_KEYWORDS, code__varname_from_oparg__doc__},

static PyObject *
code__varname_from_oparg_impl(PyCodeObject *self, int oparg);


/*[clinic end generated code: output=ff40f7bdd3851de3 input=a9049054013a1b77]*/
