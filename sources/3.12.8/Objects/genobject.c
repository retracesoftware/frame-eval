/* Generator object implementation */

#define _PY_INTERPRETER

#include "Python.h"
#include "pycore_call.h"          // _PyObject_CallNoArgs()
#include "pycore_ceval.h"         // _PyEval_EvalFrame()
#include "pycore_frame.h"         // _PyInterpreterFrame
#include "pycore_genobject.h"     // struct _Py_async_gen_state
#include "pycore_object.h"        // _PyObject_GC_UNTRACK()
#include "pycore_pyerrors.h"      // _PyErr_ClearExcState()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "structmember.h"         // PyMemberDef
#include "opcode.h"               // SEND
#include "frameobject.h"          // _PyInterpreterFrame_GetLine
#include "pystats.h"

static PyObject *gen_close(PyGenObject *, PyObject *);
static PyObject *async_gen_asend_new(PyAsyncGenObject *, PyObject *);
static PyObject *async_gen_athrow_new(PyAsyncGenObject *, PyObject *);

;

;

/* Returns a borrowed reference */
static inline PyCodeObject *
_PyGen_GetCode(PyGenObject *gen) {
    _PyInterpreterFrame *frame = (_PyInterpreterFrame *)(gen->gi_iframe);
    return frame->f_code;
}

















PyDoc_STRVAR(send_doc,
"send(arg) -> send 'arg' into generator,\n\
return next yielded value or raise StopIteration.");



PyDoc_STRVAR(close_doc,
"close() -> raise GeneratorExit inside generator.");

/*
 *   This helper function is used by gen_close and gen_throw to
 *   close a subiterator being delegated to by yield-from.
 */



static inline bool
is_resume(_Py_CODEUNIT *instr)
{
    return instr->op.code == RESUME || instr->op.code == INSTRUMENTED_RESUME;
}



PyObject *
_PyGen_yf(PyGenObject *gen)
{
    PyObject *yf = NULL;

    if (gen->gi_frame_state < FRAME_CLEARED) {
        _PyInterpreterFrame *frame = (_PyInterpreterFrame *)gen->gi_iframe;

        if (gen->gi_frame_state == FRAME_CREATED) {
            /* Return immediately if the frame didn't start yet. SEND
               always come after LOAD_CONST: a code object should not start
               with SEND */
            assert(_PyCode_CODE(_PyGen_GetCode(gen))[0].op.code != SEND);
            return NULL;
        }
        _Py_CODEUNIT next = frame->prev_instr[1];
        if (!is_resume(&next) || next.op.arg < 2)
        {
            /* Not in a yield from */
            return NULL;
        }
        yf = Py_NewRef(_PyFrame_StackPeek(frame));
    }

    return yf;
}




PyDoc_STRVAR(throw_doc,
"throw(value)\n\
throw(type[,value[,tb]])\n\
\n\
Raise exception in generator, return next yielded value or raise\n\
StopIteration.\n\
the (type, val, tb) signature is deprecated, \n\
and may be removed in a future version of Python.");









/*
 * Set StopIteration with specified value.  Value can be arbitrary object
 * or NULL.
 *
 * Returns 0 if StopIteration is set and -1 if any other exception is set.
 */


/*
 *   If StopIteration exception is set, fetches its 'value'
 *   attribute if any, otherwise sets pvalue to None.
 *
 *   Returns 0 if no exception or StopIteration is set.
 *   If any other exception is set, returns -1 and leaves
 *   pvalue unchanged.
 */




























;

;



PyDoc_STRVAR(sizeof__doc__,
"gen.__sizeof__() -> size of gen in memory, in bytes");

;

;


;

static PyObject *
make_gen(PyTypeObject *type, PyFunctionObject *func)
{
    PyCodeObject *code = (PyCodeObject *)func->func_code;
    int slots = _PyFrame_NumSlotsForCodeObject(code);
    PyGenObject *gen = PyObject_GC_NewVar(PyGenObject, type, slots);
    if (gen == NULL) {
        return NULL;
    }
    gen->gi_frame_state = FRAME_CLEARED;
    gen->gi_weakreflist = NULL;
    gen->gi_exc_state.exc_value = NULL;
    gen->gi_exc_state.previous_item = NULL;
    assert(func->func_name != NULL);
    gen->gi_name = Py_NewRef(func->func_name);
    assert(func->func_qualname != NULL);
    gen->gi_qualname = Py_NewRef(func->func_qualname);
    _PyObject_GC_TRACK(gen);
    return (PyObject *)gen;
}

static PyObject *
compute_cr_origin(int origin_depth, _PyInterpreterFrame *current_frame);

PyObject *
_Py_MakeCoro(PyFunctionObject *func)
{
    int coro_flags = ((PyCodeObject *)func->func_code)->co_flags &
        (CO_GENERATOR | CO_COROUTINE | CO_ASYNC_GENERATOR);
    assert(coro_flags);
    if (coro_flags == CO_GENERATOR) {
        return make_gen(&PyGen_Type, func);
    }
    if (coro_flags == CO_ASYNC_GENERATOR) {
        PyAsyncGenObject *o;
        o = (PyAsyncGenObject *)make_gen(&PyAsyncGen_Type, func);
        if (o == NULL) {
            return NULL;
        }
        o->ag_origin_or_finalizer = NULL;
        o->ag_closed = 0;
        o->ag_hooks_inited = 0;
        o->ag_running_async = 0;
        return (PyObject*)o;
    }
    assert (coro_flags == CO_COROUTINE);
    PyObject *coro = make_gen(&PyCoro_Type, func);
    if (!coro) {
        return NULL;
    }
    PyThreadState *tstate = _PyThreadState_GET();
    int origin_depth = tstate->coroutine_origin_tracking_depth;

    if (origin_depth == 0) {
        ((PyCoroObject *)coro)->cr_origin_or_finalizer = NULL;
    } else {
        _PyInterpreterFrame *frame = tstate->cframe->current_frame;
        assert(frame);
        assert(_PyFrame_IsIncomplete(frame));
        frame = _PyFrame_GetFirstComplete(frame->previous);
        PyObject *cr_origin = compute_cr_origin(origin_depth, frame);
        ((PyCoroObject *)coro)->cr_origin_or_finalizer = cr_origin;
        if (!cr_origin) {
            Py_DECREF(coro);
            return NULL;
        }
    }
    return coro;
}







/* Coroutine Object */

typedef struct {
    PyObject_HEAD
    PyCoroObject *cw_coroutine;
} PyCoroWrapper;

static int
gen_is_coroutine(PyObject *o)
{
    if (PyGen_CheckExact(o)) {
        PyCodeObject *code = _PyGen_GetCode((PyGenObject*)o);
        if (code->co_flags & CO_ITERABLE_COROUTINE) {
            return 1;
        }
    }
    return 0;
}

/*
 *   This helper function returns an awaitable for `o`:
 *     - `o` if `o` is a coroutine-object;
 *     - `type(o)->tp_as_async->am_await(o)`
 *
 *   Raises a TypeError if it's not possible to return
 *   an awaitable and returns NULL.
 */
PyObject *
_PyCoro_GetAwaitableIter(PyObject *o)
{
    unaryfunc getter = NULL;
    PyTypeObject *ot;

    if (PyCoro_CheckExact(o) || gen_is_coroutine(o)) {
        /* 'o' is a coroutine. */
        return Py_NewRef(o);
    }

    ot = Py_TYPE(o);
    if (ot->tp_as_async != NULL) {
        getter = ot->tp_as_async->am_await;
    }
    if (getter != NULL) {
        PyObject *res = (*getter)(o);
        if (res != NULL) {
            if (PyCoro_CheckExact(res) || gen_is_coroutine(res)) {
                /* __await__ must return an *iterator*, not
                   a coroutine or another awaitable (see PEP 492) */
                PyErr_SetString(PyExc_TypeError,
                                "__await__() returned a coroutine");
                Py_CLEAR(res);
            } else if (!PyIter_Check(res)) {
                PyErr_Format(PyExc_TypeError,
                             "__await__() returned non-iterator "
                             "of type '%.100s'",
                             Py_TYPE(res)->tp_name);
                Py_CLEAR(res);
            }
        }
        return res;
    }

    PyErr_Format(PyExc_TypeError,
                 "object %.100s can't be used in 'await' expression",
                 ot->tp_name);
    return NULL;
}
















;

;

PyDoc_STRVAR(coro_send_doc,
"send(arg) -> send 'arg' into coroutine,\n\
return next iterated value or raise StopIteration.");

PyDoc_STRVAR(coro_throw_doc,
"throw(value)\n\
throw(type[,value[,traceback]])\n\
\n\
Raise exception in coroutine, return next iterated value or raise\n\
StopIteration.\n\
the (type, val, tb) signature is deprecated, \n\
and may be removed in a future version of Python.");


PyDoc_STRVAR(coro_close_doc,
"close() -> raise GeneratorExit inside coroutine.");

;

;

;













;

;

static PyObject *
compute_cr_origin(int origin_depth, _PyInterpreterFrame *current_frame)
{
    _PyInterpreterFrame *frame = current_frame;
    /* First count how many frames we have */
    int frame_count = 0;
    for (; frame && frame_count < origin_depth; ++frame_count) {
        frame = _PyFrame_GetFirstComplete(frame->previous);
    }

    /* Now collect them */
    PyObject *cr_origin = PyTuple_New(frame_count);
    if (cr_origin == NULL) {
        return NULL;
    }
    frame = current_frame;
    for (int i = 0; i < frame_count; ++i) {
        PyCodeObject *code = frame->f_code;
        int line = PyUnstable_InterpreterFrame_GetLine(frame);
        PyObject *frameinfo = Py_BuildValue("OiO", code->co_filename, line,
                                            code->co_name);
        if (!frameinfo) {
            Py_DECREF(cr_origin);
            return NULL;
        }
        PyTuple_SET_ITEM(cr_origin, i, frameinfo);
        frame = _PyFrame_GetFirstComplete(frame->previous);
    }

    return cr_origin;
}




/* ========= Asynchronous Generators ========= */


typedef enum {
    AWAITABLE_STATE_INIT,   /* new awaitable, has not yet been iterated */
    AWAITABLE_STATE_ITER,   /* being iterated */
    AWAITABLE_STATE_CLOSED, /* closed */
} AwaitableState;


typedef struct PyAsyncGenASend {
    PyObject_HEAD
    PyAsyncGenObject *ags_gen;

    /* Can be NULL, when in the __anext__() mode
       (equivalent of "asend(None)") */
    PyObject *ags_sendval;

    AwaitableState ags_state;
} PyAsyncGenASend;


typedef struct PyAsyncGenAThrow {
    PyObject_HEAD
    PyAsyncGenObject *agt_gen;

    /* Can be NULL, when in the "aclose()" mode
       (equivalent of "athrow(GeneratorExit)") */
    PyObject *agt_args;

    AwaitableState agt_state;
} PyAsyncGenAThrow;


typedef struct _PyAsyncGenWrappedValue {
    PyObject_HEAD
    PyObject *agw_val;
} _PyAsyncGenWrappedValue;


#define _PyAsyncGenWrappedValue_CheckExact(o) \
                    Py_IS_TYPE(o, &_PyAsyncGenWrappedValue_Type)



























;

;

PyDoc_STRVAR(async_aclose_doc,
"aclose() -> raise GeneratorExit inside generator.");

PyDoc_STRVAR(async_asend_doc,
"asend(v) -> send 'v' in generator.");

PyDoc_STRVAR(async_athrow_doc,
"athrow(value)\n\
athrow(type[,value[,tb]])\n\
\n\
raise exception in generator.\n\
the (type, val, tb) signature is deprecated, \n\
and may be removed in a future version of Python.");

;


;


;


#if _PyAsyncGen_MAXFREELIST > 0

#endif





void
_PyAsyncGen_ClearFreeLists(PyInterpreterState *interp)
{
#if _PyAsyncGen_MAXFREELIST > 0
    struct _Py_async_gen_state *state = &interp->async_gen;

    while (state->value_numfree) {
        _PyAsyncGenWrappedValue *o;
        o = state->value_freelist[--state->value_numfree];
        assert(_PyAsyncGenWrappedValue_CheckExact(o));
        PyObject_GC_Del(o);
    }

    while (state->asend_numfree) {
        PyAsyncGenASend *o;
        o = state->asend_freelist[--state->asend_numfree];
        assert(Py_IS_TYPE(o, &_PyAsyncGenASend_Type));
        PyObject_GC_Del(o);
    }
#endif
}







/* ---------- Async Generator ASend Awaitable ------------ */



















;


;


;





/* ---------- Async Generator Value Wrapper ------------ */








;


PyObject *
_PyAsyncGenValueWrapperNew(PyThreadState *tstate, PyObject *val)
{
    _PyAsyncGenWrappedValue *o;
    assert(val);

#if _PyAsyncGen_MAXFREELIST > 0
    struct _Py_async_gen_state *state = &tstate->interp->async_gen;
#ifdef Py_DEBUG
    // _PyAsyncGenValueWrapperNew() must not be called after _PyAsyncGen_Fini()
    assert(state->value_numfree != -1);
#endif
    if (state->value_numfree) {
        state->value_numfree--;
        o = state->value_freelist[state->value_numfree];
        OBJECT_STAT_INC(from_freelist);
        assert(_PyAsyncGenWrappedValue_CheckExact(o));
        _Py_NewReference((PyObject*)o);
    }
    else
#endif
    {
        o = PyObject_GC_New(_PyAsyncGenWrappedValue,
                            &_PyAsyncGenWrappedValue_Type);
        if (o == NULL) {
            return NULL;
        }
    }
    o->agw_val = Py_NewRef(val);
    _PyObject_GC_TRACK((PyObject*)o);
    return (PyObject*)o;
}


/* ---------- Async Generator AThrow awaitable ------------ */




















;


;


;



