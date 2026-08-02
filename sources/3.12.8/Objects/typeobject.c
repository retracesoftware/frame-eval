/* Type object implementation */

#include "Python.h"
#include "pycore_call.h"
#include "pycore_code.h"          // CO_FAST_FREE
#include "pycore_symtable.h"      // _Py_Mangle()
#include "pycore_dict.h"          // _PyDict_KeysSize()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_memoryobject.h"  // _PyMemoryView_FromBufferProc()
#include "pycore_moduleobject.h"  // _PyModule_GetDef()
#include "pycore_object.h"        // _PyType_HasFeature()
#include "pycore_long.h"          // _PyLong_IsNegative()
#include "pycore_pyerrors.h"      // _PyErr_Occurred()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_typeobject.h"    // struct type_cache
#include "pycore_unionobject.h"   // _Py_union_type_or
#include "pycore_frame.h"         // _PyInterpreterFrame
#include "opcode.h"               // MAKE_CELL
#include "structmember.h"         // PyMemberDef

#include <ctype.h>
#include <stddef.h>               // ptrdiff_t

/*[clinic input]
class type "PyTypeObject *" "&PyType_Type"
class object "PyObject *" "&PyBaseObject_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=4b94608d231c434b]*/

#include "clinic/typeobject.c.h"

/* Support type attribute lookup cache */

/* The cache can keep references to the names alive for longer than
   they normally would.  This is why the maximum size is limited to
   MCACHE_MAX_ATTR_SIZE, since it might be a problem if very large
   strings are used as attribute names. */
#define MCACHE_MAX_ATTR_SIZE    100
#define MCACHE_HASH(version, name_hash)                                 \
        (((unsigned int)(version) ^ (unsigned int)(name_hash))          \
         & ((1 << MCACHE_SIZE_EXP) - 1))

#define MCACHE_HASH_METHOD(type, name)                                  \
    MCACHE_HASH((type)->tp_version_tag, ((Py_ssize_t)(name)) >> 3)
#define MCACHE_CACHEABLE_NAME(name)                             \
        PyUnicode_CheckExact(name) &&                           \
        PyUnicode_IS_READY(name) &&                             \
        (PyUnicode_GET_LENGTH(name) <= MCACHE_MAX_ATTR_SIZE)

#define NEXT_GLOBAL_VERSION_TAG _PyRuntime.types.next_version_tag
#define NEXT_VERSION_TAG(interp) \
    (interp)->types.next_version_tag

typedef struct PySlot_Offset {
    short subslot_offset;
    short slot_offset;
} PySlot_Offset;

static void
slot_bf_releasebuffer(PyObject *self, Py_buffer *buffer);

static void
releasebuffer_call_python(PyObject *self, Py_buffer *buffer);

static PyObject *
slot_tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

static PyObject *
lookup_maybe_method(PyObject *self, PyObject *attr, int *unbound);

static int
slot_tp_setattro(PyObject *self, PyObject *name, PyObject *value);





/* helpers for for static builtin types */



static inline size_t
static_builtin_index_get(PyTypeObject *self)
{
    assert(static_builtin_index_is_set(self));
    /* We store a 1-based index so 0 can mean "not initialized". */
    return (size_t)self->tp_subclasses - 1;
}






static inline static_builtin_state *
static_builtin_state_get(PyInterpreterState *interp, PyTypeObject *self)
{
    return &(interp->types.builtins[static_builtin_index_get(self)]);
}

/* For static types we store some state in an array on each interpreter. */
static_builtin_state *
_PyStaticType_GetState(PyInterpreterState *interp, PyTypeObject *self)
{
    assert(self->tp_flags & _Py_TPFLAGS_STATIC_BUILTIN);
    return static_builtin_state_get(interp, self);
}

/* Set the type's per-interpreter state. */


/* Reset the type's per-interpreter state.
   This basically undoes what static_builtin_state_init() did. */


// Also see _PyStaticType_InitBuiltin() and _PyStaticType_Dealloc().

/* end static builtin helpers */









/* accessors for objects stored on PyTypeObject */

static inline PyObject *
lookup_tp_dict(PyTypeObject *self)
{
    if (self->tp_flags & _Py_TPFLAGS_STATIC_BUILTIN) {
        PyInterpreterState *interp = _PyInterpreterState_GET();
        static_builtin_state *state = _PyStaticType_GetState(interp, self);
        assert(state != NULL);
        return state->tp_dict;
    }
    return self->tp_dict;
}



















static inline PyObject *
lookup_tp_mro(PyTypeObject *self)
{
    return self->tp_mro;
}


















/* end accessors for objects stored on PyTypeObject */


/*
 * finds the beginning of the docstring's introspection signature.
 * if present, returns a pointer pointing to the first '('.
 * otherwise returns NULL.
 *
 * doesn't guarantee that the signature is valid, only that it
 * has a valid prefix.  (the signature must also pass skip_signature.)
 */


#define SIGNATURE_END_MARKER         ")\n--\n\n"
#define SIGNATURE_END_MARKER_LENGTH  6
/*
 * skips past the end of the docstring's introspection signature.
 * (assumes doc starts with a valid signature prefix.)
 */



































static int assign_version_tag(PyInterpreterState *interp, PyTypeObject *type);














;

























static PyTypeObject *best_base(PyObject *);
static int mro_internal(PyTypeObject *, PyObject **);
static int type_is_subtype_base_chain(PyTypeObject *, PyTypeObject *);
static int compatible_for_assignment(PyTypeObject *, PyTypeObject *, const char *);
static int add_subclass(PyTypeObject*, PyTypeObject*);
static int add_all_subclasses(PyTypeObject *type, PyObject *bases);
static void remove_subclass(PyTypeObject *, PyTypeObject *);
static void remove_all_subclasses(PyTypeObject *type, PyObject *bases);
static void update_all_slots(PyTypeObject *);

typedef int (*update_callback)(PyTypeObject *, void *);
static int update_subclasses(PyTypeObject *type, PyObject *attr_name,
                             update_callback callback, void *data);
static int recurse_down_subclasses(PyTypeObject *type, PyObject *name,
                                   update_callback callback, void *data);






















/*[clinic input]
type.__instancecheck__ -> bool

    instance: object
    /

Check if an object is an instance.
[clinic start generated code]*/



/*[clinic input]
type.__subclasscheck__ -> bool

    subclass: object
    /

Check if a class is a subclass.
[clinic start generated code]*/




;











/* Helpers for subtyping */













static PyTypeObject *solid_base(PyTypeObject *type);

/* type test with subclassing support */





/* Routines to do a method lookup in the type without looking in the
   instance dictionary (so we can't use PyObject_GetAttr) but still
   binding it to the instance.

   Variants:

   - _PyObject_LookupSpecial() returns NULL without raising an exception
     when the _PyType_Lookup() call fails;

   - lookup_maybe_method() and lookup_method() are internal routines similar
     to _PyObject_LookupSpecial(), but can return unbound PyFunction
     to avoid temporary method object. Pass self as first argument when
     unbound == 1.
*/





static PyObject *
lookup_maybe_method(PyObject *self, PyObject *attr, int *unbound)
{
    PyObject *res = _PyType_Lookup(Py_TYPE(self), attr);
    if (res == NULL) {
        return NULL;
    }

    if (_PyType_HasFeature(Py_TYPE(res), Py_TPFLAGS_METHOD_DESCRIPTOR)) {
        /* Avoid temporary PyMethodObject */
        *unbound = 1;
        Py_INCREF(res);
    }
    else {
        *unbound = 0;
        descrgetfunc f = Py_TYPE(res)->tp_descr_get;
        if (f == NULL) {
            Py_INCREF(res);
        }
        else {
            res = f(res, self, (PyObject *)(Py_TYPE(self)));
        }
    }
    return res;
}

static PyObject *
lookup_method(PyObject *self, PyObject *attr, int *unbound)
{
    PyObject *res = lookup_maybe_method(self, attr, unbound);
    if (res == NULL && !PyErr_Occurred()) {
        PyErr_SetObject(PyExc_AttributeError, attr);
    }
    return res;
}


static inline PyObject*
vectorcall_unbound(PyThreadState *tstate, int unbound, PyObject *func,
                   PyObject *const *args, Py_ssize_t nargs)
{
    size_t nargsf = nargs;
    if (!unbound) {
        /* Skip self argument, freeing up args[0] to use for
         * PY_VECTORCALL_ARGUMENTS_OFFSET */
        args++;
        nargsf = nargsf - 1 + PY_VECTORCALL_ARGUMENTS_OFFSET;
    }
    EVAL_CALL_STAT_INC_IF_FUNCTION(EVAL_CALL_SLOT, func);
    return _PyObject_VectorcallTstate(tstate, func, args, nargsf, NULL);
}



/* A variation of PyObject_CallMethod* that uses lookup_method()
   instead of PyObject_GetAttrString().

   args is an argument vector of length nargs. The first element in this
   vector is the special object "self" which is used for the method lookup */
static PyObject *
vectorcall_method(PyObject *name, PyObject *const *args, Py_ssize_t nargs)
{
    assert(nargs >= 1);

    PyThreadState *tstate = _PyThreadState_GET();
    int unbound;
    PyObject *self = args[0];
    PyObject *func = lookup_method(self, name, &unbound);
    if (func == NULL) {
        return NULL;
    }
    PyObject *retval = vectorcall_unbound(tstate, unbound, func, args, nargs);
    Py_DECREF(func);
    return retval;
}

/* Clone of vectorcall_method() that returns NotImplemented
 * when the lookup fails. */


/*
    Method resolution order algorithm C3 described in
    "A Monotonic Superclass Linearization for Dylan",
    by Kim Barrett, Bob Cassel, Paul Haahr,
    David A. Moon, Keith Playford, and P. Tucker Withington.
    (OOPSLA 1996)

    Some notes about the rules implied by C3:

    No duplicate bases.
    It isn't legal to repeat a class in a list of base classes.

    The next three properties are the 3 constraints in "C3".

    Local precedence order.
    If A precedes B in C's MRO, then A will precede B in the MRO of all
    subclasses of C.

    Monotonicity.
    The MRO of a class must be an extension without reordering of the
    MRO of each of its superclasses.

    Extended Precedence Graph (EPG).
    Linearization is consistent if there is a path in the EPG from
    each class to all its successors in the linearization.  See
    the paper for definition of EPG.
 */







/* Raise a TypeError for an MRO order disagreement.

   It's hard to produce a good error message.  In the absence of better
   insight into error reporting, report the classes that were candidates
   to be put next into the MRO.  There is some conflict between the
   order in which they should be put in the MRO, but it's hard to
   diagnose what constraint can't be satisfied.
*/







/*[clinic input]
type.mro

Return a type's method resolution order.
[clinic start generated code]*/





/* Lookups an mcls.mro method, invokes it and checks the result (if needed,
   in case of a custom mro() implementation).

   Keep in mind that during execution of this function type->tp_mro
   can be replaced due to possible reentrance (for example,
   through type_set_bases):

      - when looking up the mcls.mro attribute (it could be
        a user-provided descriptor);

      - from inside a custom mro() itself;

      - through a finalizer of the return value of mro().
*/


/* Calculates and assigns a new MRO to type->tp_mro.
   Return values and invariants:

     - Returns 1 if a new MRO value has been set to type->tp_mro due to
       this call of mro_internal (no tricky reentrancy and no errors).

       In case if p_old_mro argument is not NULL, a previous value
       of type->tp_mro is put there, and the ownership of this
       reference is transferred to a caller.
       Otherwise, the previous value (if any) is decref'ed.

     - Returns 0 in case when type->tp_mro gets changed because of
       reentering here through a custom mro() (see a comment to mro_invoke).

       In this case, a refcount of an old type->tp_mro is adjusted
       somewhere deeper in the call stack (by the innermost mro_internal
       or its caller) and may become zero upon returning from here.
       This also implies that the whole hierarchy of subclasses of the type
       has seen the new value and updated their MRO accordingly.

     - Returns -1 in case of an error.
*/


/* Calculate the best base amongst multiple base classes.
   This is the first one that's on the path to the "solid base". */







static void object_dealloc(PyObject *);
static PyObject *object_new(PyTypeObject *, PyObject *, PyObject *);
static int object_init(PyObject *, PyObject *, PyObject *);
static int update_slot(PyTypeObject *, PyObject *);
static void fixup_slot_dispatchers(PyTypeObject *);
static int type_new_set_names(PyTypeObject *);
static int type_new_init_subclass(PyTypeObject *, PyObject *);

/*
 * Helpers for  __dict__ descriptor.  We don't want to expose the dicts
 * inherited from various builtin types.  The builtin base usually provides
 * its own __dict__ descriptor, so we use that when we can.
 */












/* Three variants on the subtype_getsets list. */

;

;

;












/* Determine the most derived metatype. */



// Forward declaration
static PyObject *
type_new(PyTypeObject *metatype, PyObject *args, PyObject *kwds);

typedef struct {
    PyTypeObject *metatype;
    PyObject *args;
    PyObject *kwds;
    PyObject *orig_dict;
    PyObject *name;
    PyObject *bases;
    PyTypeObject *base;
    PyObject *slots;
    Py_ssize_t nslot;
    int add_dict;
    int add_weak;
    int may_add_dict;
    int may_add_weak;
} type_new_ctx;


/* Check for valid slot names and two special cases */



/* Copy slots into a list, mangle names and sort them.
   Sorted names are needed for __class__ assignment.
   Convert them back to tuple at the end.
*/


















/* Set __module__ in the dict */



/* Set ht_qualname to dict['__qualname__'] if available, else to
   __name__.  The __qualname__ accessor will look for ht_qualname. */



/* Set tp_doc to a copy of dict['__doc__'], if the latter is there
   and is a string.  The __doc__ accessor will first look for tp_doc;
   if that fails, it will still look into __dict__. */









/* Add descriptors for custom slots from __slots__, or for __dict__ */






/* store type in class' cell if one is supplied */
























/* An array of type slot offsets corresponding to Py_tp_* constants,
  * for use in e.g. PyType_Spec and PyType_GetSlot.
  * Each entry has two offsets: "slot_offset" and "subslot_offset".
  * If is subslot_offset is -1, slot_offset is an offset within the
  * PyTypeObject struct.
  * Otherwise slot_offset is an offset to a pointer to a sub-slots struct
  * (such as "tp_as_number"), and subslot_offset is the offset within
  * that struct.
  * The actual table is generated by a script.
  */
;

/* Align up to the nearest multiple of alignof(max_align_t)
 * (like _Py_ALIGN_UP, but for a size rather than pointer)
 */


/* Given a PyType_FromMetaclass `bases` argument (NULL, type, or tuple of
 * types), return a tuple of types.
 */

























/* Get the module of the first superclass where the module has the
 * given PyModuleDef.
 */








/* Internal API to look for a name through the MRO, bypassing the method cache.
   This returns a borrowed reference, and might set an exception.
   'error' is set to: -1: error with exception; 1: error without exception; 0: ok */


/* Check if the "readied" PyUnicode name
   is a double-underscore special name. */


/* Internal API to look for a name through the MRO.
   This returns a borrowed reference, and doesn't set an exception! */




/* This is similar to PyObject_GenericGetAttr(),
   but uses _PyType_Lookup() instead of just looking in type->tp_dict.

   The argument suppress_missing_attribute is used to provide a
   fast path for hasattr. The possible values are:

   * NULL: do not suppress the exception
   * Non-zero pointer: suppress the PyExc_AttributeError and
     set *suppress_missing_attribute to 1 to signal we are returning NULL while
     having suppressed the exception (other exceptions are not suppressed)

   */


/* This is similar to PyObject_GenericGetAttr(),
   but uses _PyType_Lookup() instead of just looking in type->tp_dict. */




extern void
_PyDictKeys_DecRef(PyDictKeysObject *keys);















/*[clinic input]
type.__subclasses__

Return a list of immediate subclasses.
[clinic start generated code]*/






/*
   Merge the __dict__ of aclass into dict, and recursively also all
   the __dict__s of aclass's base classes.  The order of merging isn't
   defined, as it's expected that only the final set of dict keys is
   interesting.
   Return 0 on success, -1 on error.
*/



/* __dir__ for type objects: returns __dict__ and __bases__.
   We deliberately don't suck up its __class__, as methods belonging to the
   metaclass would probably be more confusing than helpful.
*/
/*[clinic input]
type.__dir__

Specialized __dir__ implementation for types.
[clinic start generated code]*/



/*[clinic input]
type.__sizeof__

Return memory consumption of the type object.
[clinic start generated code]*/



;

PyDoc_STRVAR(type_doc,
"type(object) -> the object's type\n"
"type(name, bases, dict, **kwds) -> a new type");








;

;


/* The base type of all types (eventually)... except itself. */

/* You may wonder why object.__new__() only complains about arguments
   when object.__init__() is not overridden, and vice versa.

   Consider the use cases:

   1. When neither is overridden, we want to hear complaints about
      excess (i.e., any) arguments, since their presence could
      indicate there's a bug.

   2. When defining an Immutable type, we are likely to override only
      __new__(), since __init__() is called too late to initialize an
      Immutable object.  Since __new__() defines the signature for the
      type, it would be a pain to have to override __init__() just to
      stop it from complaining about excess arguments.

   3. When defining a Mutable type, we are likely to override only
      __init__().  So here the converse reasoning applies: we don't
      want to have to override __new__() just to stop it from
      complaining.

   4. When __init__() is overridden, and the subclass __init__() calls
      object.__init__(), the latter should complain about excess
      arguments; ditto for __new__().

   Use cases 2 and 3 make it unattractive to unconditionally check for
   excess arguments.  The best solution that addresses all four use
   cases is as follows: __init__() complains about excess arguments
   unless __new__() is overridden and __init__() is not overridden
   (IOW, if __init__() is overridden or __new__() is not overridden);
   symmetrically, __new__() complains about excess arguments unless
   __init__() is overridden and __new__() is not overridden
   (IOW, if __new__() is overridden or __init__() is not overridden).

   However, for backwards compatibility, this breaks too much code.
   Therefore, in 2.6, we'll *warn* about excess arguments when both
   methods are overridden; for all other cases we'll use the above
   rules.

*/

/* Forward */
static PyObject *
object_new(PyTypeObject *type, PyObject *args, PyObject *kwds);



























;


/* Stuff to implement __reduce_ex__ for pickle protocols >= 2.
   We fall back to helpers in copyreg for:
   - pickle protocols < 2
   - calculating the list of slot names (done only once per class)
   - the __newobj__ function (which is used as a token but never called)
*/











/*[clinic input]
object.__getstate__

Helper for pickle.
[clinic start generated code]*/









/*
 * There were two problems when object.__reduce__ and object.__reduce_ex__
 * were implemented in the same function:
 *  - trying to pickle an object with a custom __reduce__ method that
 *    fell back to object.__reduce__ in certain circumstances led to
 *    infinite recursion at Python level and eventual RecursionError.
 *  - Pickling objects that lied about their type by overwriting the
 *    __class__ descriptor could lead to infinite recursion at C level
 *    and eventual segfault.
 *
 * Because of backwards compatibility, the two methods still have to
 * behave in the same way, even if this is not required by the pickle
 * protocol. This common functionality was moved to the _common_reduce
 * function.
 */


/*[clinic input]
object.__reduce__

Helper for pickle.
[clinic start generated code]*/



/*[clinic input]
object.__reduce_ex__

  protocol: int
  /

Helper for pickle.
[clinic start generated code]*/





PyDoc_STRVAR(object_subclasshook_doc,
"Abstract classes can override this to customize issubclass().\n"
"\n"
"This is invoked early on by abc.ABCMeta.__subclasscheck__().\n"
"It should return True, False or NotImplemented.  If it returns\n"
"NotImplemented, the normal algorithm is used.  Otherwise, it\n"
"overrides the normal algorithm (and the outcome is cached).\n");



PyDoc_STRVAR(object_init_subclass_doc,
"This method is called when a class is subclassed.\n"
"\n"
"The default implementation does nothing. It may be\n"
"overridden to extend subclasses.\n");

/*[clinic input]
object.__format__

  format_spec: unicode
  /

Default object formatter.

Return str(self) if format_spec is empty. Raise TypeError otherwise.
[clinic start generated code]*/



/*[clinic input]
object.__sizeof__

Size of object in memory, in bytes.
[clinic start generated code]*/



/* __dir__ for generic objects: returns __dict__, __class__,
   and recursively up the __class__.__bases__ chain.
*/
/*[clinic input]
object.__dir__

Default dir() implementation.
[clinic start generated code]*/



;

PyDoc_STRVAR(object_doc,
"object()\n--\n\n"
"The base class of the class hierarchy.\n\n"
"When called, it accepts no arguments and returns a new featureless\n"
"instance that has no instance attributes and cannot be given any.\n");

;







/* Add the methods from tp_methods to the __dict__ in a type object */















static int add_operators(PyTypeObject *);
static int add_tp_new_wrapper(PyTypeObject *type);

#define COLLECTION_FLAGS (Py_TPFLAGS_SEQUENCE | Py_TPFLAGS_MAPPING)














/* If the type dictionary doesn't contain a __doc__, set it from
   the tp_doc slot. */










// For static types, inherit tp_as_xxx structures from the base class
// if it's NULL.
//
// For heap types, tp_as_xxx structures are not NULL: they are set to the
// PyHeapTypeObject.as_xxx fields by type_new_alloc().







/* Hack for tp_hash and __hash__.
   If after all that, tp_hash is still NULL, and __hash__ is not in
   tp_dict, set tp_hash to PyObject_HashNotImplemented and
   tp_dict['__hash__'] equal to None.
   This signals that __hash__ is not inherited. */



/* Link into each base class's list of subclasses */



// Set tp_new and the "__new__" key in the type dictionary.
// Use the Py_TPFLAGS_DISALLOW_INSTANTIATION flag.


























/* Generic wrappers for overloadable 'operators' such as __getitem__ */

/* There's a wrapper *function* for each distinct function typedef used
   for type object slots (e.g. binaryfunc, ternaryfunc, etc.).  There's a
   wrapper *table* for each distinct operation (e.g. __len__, __add__).
   Most tables have only one entry; the tables for binary operators have two
   entries, one regular and one with reversed arguments. */



























/* XXX objobjproc is a misnomer; should be objargpred */






/* Helper to check for object.__setattr__ or __delattr__ applied to a type.
   This is called the Carlo Verre hack after its discoverer.  See
   https://mail.python.org/pipermail/python-dev/2003-April/034535.html
   */














#undef RICHCMP_WRAPPER
#define RICHCMP_WRAPPER(NAME, OP) \
static PyObject * \
richcmp_##NAME(PyObject *self, PyObject *args, void *wrapped) \
{ \
    return wrap_richcmpfunc(self, args, wrapped, OP); \
}
























;



/* Slot wrappers that call the corresponding __foo__ slot.  See comments
   below at override_slots() for more explanation. */

#define SLOT0(FUNCNAME, DUNDER) \
static PyObject * \
FUNCNAME(PyObject *self) \
{ \
    PyObject* stack[1] = {self}; \
    return vectorcall_method(&_Py_ID(DUNDER), stack, 1); \
}

#define SLOT1(FUNCNAME, DUNDER, ARG1TYPE) \
static PyObject * \
FUNCNAME(PyObject *self, ARG1TYPE arg1) \
{ \
    PyObject* stack[2] = {self, arg1}; \
    return vectorcall_method(&_Py_ID(DUNDER), stack, 2); \
}

/* Boolean helper for SLOT1BINFULL().
   right.__class__ is a nontrivial subclass of left.__class__. */



#define SLOT1BINFULL(FUNCNAME, TESTFUNC, SLOTNAME, DUNDER, RDUNDER) \
static PyObject * \
FUNCNAME(PyObject *self, PyObject *other) \
{ \
    PyObject* stack[2]; \
    PyThreadState *tstate = _PyThreadState_GET(); \
    int do_other = !Py_IS_TYPE(self, Py_TYPE(other)) && \
        Py_TYPE(other)->tp_as_number != NULL && \
        Py_TYPE(other)->tp_as_number->SLOTNAME == TESTFUNC; \
    if (Py_TYPE(self)->tp_as_number != NULL && \
        Py_TYPE(self)->tp_as_number->SLOTNAME == TESTFUNC) { \
        PyObject *r; \
        if (do_other && PyType_IsSubtype(Py_TYPE(other), Py_TYPE(self))) { \
            int ok = method_is_overloaded(self, other, &_Py_ID(RDUNDER)); \
            if (ok < 0) { \
                return NULL; \
            } \
            if (ok) { \
                stack[0] = other; \
                stack[1] = self; \
                r = vectorcall_maybe(tstate, &_Py_ID(RDUNDER), stack, 2); \
                if (r != Py_NotImplemented) \
                    return r; \
                Py_DECREF(r); \
                do_other = 0; \
            } \
        } \
        stack[0] = self; \
        stack[1] = other; \
        r = vectorcall_maybe(tstate, &_Py_ID(DUNDER), stack, 2); \
        if (r != Py_NotImplemented || \
            Py_IS_TYPE(other, Py_TYPE(self))) \
            return r; \
        Py_DECREF(r); \
    } \
    if (do_other) { \
        stack[0] = other; \
        stack[1] = self; \
        return vectorcall_maybe(tstate, &_Py_ID(RDUNDER), stack, 2); \
    } \
    Py_RETURN_NOTIMPLEMENTED; \
}

#define SLOT1BIN(FUNCNAME, SLOTNAME, DUNDER, RDUNDER) \
    SLOT1BINFULL(FUNCNAME, FUNCNAME, SLOTNAME, DUNDER, RDUNDER)









#define slot_mp_length slot_sq_length












static PyObject *slot_nb_power(PyObject *, PyObject *, PyObject *);





























/* Can't use SLOT1 here, because nb_inplace_power is ternary */



















/* There are two slot dispatch functions for tp_getattro.

   - _Py_slot_tp_getattro() is used when __getattribute__ is overridden
     but no __getattr__ hook is present;

   - _Py_slot_tp_getattr_hook() is used when a __getattr__ hook is present.

   The code in update_one_slot() always installs _Py_slot_tp_getattr_hook();
   this detects the absence of __getattr__ and then installs the simpler
   slot if necessary. */

PyObject *
_Py_slot_tp_getattro(PyObject *self, PyObject *name)
{
    PyObject *stack[2] = {self, name};
    return vectorcall_method(&_Py_ID(__getattribute__), stack, 2);
}

static inline PyObject *
call_attribute(PyObject *self, PyObject *attr, PyObject *name)
{
    PyObject *res, *descr = NULL;

    if (_PyType_HasFeature(Py_TYPE(attr), Py_TPFLAGS_METHOD_DESCRIPTOR)) {
        PyObject *args[] = { self, name };
        res = PyObject_Vectorcall(attr, args, 2, NULL);
        return res;
    }

    descrgetfunc f = Py_TYPE(attr)->tp_descr_get;

    if (f != NULL) {
        descr = f(attr, self, (PyObject *)(Py_TYPE(self)));
        if (descr == NULL)
            return NULL;
        else
            attr = descr;
    }
    res = PyObject_CallOneArg(attr, name);
    Py_XDECREF(descr);
    return res;
}

PyObject *
_Py_slot_tp_getattr_hook(PyObject *self, PyObject *name)
{
    PyTypeObject *tp = Py_TYPE(self);
    PyObject *getattr, *getattribute, *res;

    /* speed hack: we could use lookup_maybe, but that would resolve the
       method fully for each attribute lookup for classes with
       __getattr__, even when the attribute is present. So we use
       _PyType_Lookup and create the method only when needed, with
       call_attribute. */
    getattr = _PyType_Lookup(tp, &_Py_ID(__getattr__));
    if (getattr == NULL) {
        /* No __getattr__ hook: use a simpler dispatcher */
        tp->tp_getattro = _Py_slot_tp_getattro;
        return _Py_slot_tp_getattro(self, name);
    }
    Py_INCREF(getattr);
    /* speed hack: we could use lookup_maybe, but that would resolve the
       method fully for each attribute lookup for classes with
       __getattr__, even when self has the default __getattribute__
       method. So we use _PyType_Lookup and create the method only when
       needed, with call_attribute. */
    getattribute = _PyType_Lookup(tp, &_Py_ID(__getattribute__));
    if (getattribute == NULL ||
        (Py_IS_TYPE(getattribute, &PyWrapperDescr_Type) &&
         ((PyWrapperDescrObject *)getattribute)->d_wrapped ==
             (void *)PyObject_GenericGetAttr)) {
        res = _PyObject_GenericGetAttrWithDict(self, name, NULL, 1);
        /* if res == NULL with no exception set, then it must be an
           AttributeError suppressed by us. */
        if (res == NULL && !PyErr_Occurred()) {
            res = call_attribute(self, getattr, name);
        }
    } else {
        Py_INCREF(getattribute);
        res = call_attribute(self, getattribute, name);
        Py_DECREF(getattribute);
        if (res == NULL && PyErr_ExceptionMatches(PyExc_AttributeError)) {
            PyErr_Clear();
            res = call_attribute(self, getattr, name);
        }
    }

    Py_DECREF(getattr);
    return res;
}



;

















typedef struct _PyBufferWrapper {
    PyObject_HEAD
    PyObject *mv;
    PyObject *obj;
} PyBufferWrapper;







;


;







/*
 * bf_releasebuffer is very delicate, because we need to ensure that
 * C bf_releasebuffer slots are called correctly (or we'll leak memory),
 * but we cannot trust any __release_buffer__ implemented in Python to
 * do so correctly. Therefore, if a base class has a C bf_releasebuffer
 * slot, we call it directly here. That is safe because this function
 * only gets called from C callers of the bf_releasebuffer slot. Python
 * code that calls __release_buffer__ directly instead goes through
 * wrap_releasebuffer(), which doesn't call the bf_releasebuffer slot
 * directly but instead simply releases the associated memoryview.
 */








/*
Table mapping __foo__ names to tp_foo offsets and slot_tp_foo wrapper functions.

The table is ordered by offsets relative to the 'PyHeapTypeObject' structure,
which incorporates the additional structures used for numbers, sequences and
mappings.  Note that multiple names may map to the same slot (e.g. __eq__,
__ne__ etc. all map to tp_richcompare) and one name may map to multiple slots
(e.g. __str__ affects tp_str as well as tp_repr). The table is terminated with
an all-zero entry.
*/

#undef TPSLOT
#undef FLSLOT
#undef BUFSLOT
#undef AMSLOT
#undef ETSLOT
#undef SQSLOT
#undef MPSLOT
#undef NBSLOT
#undef UNSLOT
#undef IBSLOT
#undef BINSLOT
#undef RBINSLOT

#define TPSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    {#NAME, offsetof(PyTypeObject, SLOT), (void *)(FUNCTION), WRAPPER, \
     PyDoc_STR(DOC), .name_strobj = &_Py_ID(NAME)}
#define FLSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC, FLAGS) \
    {#NAME, offsetof(PyTypeObject, SLOT), (void *)(FUNCTION), WRAPPER, \
     PyDoc_STR(DOC), FLAGS, .name_strobj = &_Py_ID(NAME) }
#define ETSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    {#NAME, offsetof(PyHeapTypeObject, SLOT), (void *)(FUNCTION), WRAPPER, \
     PyDoc_STR(DOC), .name_strobj = &_Py_ID(NAME) }
#define BUFSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    ETSLOT(NAME, as_buffer.SLOT, FUNCTION, WRAPPER, DOC)
#define AMSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    ETSLOT(NAME, as_async.SLOT, FUNCTION, WRAPPER, DOC)
#define SQSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    ETSLOT(NAME, as_sequence.SLOT, FUNCTION, WRAPPER, DOC)
#define MPSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    ETSLOT(NAME, as_mapping.SLOT, FUNCTION, WRAPPER, DOC)
#define NBSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    ETSLOT(NAME, as_number.SLOT, FUNCTION, WRAPPER, DOC)
#define UNSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    ETSLOT(NAME, as_number.SLOT, FUNCTION, WRAPPER, \
           #NAME "($self, /)\n--\n\n" DOC)
#define IBSLOT(NAME, SLOT, FUNCTION, WRAPPER, DOC) \
    ETSLOT(NAME, as_number.SLOT, FUNCTION, WRAPPER, \
           #NAME "($self, value, /)\n--\n\nReturn self" DOC "value.")
#define BINSLOT(NAME, SLOT, FUNCTION, DOC) \
    ETSLOT(NAME, as_number.SLOT, FUNCTION, wrap_binaryfunc_l, \
           #NAME "($self, value, /)\n--\n\nReturn self" DOC "value.")
#define RBINSLOT(NAME, SLOT, FUNCTION, DOC) \
    ETSLOT(NAME, as_number.SLOT, FUNCTION, wrap_binaryfunc_r, \
           #NAME "($self, value, /)\n--\n\nReturn value" DOC "self.")
#define BINSLOTNOTINFIX(NAME, SLOT, FUNCTION, DOC) \
    ETSLOT(NAME, as_number.SLOT, FUNCTION, wrap_binaryfunc_l, \
           #NAME "($self, value, /)\n--\n\n" DOC)
#define RBINSLOTNOTINFIX(NAME, SLOT, FUNCTION, DOC) \
    ETSLOT(NAME, as_number.SLOT, FUNCTION, wrap_binaryfunc_r, \
           #NAME "($self, value, /)\n--\n\n" DOC)

;

/* Given a type pointer and an offset gotten from a slotdef entry, return a
   pointer to the actual slot.  This is not quite the same as simply adding
   the offset to the type pointer, since it takes care to indirect through the
   proper indirection pointer (as_buffer, etc.); it returns NULL if the
   indirection pointer is NULL. */


/* Return a slot pointer for a given name, but ONLY if the attribute has
   exactly one slot function.  The name must be an interned string. */



/* Common code for update_slots_callback() and fixup_slot_dispatchers().
 *
 * This is meant to set a "slot" like type->tp_repr or
 * type->tp_as_sequence->sq_concat by looking up special methods like
 * __repr__ or __add__. The opposite (adding special methods from slots) is
 * done by add_operators(), called from PyType_Ready(). Since update_one_slot()
 * calls PyType_Ready() if needed, the special methods are already in place.
 *
 * The special methods corresponding to each slot are defined in the "slotdef"
 * array. Note that one slot may correspond to multiple special methods and vice
 * versa. For example, tp_richcompare uses 6 methods __lt__, ..., __ge__ and
 * tp_as_number->nb_add uses __add__ and __radd__. In the other direction,
 * __add__ is used by the number and sequence protocols and __getitem__ by the
 * sequence and mapping protocols. This causes a lot of complications.
 *
 * In detail, update_one_slot() does the following:
 *
 * First of all, if the slot in question does not exist, return immediately.
 * This can happen for example if it's tp_as_number->nb_add but tp_as_number
 * is NULL.
 *
 * For the given slot, we loop over all the special methods with a name
 * corresponding to that slot (for example, for tp_descr_set, this would be
 * __set__ and __delete__) and we look up these names in the MRO of the type.
 * If we don't find any special method, the slot is set to NULL (regardless of
 * what was in the slot before).
 *
 * Suppose that we find exactly one special method. If it's a wrapper_descriptor
 * (i.e. a special method calling a slot, for example str.__repr__ which calls
 * the tp_repr for the 'str' class) with the correct name ("__repr__" for
 * tp_repr), for the right class, calling the right wrapper C function (like
 * wrap_unaryfunc for tp_repr), then the slot is set to the slot that the
 * wrapper_descriptor originally wrapped. For example, a class inheriting
 * from 'str' and not redefining __repr__ will have tp_repr set to the tp_repr
 * of 'str'.
 * In all other cases where the special method exists, the slot is set to a
 * wrapper calling the special method. There is one exception: if the special
 * method is a wrapper_descriptor with the correct name but the type has
 * precisely one slot set for that name and that slot is not the one that we
 * are updating, then NULL is put in the slot (this exception is the only place
 * in update_one_slot() where the *existing* slots matter).
 *
 * When there are multiple special methods for the same slot, the above is
 * applied for each special method. As long as the results agree, the common
 * resulting slot is applied. If the results disagree, then a wrapper for
 * the special methods is installed. This is always safe, but less efficient
 * because it uses method lookup instead of direct C calls.
 *
 * There are some further special cases for specific slots, like supporting
 * __hash__ = None for tp_hash and special code for tp_new.
 *
 * When done, return a pointer to the next slotdef with a different offset,
 * because that's convenient for fixup_slot_dispatchers(). This function never
 * sets an exception: if an internal error happens (unlikely), it's ignored. */


/* In the type, update the slots whose slotdefs are gathered in the pp array.
   This is a callback for update_subclasses(). */


/* Update the slots after assignment to a class (type) attribute. */


/* Store the proper functions in the slot dispatches at class (type)
   definition time, based upon which operations the class overrides in its
   dict. */





/* Call __set_name__ on all attributes (including descriptors)
  in a newly generated type */



/* Call __init_subclass__ on the parent of a newly generated type */



/* recurse_down_subclasses() and update_subclasses() are mutually
   recursive functions to call a callback for all subclasses,
   but refraining from recursing into subclasses that define 'attr_name'. */







/* This function is called by PyType_Ready() to populate the type's
   dictionary with method descriptors for function slots.  For each
   function slot (like tp_repr) that's defined in the type, one or more
   corresponding descriptors are added in the type's tp_dict dictionary
   under the appropriate name (like __repr__).  Some function slots
   cause more than one descriptor to be added (for example, the nb_add
   slot adds both __add__ and __radd__ descriptors) and some function
   slots compete for the same descriptor (for example both sq_item and
   mp_subscript generate a __getitem__ descriptor).

   In the latter case, the first slotdef entry encountered wins.  Since
   slotdef entries are sorted by the offset of the slot in the
   PyHeapTypeObject, this gives us some control over disambiguating
   between competing slots: the members of PyHeapTypeObject are listed
   from most general to least general, so the most general slot is
   preferred.  In particular, because as_mapping comes before as_sequence,
   for a type that defines both mp_subscript and sq_item, mp_subscript
   wins.

   This only adds new descriptors and doesn't overwrite entries in
   tp_dict that were previously defined.  The descriptors contain a
   reference to the C function they must call, so that it's safe if they
   are copied into a subtype's __dict__ and the subtype has a different
   C function in its slot -- calling the method defined by the
   descriptor will call the C function that was used to create it,
   rather than the C function present in the slot when it is called.
   (This is important because a subtype may have a C function in the
   slot that calls the method from the dictionary, and we want to avoid
   infinite recursion here.) */




/* Cooperative 'super' */

typedef struct {
    PyObject_HEAD
    PyTypeObject *type;
    PyObject *obj;
    PyTypeObject *obj_type;
} superobject;

;





/* Do a super lookup without executing descriptors or falling back to getattr
on the super object itself.

May return NULL with or without an exception set, like PyDict_GetItemWithError. */
static PyObject *
_super_lookup_descr(PyTypeObject *su_type, PyTypeObject *su_obj_type, PyObject *name)
{
    PyObject *mro, *res;
    Py_ssize_t i, n;

    mro = lookup_tp_mro(su_obj_type);
    if (mro == NULL)
        return NULL;

    assert(PyTuple_Check(mro));
    n = PyTuple_GET_SIZE(mro);

    /* No need to check the last one: it's gonna be skipped anyway.  */
    for (i = 0; i+1 < n; i++) {
        if ((PyObject *)(su_type) == PyTuple_GET_ITEM(mro, i))
            break;
    }
    i++;  /* skip su->type (if any)  */
    if (i >= n)
        return NULL;

    /* keep a strong reference to mro because su_obj_type->tp_mro can be
       replaced during PyDict_GetItemWithError(dict, name)  */
    Py_INCREF(mro);
    do {
        PyObject *obj = PyTuple_GET_ITEM(mro, i);
        PyObject *dict = lookup_tp_dict(_PyType_CAST(obj));
        assert(dict != NULL && PyDict_Check(dict));

        res = PyDict_GetItemWithError(dict, name);
        if (res != NULL) {
            Py_INCREF(res);
            Py_DECREF(mro);
            return res;
        }
        else if (PyErr_Occurred()) {
            Py_DECREF(mro);
            return NULL;
        }

        i++;
    } while (i < n);
    Py_DECREF(mro);
    return NULL;
}

// if `method` is non-NULL, we are looking for a method descriptor,
// and setting `*method = 1` means we found one.
static PyObject *
do_super_lookup(superobject *su, PyTypeObject *su_type, PyObject *su_obj,
                PyTypeObject *su_obj_type, PyObject *name, int *method)
{
    PyObject *res;
    int temp_su = 0;

    if (su_obj_type == NULL) {
        goto skip;
    }

    res = _super_lookup_descr(su_type, su_obj_type, name);
    if (res != NULL) {
        if (method && _PyType_HasFeature(Py_TYPE(res), Py_TPFLAGS_METHOD_DESCRIPTOR)) {
            *method = 1;
        }
        else {
            descrgetfunc f = Py_TYPE(res)->tp_descr_get;
            if (f != NULL) {
                PyObject *res2;
                res2 = f(res,
                    /* Only pass 'obj' param if this is instance-mode super
                    (See SF ID #743627)  */
                    (su_obj == (PyObject *)su_obj_type) ? NULL : su_obj,
                    (PyObject *)su_obj_type);
                Py_SETREF(res, res2);
            }
        }

        return res;
    }
    else if (PyErr_Occurred()) {
        return NULL;
    }

  skip:
    if (su == NULL) {
        PyObject *args[] = {(PyObject *)su_type, su_obj};
        su = (superobject *)PyObject_Vectorcall((PyObject *)&PySuper_Type, args, 2, NULL);
        if (su == NULL) {
            return NULL;
        }
        temp_su = 1;
    }
    res = PyObject_GenericGetAttr((PyObject *)su, name);
    if (temp_su) {
        Py_DECREF(su);
    }
    return res;
}



static PyTypeObject *
supercheck(PyTypeObject *type, PyObject *obj)
{
    /* Check that a super() call makes sense.  Return a type object.

       obj can be a class, or an instance of one:

       - If it is a class, it must be a subclass of 'type'.      This case is
         used for class methods; the return value is obj.

       - If it is an instance, it must be an instance of 'type'.  This is
         the normal case; the return value is obj.__class__.

       But... when obj is an instance, we want to allow for the case where
       Py_TYPE(obj) is not a subclass of type, but obj.__class__ is!
       This will allow using super() with a proxy for obj.
    */

    /* Check for first bullet above (special case) */
    if (PyType_Check(obj) && PyType_IsSubtype((PyTypeObject *)obj, type)) {
        return (PyTypeObject *)Py_NewRef(obj);
    }

    /* Normal case */
    if (PyType_IsSubtype(Py_TYPE(obj), type)) {
        return (PyTypeObject*)Py_NewRef(Py_TYPE(obj));
    }
    else {
        /* Try the slow way */
        PyObject *class_attr;

        if (_PyObject_LookupAttr(obj, &_Py_ID(__class__), &class_attr) < 0) {
            return NULL;
        }
        if (class_attr != NULL &&
            PyType_Check(class_attr) &&
            (PyTypeObject *)class_attr != Py_TYPE(obj))
        {
            int ok = PyType_IsSubtype(
                (PyTypeObject *)class_attr, type);
            if (ok) {
                return (PyTypeObject *)class_attr;
            }
        }
        Py_XDECREF(class_attr);
    }

    PyErr_SetString(PyExc_TypeError,
                    "super(type, obj): "
                    "obj must be an instance or subtype of type");
    return NULL;
}

PyObject *
_PySuper_Lookup(PyTypeObject *su_type, PyObject *su_obj, PyObject *name, int *method)
{
    PyTypeObject *su_obj_type = supercheck(su_type, su_obj);
    if (su_obj_type == NULL) {
        return NULL;
    }
    PyObject *res = do_super_lookup(NULL, su_type, su_obj, su_obj_type, name, method);
    Py_DECREF(su_obj_type);
    return res;
}





static int super_init_impl(PyObject *self, PyTypeObject *type, PyObject *obj);





PyDoc_STRVAR(super_doc,
"super() -> same as super(__class__, <first argument>)\n"
"super(type) -> unbound super object\n"
"super(type, obj) -> bound super object; requires isinstance(obj, type)\n"
"super(type, type2) -> bound super object; requires issubclass(type2, type)\n"
"Typical use to call a cooperative superclass method:\n"
"class C(B):\n"
"    def meth(self, arg):\n"
"        super().meth(arg)\n"
"This works for class methods too:\n"
"class C(B):\n"
"    @classmethod\n"
"    def cmeth(cls, arg):\n"
"        super().cmeth(arg)\n");





;
