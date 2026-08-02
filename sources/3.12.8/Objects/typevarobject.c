// TypeVar, TypeVarTuple, and ParamSpec
#include "Python.h"
#include "pycore_object.h"  // _PyObject_GC_TRACK/UNTRACK
#include "pycore_typevarobject.h"
#include "pycore_unionobject.h"   // _Py_union_type_or
#include "structmember.h"

/*[clinic input]
class typevar "typevarobject *" "&_PyTypeVar_Type"
class paramspec "paramspecobject *" "&_PyParamSpec_Type"
class paramspecargs "paramspecattrobject *" "&_PyParamSpecArgs_Type"
class paramspeckwargs "paramspecattrobject *" "&_PyParamSpecKwargs_Type"
class typevartuple "typevartupleobject *" "&_PyTypeVarTuple_Type"
class typealias "typealiasobject *" "&_PyTypeAlias_Type"
class Generic "PyObject *" "&PyGeneric_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=aa86741931a0f55c]*/

typedef struct {
    PyObject_HEAD
    PyObject *name;
    PyObject *bound;
    PyObject *evaluate_bound;
    PyObject *constraints;
    PyObject *evaluate_constraints;
    bool covariant;
    bool contravariant;
    bool infer_variance;
} typevarobject;

typedef struct {
    PyObject_HEAD
    PyObject *name;
} typevartupleobject;

typedef struct {
    PyObject_HEAD
    PyObject *name;
    PyObject *bound;
    bool covariant;
    bool contravariant;
    bool infer_variance;
} paramspecobject;

typedef struct {
    PyObject_HEAD
    PyObject *name;
    PyObject *type_params;
    PyObject *compute_value;
    PyObject *value;
    PyObject *module;
} typealiasobject;

#include "clinic/typevarobject.c.h"

static PyObject *
call_typing_func_object(const char *name, PyObject **args, size_t nargs)
{
    PyObject *typing = PyImport_ImportModule("typing");
    if (typing == NULL) {
        return NULL;
    }
    PyObject *func = PyObject_GetAttrString(typing, name);
    if (func == NULL) {
        Py_DECREF(typing);
        return NULL;
    }
    PyObject *result = PyObject_Vectorcall(func, args, nargs, NULL);
    Py_DECREF(func);
    Py_DECREF(typing);
    return result;
}



/*
 * Return a typing.Union. This is used as the nb_or (|) operator for
 * TypeVar and ParamSpec. We use this rather than _Py_union_type_or
 * (which would produce a types.Union) because historically TypeVar
 * supported unions with string forward references, and we want to
 * preserve that behavior. _Py_union_type_or only allows a small set
 * of types.
 */


static PyObject *
caller(void)
{
    _PyInterpreterFrame *f = _PyThreadState_GET()->cframe->current_frame;
    if (f == NULL) {
        Py_RETURN_NONE;
    }
    if (f == NULL || f->f_funcobj == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *r = PyFunction_GetModule(f->f_funcobj);
    if (!r) {
        PyErr_Clear();
        Py_RETURN_NONE;
    }
    return Py_NewRef(r);
}

static PyObject *
typevartuple_unpack(PyObject *tvt)
{
    PyObject *typing = PyImport_ImportModule("typing");
    if (typing == NULL) {
        return NULL;
    }
    PyObject *unpack = PyObject_GetAttrString(typing, "Unpack");
    if (unpack == NULL) {
        Py_DECREF(typing);
        return NULL;
    }
    PyObject *unpacked = PyObject_GetItem(unpack, tvt);
    Py_DECREF(typing);
    Py_DECREF(unpack);
    return unpacked;
}

static int
contains_typevartuple(PyTupleObject *params)
{
    Py_ssize_t n = PyTuple_GET_SIZE(params);
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typevartuple_type;
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *param = PyTuple_GET_ITEM(params, i);
        if (Py_IS_TYPE(param, tp)) {
            return 1;
        }
    }
    return 0;
}

static PyObject *
unpack_typevartuples(PyObject *params)
{
    assert(PyTuple_Check(params));
    // TypeVarTuple must be unpacked when passed to Generic, so we do that here.
    if (contains_typevartuple((PyTupleObject *)params)) {
        Py_ssize_t n = PyTuple_GET_SIZE(params);
        PyObject *new_params = PyTuple_New(n);
        if (new_params == NULL) {
            return NULL;
        }
        PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typevartuple_type;
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject *param = PyTuple_GET_ITEM(params, i);
            if (Py_IS_TYPE(param, tp)) {
                PyObject *unpacked = typevartuple_unpack(param);
                if (unpacked == NULL) {
                    Py_DECREF(new_params);
                    return NULL;
                }
                PyTuple_SET_ITEM(new_params, i, unpacked);
            }
            else {
                PyTuple_SET_ITEM(new_params, i, Py_NewRef(param));
            }
        }
        return new_params;
    }
    else {
        return Py_NewRef(params);
    }
}









;





;

static typevarobject *
typevar_alloc(PyObject *name, PyObject *bound, PyObject *evaluate_bound,
              PyObject *constraints, PyObject *evaluate_constraints,
              bool covariant, bool contravariant, bool infer_variance,
              PyObject *module)
{
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typevar_type;
    assert(tp != NULL);
    typevarobject *tv = PyObject_GC_New(typevarobject, tp);
    if (tv == NULL) {
        return NULL;
    }

    tv->name = Py_NewRef(name);

    tv->bound = Py_XNewRef(bound);
    tv->evaluate_bound = Py_XNewRef(evaluate_bound);
    tv->constraints = Py_XNewRef(constraints);
    tv->evaluate_constraints = Py_XNewRef(evaluate_constraints);

    tv->covariant = covariant;
    tv->contravariant = contravariant;
    tv->infer_variance = infer_variance;
    _PyObject_GC_TRACK(tv);

    if (module != NULL) {
        if (PyObject_SetAttrString((PyObject *)tv, "__module__", module) < 0) {
            Py_DECREF(tv);
            return NULL;
        }
    }

    return tv;
}

/*[clinic input]
@classmethod
typevar.__new__ as typevar_new

    name: object(subclass_of="&PyUnicode_Type")
    *constraints: object
    *
    bound: object = None
    covariant: bool = False
    contravariant: bool = False
    infer_variance: bool = False

Create a TypeVar.
[clinic start generated code]*/



/*[clinic input]
typevar.__typing_subst__ as typevar_typing_subst

    arg: object

[clinic start generated code]*/



/*[clinic input]
typevar.__reduce__ as typevar_reduce

[clinic start generated code]*/





;

PyDoc_STRVAR(typevar_doc,
"Type variable.\n\
\n\
The preferred way to construct a type variable is via the dedicated\n\
syntax for generic functions, classes, and type aliases::\n\
\n\
    class Sequence[T]:  # T is a TypeVar\n\
        ...\n\
\n\
This syntax can also be used to create bound and constrained type\n\
variables::\n\
\n\
    # S is a TypeVar bound to str\n\
    class StrSequence[S: str]:\n\
        ...\n\
\n\
    # A is a TypeVar constrained to str or bytes\n\
    class StrOrBytesSequence[A: (str, bytes)]:\n\
        ...\n\
\n\
However, if desired, reusable type variables can also be constructed\n\
manually, like so::\n\
\n\
   T = TypeVar('T')  # Can be anything\n\
   S = TypeVar('S', bound=str)  # Can be any subtype of str\n\
   A = TypeVar('A', str, bytes)  # Must be exactly str or bytes\n\
\n\
Type variables exist primarily for the benefit of static type\n\
checkers.  They serve as the parameters for generic types as well\n\
as for generic function and type alias definitions.\n\
\n\
The variance of type variables is inferred by type checkers when they\n\
are created through the type parameter syntax and when\n\
``infer_variance=True`` is passed. Manually created type variables may\n\
be explicitly marked covariant or contravariant by passing\n\
``covariant=True`` or ``contravariant=True``. By default, manually\n\
created type variables are invariant. See PEP 484 and PEP 695 for more\n\
details.\n\
");

;

;

typedef struct {
    PyObject_HEAD
    PyObject *__origin__;
} paramspecattrobject;









;






/*[clinic input]
@classmethod
paramspecargs.__new__ as paramspecargs_new

    origin: object

Create a ParamSpecArgs object.
[clinic start generated code]*/





;

PyDoc_STRVAR(paramspecargs_doc,
"The args for a ParamSpec object.\n\
\n\
Given a ParamSpec object P, P.args is an instance of ParamSpecArgs.\n\
\n\
ParamSpecArgs objects have a reference back to their ParamSpec::\n\
\n\
    >>> P = ParamSpec(\"P\")\n\
    >>> P.args.__origin__ is P\n\
    True\n\
\n\
This type is meant for runtime introspection and has no special meaning\n\
to static type checkers.\n\
");

;

;



/*[clinic input]
@classmethod
paramspeckwargs.__new__ as paramspeckwargs_new

    origin: object

Create a ParamSpecKwargs object.
[clinic start generated code]*/





;

PyDoc_STRVAR(paramspeckwargs_doc,
"The kwargs for a ParamSpec object.\n\
\n\
Given a ParamSpec object P, P.kwargs is an instance of ParamSpecKwargs.\n\
\n\
ParamSpecKwargs objects have a reference back to their ParamSpec::\n\
\n\
    >>> P = ParamSpec(\"P\")\n\
    >>> P.kwargs.__origin__ is P\n\
    True\n\
\n\
This type is meant for runtime introspection and has no special meaning\n\
to static type checkers.\n\
");

;

;









;





;

static paramspecobject *
paramspec_alloc(PyObject *name, PyObject *bound, bool covariant,
                bool contravariant, bool infer_variance, PyObject *module)
{
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.paramspec_type;
    paramspecobject *ps = PyObject_GC_New(paramspecobject, tp);
    if (ps == NULL) {
        return NULL;
    }
    ps->name = Py_NewRef(name);
    ps->bound = Py_XNewRef(bound);
    ps->covariant = covariant;
    ps->contravariant = contravariant;
    ps->infer_variance = infer_variance;
    _PyObject_GC_TRACK(ps);
    if (module != NULL) {
        if (PyObject_SetAttrString((PyObject *)ps, "__module__", module) < 0) {
            Py_DECREF(ps);
            return NULL;
        }
    }
    return ps;
}

/*[clinic input]
@classmethod
paramspec.__new__ as paramspec_new

    name: object(subclass_of="&PyUnicode_Type")
    *
    bound: object = None
    covariant: bool = False
    contravariant: bool = False
    infer_variance: bool = False

Create a ParamSpec object.
[clinic start generated code]*/




/*[clinic input]
paramspec.__typing_subst__ as paramspec_typing_subst

    arg: object

[clinic start generated code]*/



/*[clinic input]
paramspec.__typing_prepare_subst__ as paramspec_typing_prepare_subst

    alias: object
    args: object

[clinic start generated code]*/



/*[clinic input]
paramspec.__reduce__ as paramspec_reduce

[clinic start generated code]*/





;

PyDoc_STRVAR(paramspec_doc,
"Parameter specification variable.\n\
\n\
The preferred way to construct a parameter specification is via the\n\
dedicated syntax for generic functions, classes, and type aliases,\n\
where the use of '**' creates a parameter specification::\n\
\n\
    type IntFunc[**P] = Callable[P, int]\n\
\n\
For compatibility with Python 3.11 and earlier, ParamSpec objects\n\
can also be created as follows::\n\
\n\
    P = ParamSpec('P')\n\
\n\
Parameter specification variables exist primarily for the benefit of\n\
static type checkers.  They are used to forward the parameter types of\n\
one callable to another callable, a pattern commonly found in\n\
higher-order functions and decorators.  They are only valid when used\n\
in ``Concatenate``, or as the first argument to ``Callable``, or as\n\
parameters for user-defined Generics. See class Generic for more\n\
information on generic types.\n\
\n\
An example for annotating a decorator::\n\
\n\
    def add_logging[**P, T](f: Callable[P, T]) -> Callable[P, T]:\n\
        '''A type-safe decorator to add logging to a function.'''\n\
        def inner(*args: P.args, **kwargs: P.kwargs) -> T:\n\
            logging.info(f'{f.__name__} was called')\n\
            return f(*args, **kwargs)\n\
        return inner\n\
\n\
    @add_logging\n\
    def add_two(x: float, y: float) -> float:\n\
        '''Add two numbers together.'''\n\
        return x + y\n\
\n\
Parameter specification variables can be introspected. e.g.::\n\
\n\
    >>> P = ParamSpec(\"P\")\n\
    >>> P.__name__\n\
    'P'\n\
\n\
Note that only parameter specification variables defined in the global\n\
scope can be pickled.\n\
");

;

;







;

static typevartupleobject *
typevartuple_alloc(PyObject *name, PyObject *module)
{
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typevartuple_type;
    typevartupleobject *tvt = PyObject_GC_New(typevartupleobject, tp);
    if (tvt == NULL) {
        return NULL;
    }
    tvt->name = Py_NewRef(name);
    _PyObject_GC_TRACK(tvt);
    if (module != NULL) {
        if (PyObject_SetAttrString((PyObject *)tvt, "__module__", module) < 0) {
            Py_DECREF(tvt);
            return NULL;
        }
    }
    return tvt;
}

/*[clinic input]
@classmethod
typevartuple.__new__

    name: object(subclass_of="&PyUnicode_Type")

Create a new TypeVarTuple with the given name.
[clinic start generated code]*/



/*[clinic input]
typevartuple.__typing_subst__ as typevartuple_typing_subst

    arg: object

[clinic start generated code]*/



/*[clinic input]
typevartuple.__typing_prepare_subst__ as typevartuple_typing_prepare_subst

    alias: object
    args: object

[clinic start generated code]*/



/*[clinic input]
typevartuple.__reduce__ as typevartuple_reduce

[clinic start generated code]*/









;

PyDoc_STRVAR(typevartuple_doc,
"Type variable tuple. A specialized form of type variable that enables\n\
variadic generics.\n\
\n\
The preferred way to construct a type variable tuple is via the\n\
dedicated syntax for generic functions, classes, and type aliases,\n\
where a single '*' indicates a type variable tuple::\n\
\n\
    def move_first_element_to_last[T, *Ts](tup: tuple[T, *Ts]) -> tuple[*Ts, T]:\n\
        return (*tup[1:], tup[0])\n\
\n\
For compatibility with Python 3.11 and earlier, TypeVarTuple objects\n\
can also be created as follows::\n\
\n\
    Ts = TypeVarTuple('Ts')  # Can be given any name\n\
\n\
Just as a TypeVar (type variable) is a placeholder for a single type,\n\
a TypeVarTuple is a placeholder for an *arbitrary* number of types. For\n\
example, if we define a generic class using a TypeVarTuple::\n\
\n\
    class C[*Ts]: ...\n\
\n\
Then we can parameterize that class with an arbitrary number of type\n\
arguments::\n\
\n\
    C[int]       # Fine\n\
    C[int, str]  # Also fine\n\
    C[()]        # Even this is fine\n\
\n\
For more details, see PEP 646.\n\
\n\
Note that only TypeVarTuples defined in the global scope can be\n\
pickled.\n\
");

;

;

PyObject *
_Py_make_typevar(PyObject *name, PyObject *evaluate_bound, PyObject *evaluate_constraints)
{
    return (PyObject *)typevar_alloc(name, NULL, evaluate_bound, NULL, evaluate_constraints,
                                     false, false, true, NULL);
}

PyObject *
_Py_make_paramspec(PyThreadState *Py_UNUSED(ignored), PyObject *v)
{
    assert(PyUnicode_Check(v));
    return (PyObject *)paramspec_alloc(v, NULL, false, false, true, NULL);
}

PyObject *
_Py_make_typevartuple(PyThreadState *Py_UNUSED(ignored), PyObject *v)
{
    assert(PyUnicode_Check(v));
    return (PyObject *)typevartuple_alloc(v, NULL);
}

static void
typealias_dealloc(PyObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    _PyObject_GC_UNTRACK(self);
    typealiasobject *ta = (typealiasobject *)self;
    Py_DECREF(ta->name);
    Py_XDECREF(ta->type_params);
    Py_XDECREF(ta->compute_value);
    Py_XDECREF(ta->value);
    Py_XDECREF(ta->module);
    Py_TYPE(self)->tp_free(self);
    Py_DECREF(tp);
}

static PyObject *
typealias_get_value(typealiasobject *ta)
{
    if (ta->value != NULL) {
        return Py_NewRef(ta->value);
    }
    PyObject *result = PyObject_CallNoArgs(ta->compute_value);
    if (result == NULL) {
        return NULL;
    }
    ta->value = Py_NewRef(result);
    return result;
}

static PyObject *
typealias_repr(PyObject *self)
{
    typealiasobject *ta = (typealiasobject *)self;
    return Py_NewRef(ta->name);
}

static PyMemberDef typealias_members[] = {
    {"__name__", T_OBJECT, offsetof(typealiasobject, name), READONLY},
    {0}
};

static PyObject *
typealias_value(PyObject *self, void *unused)
{
    typealiasobject *ta = (typealiasobject *)self;
    return typealias_get_value(ta);
}

static PyObject *
typealias_parameters(PyObject *self, void *unused)
{
    typealiasobject *ta = (typealiasobject *)self;
    if (ta->type_params == NULL) {
        return PyTuple_New(0);
    }
    return unpack_typevartuples(ta->type_params);
}

static PyObject *
typealias_type_params(PyObject *self, void *unused)
{
    typealiasobject *ta = (typealiasobject *)self;
    if (ta->type_params == NULL) {
        return PyTuple_New(0);
    }
    return Py_NewRef(ta->type_params);
}

static PyObject *
typealias_module(PyObject *self, void *unused)
{
    typealiasobject *ta = (typealiasobject *)self;
    if (ta->module != NULL) {
        return Py_NewRef(ta->module);
    }
    if (ta->compute_value != NULL) {
        PyObject* mod = PyFunction_GetModule(ta->compute_value);
        if (mod != NULL) {
            // PyFunction_GetModule() returns a borrowed reference,
            // and it may return NULL (e.g., for functions defined
            // in an exec()'ed block).
            return Py_NewRef(mod);
        }
    }
    Py_RETURN_NONE;
}

static PyGetSetDef typealias_getset[] = {
    {"__parameters__", typealias_parameters, (setter)NULL, NULL, NULL},
    {"__type_params__", typealias_type_params, (setter)NULL, NULL, NULL},
    {"__value__", typealias_value, (setter)NULL, NULL, NULL},
    {"__module__", typealias_module, (setter)NULL, NULL, NULL},
    {0}
};

static typealiasobject *
typealias_alloc(PyObject *name, PyObject *type_params, PyObject *compute_value,
                PyObject *value, PyObject *module)
{
    typealiasobject *ta = PyObject_GC_New(typealiasobject, &_PyTypeAlias_Type);
    if (ta == NULL) {
        return NULL;
    }
    ta->name = Py_NewRef(name);
    if (
        type_params == NULL
        || Py_IsNone(type_params)
        || (PyTuple_Check(type_params) && PyTuple_GET_SIZE(type_params) == 0)
    ) {
        ta->type_params = NULL;
    }
    else {
        ta->type_params = Py_NewRef(type_params);
    }
    ta->compute_value = Py_XNewRef(compute_value);
    ta->value = Py_XNewRef(value);
    ta->module = Py_XNewRef(module);
    _PyObject_GC_TRACK(ta);
    return ta;
}

static int
typealias_traverse(typealiasobject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->type_params);
    Py_VISIT(self->compute_value);
    Py_VISIT(self->value);
    Py_VISIT(self->module);
    return 0;
}

static int
typealias_clear(typealiasobject *self)
{
    Py_CLEAR(self->type_params);
    Py_CLEAR(self->compute_value);
    Py_CLEAR(self->value);
    Py_CLEAR(self->module);
    return 0;
}

/*[clinic input]
typealias.__reduce__ as typealias_reduce

[clinic start generated code]*/

static PyObject *
typealias_reduce_impl(typealiasobject *self)
/*[clinic end generated code: output=913724f92ad3b39b input=4f06fbd9472ec0f1]*/
{
    return Py_NewRef(self->name);
}

static PyObject *
typealias_subscript(PyObject *self, PyObject *args)
{
    if (((typealiasobject *)self)->type_params == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Only generic type aliases are subscriptable");
        return NULL;
    }
    return Py_GenericAlias(self, args);
}

static PyMethodDef typealias_methods[] = {
    TYPEALIAS_REDUCE_METHODDEF
    {0}
};


/*[clinic input]
@classmethod
typealias.__new__ as typealias_new

    name: object(subclass_of="&PyUnicode_Type")
    value: object
    *
    type_params: object = NULL

Create a TypeAliasType.
[clinic start generated code]*/

static PyObject *
typealias_new_impl(PyTypeObject *type, PyObject *name, PyObject *value,
                   PyObject *type_params)
/*[clinic end generated code: output=8920ce6bdff86f00 input=df163c34e17e1a35]*/
{
    if (type_params != NULL && !PyTuple_Check(type_params)) {
        PyErr_SetString(PyExc_TypeError, "type_params must be a tuple");
        return NULL;
    }
    PyObject *module = caller();
    if (module == NULL) {
        return NULL;
    }
    PyObject *ta = (PyObject *)typealias_alloc(name, type_params, NULL, value,
                                               module);
    Py_DECREF(module);
    return ta;
}

PyDoc_STRVAR(typealias_doc,
"Type alias.\n\
\n\
Type aliases are created through the type statement::\n\
\n\
    type Alias = int\n\
\n\
In this example, Alias and int will be treated equivalently by static\n\
type checkers.\n\
\n\
At runtime, Alias is an instance of TypeAliasType. The __name__\n\
attribute holds the name of the type alias. The value of the type alias\n\
is stored in the __value__ attribute. It is evaluated lazily, so the\n\
value is computed only if the attribute is accessed.\n\
\n\
Type aliases can also be generic::\n\
\n\
    type ListOrSet[T] = list[T] | set[T]\n\
\n\
In this case, the type parameters of the alias are stored in the\n\
__type_params__ attribute.\n\
\n\
See PEP 695 for more information.\n\
");

static PyNumberMethods typealias_as_number = {
    .nb_or = _Py_union_type_or,
};

static PyMappingMethods typealias_as_mapping = {
    .mp_subscript = typealias_subscript,
};

PyTypeObject _PyTypeAlias_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "typing.TypeAliasType",
    .tp_basicsize = sizeof(typealiasobject),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE | Py_TPFLAGS_HAVE_GC,
    .tp_doc = typealias_doc,
    .tp_members = typealias_members,
    .tp_methods = typealias_methods,
    .tp_getset = typealias_getset,
    .tp_alloc = PyType_GenericAlloc,
    .tp_dealloc = typealias_dealloc,
    .tp_new = typealias_new,
    .tp_free = PyObject_GC_Del,
    .tp_traverse = (traverseproc)typealias_traverse,
    .tp_clear = (inquiry)typealias_clear,
    .tp_repr = typealias_repr,
    .tp_as_number = &typealias_as_number,
    .tp_as_mapping = &typealias_as_mapping,
};

PyObject *
_Py_make_typealias(PyThreadState* unused, PyObject *args)
{
    assert(PyTuple_Check(args));
    assert(PyTuple_GET_SIZE(args) == 3);
    PyObject *name = PyTuple_GET_ITEM(args, 0);
    assert(PyUnicode_Check(name));
    PyObject *type_params = PyTuple_GET_ITEM(args, 1);
    PyObject *compute_value = PyTuple_GET_ITEM(args, 2);
    assert(PyFunction_Check(compute_value));
    return (PyObject *)typealias_alloc(name, type_params, compute_value, NULL, NULL);
}

PyDoc_STRVAR(generic_doc,
"Abstract base class for generic types.\n\
\n\
On Python 3.12 and newer, generic classes implicitly inherit from\n\
Generic when they declare a parameter list after the class's name::\n\
\n\
    class Mapping[KT, VT]:\n\
        def __getitem__(self, key: KT) -> VT:\n\
            ...\n\
        # Etc.\n\
\n\
On older versions of Python, however, generic classes have to\n\
explicitly inherit from Generic.\n\
\n\
After a class has been declared to be generic, it can then be used as\n\
follows::\n\
\n\
    def lookup_name[KT, VT](mapping: Mapping[KT, VT], key: KT, default: VT) -> VT:\n\
        try:\n\
            return mapping[key]\n\
        except KeyError:\n\
            return default\n\
");

PyDoc_STRVAR(generic_class_getitem_doc,
"Parameterizes a generic class.\n\
\n\
At least, parameterizing a generic class is the *main* thing this\n\
method does. For example, for some generic class `Foo`, this is called\n\
when we do `Foo[int]` - there, with `cls=Foo` and `params=int`.\n\
\n\
However, note that this method is also called when defining generic\n\
classes in the first place with `class Foo[T]: ...`.\n\
");







PyObject *
_Py_subscript_generic(PyThreadState* unused, PyObject *params)
{
    params = unpack_typevartuples(params);

    PyInterpreterState *interp = PyInterpreterState_Get();
    if (interp->cached_objects.generic_type == NULL) {
        PyErr_SetString(PyExc_SystemError, "Cannot find Generic type");
        return NULL;
    }
    PyObject *args[2] = {(PyObject *)interp->cached_objects.generic_type, params};
    PyObject *result = call_typing_func_object("_GenericAlias", args, 2);
    Py_DECREF(params);
    return result;
}

;





;

;




