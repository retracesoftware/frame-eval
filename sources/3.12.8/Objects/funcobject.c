
/* Function object implementation */

#include "Python.h"
#include "pycore_ceval.h"         // _PyEval_BuiltinsFromGlobals()
#include "pycore_code.h"          // _Py_next_func_version
#include "pycore_object.h"        // _PyObject_GC_UNTRACK()
#include "pycore_pyerrors.h"      // _PyErr_Occurred()
#include "structmember.h"         // PyMemberDef

static PyObject* func_repr(PyFunctionObject *op);














uint32_t _PyFunction_GetVersionForCurrentState(PyFunctionObject *func)
{
    if (func->func_version != 0) {
        return func->func_version;
    }
    if (func->vectorcall != _PyFunction_Vectorcall) {
        return 0;
    }
    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (interp->func_state.next_version == 0) {
        return 0;
    }
    uint32_t v = interp->func_state.next_version++;
    func->func_version = v;
    return v;
}





























/* Methods */

#define OFF(x) offsetof(PyFunctionObject, x)

;





























PyObject *
_Py_set_function_type_params(PyThreadState *Py_UNUSED(ignored), PyObject *func,
                             PyObject *type_params)
{
    assert(PyFunction_Check(func));
    assert(PyTuple_Check(type_params));
    PyFunctionObject *f = (PyFunctionObject *)func;
    Py_XSETREF(f->func_typeparams, Py_NewRef(type_params));
    return Py_NewRef(func);
}

;

/*[clinic input]
class function "PyFunctionObject *" "&PyFunction_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=70af9c90aa2e71b0]*/

#include "clinic/funcobject.c.h"

/* function.__new__() maintains the following invariants for closures.
   The closure must correspond to the free variables of the code object.

   if len(code.co_freevars) == 0:
       closure = NULL
   else:
       len(closure) == len(code.co_freevars)
   for every elt in closure, type(elt) == cell
*/

/*[clinic input]
@classmethod
function.__new__ as func_new
    code: object(type="PyCodeObject *", subclass_of="&PyCode_Type")
        a code object
    globals: object(subclass_of="&PyDict_Type")
        the globals dictionary
    name: object = None
        a string that overrides the name from the code object
    argdefs as defaults: object = None
        a tuple that specifies the default argument values
    closure: object = None
        a tuple that supplies the bindings for free variables

Create a function object.
[clinic start generated code]*/











/* Bind a function to an object */


;




// Similar to functools.wraps(wrapper, wrapped)



/* Class method object */

/* A class method receives the class as implicit first argument,
   just like an instance method receives the instance.
   To declare a class method, use this idiom:

     class C:
         @classmethod
         def f(cls, arg1, arg2, argN):
             ...

   It can be called either on the class (e.g. C.f()) or on an instance
   (e.g. C().f()); the instance is ignored except for its class.
   If a class method is called for a derived class, the derived class
   object is passed as the implied first argument.

   Class methods are different than C++ or Java static methods.
   If you want those, see static methods below.
*/

typedef struct {
    PyObject_HEAD
    PyObject *cm_callable;
    PyObject *cm_dict;
} classmethod;












;



;



PyDoc_STRVAR(classmethod_doc,
"classmethod(function) -> method\n\
\n\
Convert a function to be a class method.\n\
\n\
A class method receives the class as implicit first argument,\n\
just like an instance method receives the instance.\n\
To declare a class method, use this idiom:\n\
\n\
  class C:\n\
      @classmethod\n\
      def f(cls, arg1, arg2, argN):\n\
          ...\n\
\n\
It can be called either on the class (e.g. C.f()) or on an instance\n\
(e.g. C().f()).  The instance is ignored except for its class.\n\
If a class method is called for a derived class, the derived class\n\
object is passed as the implied first argument.\n\
\n\
Class methods are different than C++ or Java static methods.\n\
If you want those, see the staticmethod builtin.");

;




/* Static method object */

/* A static method does not receive an implicit first argument.
   To declare a static method, use this idiom:

     class C:
         @staticmethod
         def f(arg1, arg2, argN):
             ...

   It can be called either on the class (e.g. C.f()) or on an instance
   (e.g. C().f()). Both the class and the instance are ignored, and
   neither is passed implicitly as the first argument to the method.

   Static methods in Python are similar to those found in Java or C++.
   For a more advanced concept, see class methods above.
*/

typedef struct {
    PyObject_HEAD
    PyObject *sm_callable;
    PyObject *sm_dict;
} staticmethod;













;



;



PyDoc_STRVAR(staticmethod_doc,
"staticmethod(function) -> method\n\
\n\
Convert a function to be a static method.\n\
\n\
A static method does not receive an implicit first argument.\n\
To declare a static method, use this idiom:\n\
\n\
     class C:\n\
         @staticmethod\n\
         def f(arg1, arg2, argN):\n\
             ...\n\
\n\
It can be called either on the class (e.g. C.f()) or on an instance\n\
(e.g. C().f()). Both the class and the instance are ignored, and\n\
neither is passed implicitly as the first argument to the method.\n\
\n\
Static methods in Python are similar to those found in Java or C++.\n\
For a more advanced concept, see the classmethod builtin.");

;


