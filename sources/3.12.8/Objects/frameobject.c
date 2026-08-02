/* Frame object implementation */

#include "Python.h"
#include "pycore_ceval.h"         // _PyEval_BuiltinsFromGlobals()
#include "pycore_code.h"          // CO_FAST_LOCAL, etc.
#include "pycore_function.h"      // _PyFunction_FromConstructor()
#include "pycore_moduleobject.h"  // _PyModule_GetDict()
#include "pycore_object.h"        // _PyObject_GC_UNTRACK()
#include "pycore_opcode.h"        // _PyOpcode_Caches

#include "frameobject.h"          // PyFrameObject
#include "pycore_frame.h"
#include "opcode.h"               // EXTENDED_ARG
#include "structmember.h"         // PyMemberDef

#define OFF(x) offsetof(PyFrameObject, x)

;






















/* Model the evaluation stack, to determine which jumps
 * are safe and how many values needs to be popped.
 * The stack is modelled by a 64 integer, treating any
 * stack that can't fit into 64 bits as "overflowed".
 */

typedef enum kind {
    Iterator = 1,
    Except = 2,
    Object = 3,
    Null = 4,
    Lasti = 5,
} Kind;



#define BITS_PER_BLOCK 3

#define UNINITIALIZED -2
#define OVERFLOWED -1

#define MAX_STACK_ENTRIES (63/BITS_PER_BLOCK)
#define WILL_OVERFLOW (1ULL<<((MAX_STACK_ENTRIES-1)*BITS_PER_BLOCK))

#define EMPTY_STACK 0





#define MASK ((1<<BITS_PER_BLOCK)-1)









#if 0
/* These functions are useful for debugging the stack marking code */

static char
tos_char(int64_t stack) {
    switch(top_of_stack(stack)) {
        case Iterator:
            return 'I';
        case Except:
            return 'E';
        case Object:
            return 'O';
        case Lasti:
            return 'L';
        case Null:
            return 'N';
    }
    return '?';
}

static void
print_stack(int64_t stack) {
    if (stack < 0) {
        if (stack == UNINITIALIZED) {
            printf("---");
        }
        else if (stack == OVERFLOWED) {
            printf("OVERFLOWED");
        }
        else {
            printf("??");
        }
        return;
    }
    while (stack) {
        printf("%c", tos_char(stack));
        stack = pop_value(stack);
    }
}

static void
print_stacks(int64_t *stacks, int n) {
    for (int i = 0; i < n; i++) {
        printf("%d: ", i);
        print_stack(stacks[i]);
        printf("\n");
    }
}

#endif













/* Setter for f_lineno - you can set f_lineno from within a trace function in
 * order to jump to a given line of code, subject to some restrictions.  Most
 * lines are OK to jump to because they don't make any assumptions about the
 * state of the stack (obvious because you could remove the line and the code
 * would still work without any stack errors), but there are some constructs
 * that limit jumping:
 *
 *  o Any exception handlers.
 *  o 'for' and 'async for' loops can't be jumped into because the
 *    iterator needs to be on the stack.
 *  o Jumps cannot be made from within a trace function invoked with a
 *    'return' or 'exception' event since the eval loop has been exited at
 *    that time.
 */







;









PyDoc_STRVAR(clear__doc__,
"F.clear(): clear most references held by the frame");



PyDoc_STRVAR(sizeof__doc__,
"F.__sizeof__() -> size of F in memory, in bytes");



;

;



PyFrameObject*
_PyFrame_New_NoTrack(PyCodeObject *code)
{
    CALL_STAT_INC(frame_objects_created);
    int slots = code->co_nlocalsplus + code->co_stacksize;
    PyFrameObject *f = PyObject_GC_NewVar(PyFrameObject, &PyFrame_Type, slots);
    if (f == NULL) {
        return NULL;
    }
    f->f_back = NULL;
    f->f_trace = NULL;
    f->f_trace_lines = 1;
    f->f_trace_opcodes = 0;
    f->f_fast_as_locals = 0;
    f->f_lineno = 0;
    return f;
}

/* Legacy API */


static int
_PyFrame_OpAlreadyRan(_PyInterpreterFrame *frame, int opcode, int oparg)
{
    // This only works when opcode is a non-quickened form:
    assert(_PyOpcode_Deopt[opcode] == opcode);
    int check_oparg = 0;
    for (_Py_CODEUNIT *instruction = _PyCode_CODE(frame->f_code);
         instruction < frame->prev_instr; instruction++)
    {
        int check_opcode = _PyOpcode_Deopt[instruction->op.code];
        check_oparg |= instruction->op.arg;
        if (check_opcode == opcode && check_oparg == oparg) {
            return 1;
        }
        if (check_opcode == EXTENDED_ARG) {
            check_oparg <<= 8;
        }
        else {
            check_oparg = 0;
        }
        instruction += _PyOpcode_Caches[check_opcode];
    }
    return 0;
}


// Initialize frame free variables if needed
static void
frame_init_get_vars(_PyInterpreterFrame *frame)
{
    // COPY_FREE_VARS has no quickened forms, so no need to use _PyOpcode_Deopt
    // here:
    PyCodeObject *co = frame->f_code;
    int lasti = _PyInterpreterFrame_LASTI(frame);
    if (!(lasti < 0 && _PyCode_CODE(co)->op.code == COPY_FREE_VARS
          && PyFunction_Check(frame->f_funcobj)))
    {
        /* Free vars are initialized */
        return;
    }

    /* Free vars have not been initialized -- Do that */
    PyObject *closure = ((PyFunctionObject *)frame->f_funcobj)->func_closure;
    int offset = PyCode_GetFirstFree(co);
    for (int i = 0; i < co->co_nfreevars; ++i) {
        PyObject *o = PyTuple_GET_ITEM(closure, i);
        frame->localsplus[offset + i] = Py_NewRef(o);
    }
    // COPY_FREE_VARS doesn't have inline CACHEs, either:
    frame->prev_instr = _PyCode_CODE(frame->f_code);
}


static int
frame_get_var(_PyInterpreterFrame *frame, PyCodeObject *co, int i,
              PyObject **pvalue)
{
    _PyLocals_Kind kind = _PyLocals_GetKind(co->co_localspluskinds, i);

    /* If the namespace is unoptimized, then one of the
       following cases applies:
       1. It does not contain free variables, because it
          uses import * or is a top-level namespace.
       2. It is a class namespace.
       We don't want to accidentally copy free variables
       into the locals dict used by the class.
    */
    if (kind & CO_FAST_FREE && !(co->co_flags & CO_OPTIMIZED)) {
        return 0;
    }

    PyObject *value = frame->localsplus[i];
    if (frame->stacktop) {
        if (kind & CO_FAST_FREE) {
            // The cell was set by COPY_FREE_VARS.
            assert(value != NULL && PyCell_Check(value));
            value = PyCell_GET(value);
        }
        else if (kind & CO_FAST_CELL) {
            // Note that no *_DEREF ops can happen before MAKE_CELL
            // executes.  So there's no need to duplicate the work
            // that MAKE_CELL would otherwise do later, if it hasn't
            // run yet.
            if (value != NULL) {
                if (PyCell_Check(value) &&
                        _PyFrame_OpAlreadyRan(frame, MAKE_CELL, i)) {
                    // (likely) MAKE_CELL must have executed already.
                    value = PyCell_GET(value);
                }
                // (likely) Otherwise it it is an arg (kind & CO_FAST_LOCAL),
                // with the initial value set when the frame was created...
                // (unlikely) ...or it was set to some initial value by
                // an earlier call to PyFrame_LocalsToFast().
            }
        }
    }
    else {
        assert(value == NULL);
    }
    *pvalue = value;
    return 1;
}


PyObject *
_PyFrame_GetLocals(_PyInterpreterFrame *frame, int include_hidden)
{
    /* Merge fast locals into f->f_locals */
    PyObject *locals = frame->f_locals;
    if (locals == NULL) {
        locals = frame->f_locals = PyDict_New();
        if (locals == NULL) {
            return NULL;
        }
    }
    PyObject *hidden = NULL;

    /* If include_hidden, "hidden" fast locals (from inlined comprehensions in
       module/class scopes) will be included in the returned dict, but not in
       frame->f_locals; the returned dict will be a modified copy. Non-hidden
       locals will still be updated in frame->f_locals. */
    if (include_hidden) {
        hidden = PyDict_New();
        if (hidden == NULL) {
            return NULL;
        }
    }

    frame_init_get_vars(frame);

    PyCodeObject *co = frame->f_code;
    for (int i = 0; i < co->co_nlocalsplus; i++) {
        PyObject *value;  // borrowed reference
        if (!frame_get_var(frame, co, i, &value)) {
            continue;
        }

        PyObject *name = PyTuple_GET_ITEM(co->co_localsplusnames, i);
        _PyLocals_Kind kind = _PyLocals_GetKind(co->co_localspluskinds, i);
        if (kind & CO_FAST_HIDDEN) {
            if (include_hidden && value != NULL) {
                if (PyObject_SetItem(hidden, name, value) != 0) {
                    goto error;
                }
            }
            continue;
        }
        if (value == NULL) {
            if (PyObject_DelItem(locals, name) != 0) {
                if (PyErr_ExceptionMatches(PyExc_KeyError)) {
                    PyErr_Clear();
                }
                else {
                    goto error;
                }
            }
        }
        else {
            if (PyObject_SetItem(locals, name, value) != 0) {
                goto error;
            }
        }
    }

    if (include_hidden && PyDict_Size(hidden)) {
        PyObject *innerlocals = PyDict_New();
        if (innerlocals == NULL) {
            goto error;
        }
        if (PyDict_Merge(innerlocals, locals, 1) != 0) {
            Py_DECREF(innerlocals);
            goto error;
        }
        if (PyDict_Merge(innerlocals, hidden, 1) != 0) {
            Py_DECREF(innerlocals);
            goto error;
        }
        locals = innerlocals;
    }
    else {
        Py_INCREF(locals);
    }
    Py_CLEAR(hidden);

    return locals;

  error:
    Py_XDECREF(hidden);
    return NULL;
}


int
_PyFrame_FastToLocalsWithError(_PyInterpreterFrame *frame)
{
    PyObject *locals = _PyFrame_GetLocals(frame, 0);
    if (locals == NULL) {
        return -1;
    }
    Py_DECREF(locals);
    return 0;
}












void
_PyFrame_LocalsToFast(_PyInterpreterFrame *frame, int clear)
{
    /* Merge locals into fast locals */
    PyObject *locals;
    PyObject **fast;
    PyCodeObject *co;
    locals = frame->f_locals;
    if (locals == NULL) {
        return;
    }
    fast = _PyFrame_GetLocalsArray(frame);
    co = frame->f_code;

    PyObject *exc = PyErr_GetRaisedException();
    for (int i = 0; i < co->co_nlocalsplus; i++) {
        _PyLocals_Kind kind = _PyLocals_GetKind(co->co_localspluskinds, i);

        /* Same test as in PyFrame_FastToLocals() above. */
        if (kind & CO_FAST_FREE && !(co->co_flags & CO_OPTIMIZED)) {
            continue;
        }
        PyObject *name = PyTuple_GET_ITEM(co->co_localsplusnames, i);
        PyObject *value = PyObject_GetItem(locals, name);
        /* We only care about NULLs if clear is true. */
        if (value == NULL) {
            PyErr_Clear();
            if (!clear) {
                continue;
            }
        }
        PyObject *oldvalue = fast[i];
        PyObject *cell = NULL;
        if (kind == CO_FAST_FREE) {
            // The cell was set when the frame was created from
            // the function's closure.
            assert(oldvalue != NULL && PyCell_Check(oldvalue));
            cell = oldvalue;
        }
        else if (kind & CO_FAST_CELL && oldvalue != NULL) {
            /* Same test as in PyFrame_FastToLocals() above. */
            if (PyCell_Check(oldvalue) &&
                    _PyFrame_OpAlreadyRan(frame, MAKE_CELL, i)) {
                // (likely) MAKE_CELL must have executed already.
                cell = oldvalue;
            }
            // (unlikely) Otherwise, it must have been set to some
            // initial value by an earlier call to PyFrame_LocalsToFast().
        }
        if (cell != NULL) {
            oldvalue = PyCell_GET(cell);
            if (value != oldvalue) {
                PyCell_SET(cell, Py_XNewRef(value));
                Py_XDECREF(oldvalue);
            }
        }
        else if (value != oldvalue) {
            if (value == NULL) {
                // Probably can't delete this, since the compiler's flow
                // analysis may have already "proven" that it exists here:
                const char *e = "assigning None to unbound local %R";
                if (PyErr_WarnFormat(PyExc_RuntimeWarning, 0, e, name)) {
                    // It's okay if frame_obj is NULL, just try anyways:
                    PyErr_WriteUnraisable((PyObject *)frame->frame_obj);
                }
                value = Py_NewRef(Py_None);
            }
            Py_XSETREF(fast[i], Py_NewRef(value));
        }
        Py_XDECREF(value);
    }
    PyErr_SetRaisedException(exc);
}





















