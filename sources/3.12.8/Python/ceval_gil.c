
#include "Python.h"
#include "pycore_atomic.h"        // _Py_atomic_int
#include "pycore_ceval.h"         // _PyEval_SignalReceived()
#include "pycore_pyerrors.h"      // _PyErr_GetRaisedException()
#include "pycore_pylifecycle.h"   // _PyErr_Print()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_interp.h"        // _Py_RunGC()
#include "pycore_pymem.h"         // _PyMem_IsPtrFreed()

/*
   Notes about the implementation:

   - The GIL is just a boolean variable (locked) whose access is protected
     by a mutex (gil_mutex), and whose changes are signalled by a condition
     variable (gil_cond). gil_mutex is taken for short periods of time,
     and therefore mostly uncontended.

   - In the GIL-holding thread, the main loop (PyEval_EvalFrameEx) must be
     able to release the GIL on demand by another thread. A volatile boolean
     variable (gil_drop_request) is used for that purpose, which is checked
     at every turn of the eval loop. That variable is set after a wait of
     `interval` microseconds on `gil_cond` has timed out.

      [Actually, another volatile boolean variable (eval_breaker) is used
       which ORs several conditions into one. Volatile booleans are
       sufficient as inter-thread signalling means since Python is run
       on cache-coherent architectures only.]

   - A thread wanting to take the GIL will first let pass a given amount of
     time (`interval` microseconds) before setting gil_drop_request. This
     encourages a defined switching period, but doesn't enforce it since
     opcodes can take an arbitrary time to execute.

     The `interval` value is available for the user to read and modify
     using the Python API `sys.{get,set}switchinterval()`.

   - When a thread releases the GIL and gil_drop_request is set, that thread
     ensures that another GIL-awaiting thread gets scheduled.
     It does so by waiting on a condition variable (switch_cond) until
     the value of last_holder is changed to something else than its
     own thread state pointer, indicating that another thread was able to
     take the GIL.

     This is meant to prohibit the latency-adverse behaviour on multi-core
     machines where one thread would speculatively release the GIL, but still
     run and end up being the first to re-acquire it, making the "timeslices"
     much longer than expected.
     (Note: this mechanism is enabled with FORCE_SWITCHING above)
*/

// GH-89279: Force inlining by using a macro.
#if defined(_MSC_VER) && SIZEOF_INT == 4
#define _Py_atomic_load_relaxed_int32(ATOMIC_VAL) (assert(sizeof((ATOMIC_VAL)->_value) == 4), *((volatile int*)&((ATOMIC_VAL)->_value)))
#else
#define _Py_atomic_load_relaxed_int32(ATOMIC_VAL) _Py_atomic_load_relaxed(ATOMIC_VAL)
#endif

/* This can set eval_breaker to 0 even though gil_drop_request became
   1.  We believe this is all right because the eval loop will release
   the GIL eventually anyway. */
static inline void
COMPUTE_EVAL_BREAKER(PyInterpreterState *interp,
                     struct _ceval_runtime_state *ceval,
                     struct _ceval_state *ceval2)
{
    _Py_atomic_store_relaxed(&ceval2->eval_breaker,
        _Py_atomic_load_relaxed_int32(&ceval2->gil_drop_request)
        | (_Py_atomic_load_relaxed_int32(&ceval->signals_pending)
           && _Py_ThreadCanHandleSignals(interp))
        | (_Py_atomic_load_relaxed_int32(&ceval2->pending.calls_to_do))
        | (_Py_IsMainThread() && _Py_IsMainInterpreter(interp)
           &&_Py_atomic_load_relaxed_int32(&ceval->pending_mainthread.calls_to_do))
        | ceval2->pending.async_exc
        | _Py_atomic_load_relaxed_int32(&ceval2->gc_scheduled));
}








static inline void
SIGNAL_PENDING_CALLS(struct _pending_calls *pending, PyInterpreterState *interp)
{
    struct _ceval_runtime_state *ceval = &interp->runtime->ceval;
    struct _ceval_state *ceval2 = &interp->ceval;
    _Py_atomic_store_relaxed(&pending->calls_to_do, 1);
    COMPUTE_EVAL_BREAKER(interp, ceval, ceval2);
}


static inline void
UNSIGNAL_PENDING_CALLS(PyInterpreterState *interp)
{
    struct _ceval_runtime_state *ceval = &interp->runtime->ceval;
    struct _ceval_state *ceval2 = &interp->ceval;
    if (_Py_IsMainThread() && _Py_IsMainInterpreter(interp)) {
        _Py_atomic_store_relaxed(&ceval->pending_mainthread.calls_to_do, 0);
    }
    _Py_atomic_store_relaxed(&ceval2->pending.calls_to_do, 0);
    COMPUTE_EVAL_BREAKER(interp, ceval, ceval2);
}


static inline void
SIGNAL_PENDING_SIGNALS(PyInterpreterState *interp, int force)
{
    struct _ceval_runtime_state *ceval = &interp->runtime->ceval;
    struct _ceval_state *ceval2 = &interp->ceval;
    _Py_atomic_store_relaxed(&ceval->signals_pending, 1);
    if (force) {
        _Py_atomic_store_relaxed(&ceval2->eval_breaker, 1);
    }
    else {
        /* eval_breaker is not set to 1 if thread_can_handle_signals() is false */
        COMPUTE_EVAL_BREAKER(interp, ceval, ceval2);
    }
}


static inline void
UNSIGNAL_PENDING_SIGNALS(PyInterpreterState *interp)
{
    struct _ceval_runtime_state *ceval = &interp->runtime->ceval;
    struct _ceval_state *ceval2 = &interp->ceval;
    _Py_atomic_store_relaxed(&ceval->signals_pending, 0);
    COMPUTE_EVAL_BREAKER(interp, ceval, ceval2);
}





static inline void
UNSIGNAL_ASYNC_EXC(PyInterpreterState *interp)
{
    struct _ceval_runtime_state *ceval = &interp->runtime->ceval;
    struct _ceval_state *ceval2 = &interp->ceval;
    ceval2->pending.async_exc = 0;
    COMPUTE_EVAL_BREAKER(interp, ceval, ceval2);
}


/*
 * Implementation of the Global Interpreter Lock (GIL).
 */

#include <stdlib.h>
#include <errno.h>

#include "pycore_atomic.h"


#include "condvar.h"

#define MUTEX_INIT(mut) \
    if (PyMUTEX_INIT(&(mut))) { \
        Py_FatalError("PyMUTEX_INIT(" #mut ") failed"); };
#define MUTEX_FINI(mut) \
    if (PyMUTEX_FINI(&(mut))) { \
        Py_FatalError("PyMUTEX_FINI(" #mut ") failed"); };
#define MUTEX_LOCK(mut) \
    if (PyMUTEX_LOCK(&(mut))) { \
        Py_FatalError("PyMUTEX_LOCK(" #mut ") failed"); };
#define MUTEX_UNLOCK(mut) \
    if (PyMUTEX_UNLOCK(&(mut))) { \
        Py_FatalError("PyMUTEX_UNLOCK(" #mut ") failed"); };

#define COND_INIT(cond) \
    if (PyCOND_INIT(&(cond))) { \
        Py_FatalError("PyCOND_INIT(" #cond ") failed"); };
#define COND_FINI(cond) \
    if (PyCOND_FINI(&(cond))) { \
        Py_FatalError("PyCOND_FINI(" #cond ") failed"); };
#define COND_SIGNAL(cond) \
    if (PyCOND_SIGNAL(&(cond))) { \
        Py_FatalError("PyCOND_SIGNAL(" #cond ") failed"); };
#define COND_WAIT(cond, mut) \
    if (PyCOND_WAIT(&(cond), &(mut))) { \
        Py_FatalError("PyCOND_WAIT(" #cond ") failed"); };
#define COND_TIMED_WAIT(cond, mut, microseconds, timeout_result) \
    { \
        int r = PyCOND_TIMEDWAIT(&(cond), &(mut), (microseconds)); \
        if (r < 0) \
            Py_FatalError("PyCOND_WAIT(" #cond ") failed"); \
        if (r) /* 1 == timeout, 2 == impl. can't say, so assume timeout */ \
            timeout_result = 1; \
        else \
            timeout_result = 0; \
    } \


#define DEFAULT_INTERVAL 5000









#ifdef HAVE_FORK

#endif




/* Take the GIL.

   The function saves errno at entry and restores its value at exit.

   tstate must be non-NULL. */




































#ifdef HAVE_FORK
/* This function is called from PyOS_AfterFork_Child to destroy all threads
   which are not running in the child process, and clear internal locks
   which might be held by those threads. */

#endif

/* This function is used to signal that async exceptions are waiting to be
   raised. */








/* Mechanism whereby asynchronously executing callbacks (e.g. UNIX
   signal handlers or Mac I/O completion routines) can schedule calls
   to a function to be called synchronously.
   The synchronous function is called with one void* argument.
   It should return 0 for success or -1 for failure -- failure should
   be accompanied by an exception.

   If registry succeeds, the registry function returns 0; if it fails
   (e.g. due to too many pending calls) it returns -1 (without setting
   an exception condition).

   Note that because registry may occur from within signal handlers,
   or other asynchronous events, calling malloc() is unsafe!

   Any thread can schedule pending calls, but only the main thread
   will execute them.
   There is no facility to schedule calls to a particular thread, but
   that should be easy to change, should that ever be required.  In
   that case, the static variables here should go into the python
   threadstate.
*/



/* Push one item onto the queue while holding the lock. */


static int
_next_pending_call(struct _pending_calls *pending,
                   int (**func)(void *), void **arg)
{
    int i = pending->first;
    if (i == pending->last) {
        /* Queue empty */
        assert(pending->calls[i].func == NULL);
        return -1;
    }
    *func = pending->calls[i].func;
    *arg = pending->calls[i].arg;
    return i;
}

/* Pop one item off the queue while holding the lock. */
static void
_pop_pending_call(struct _pending_calls *pending,
                  int (**func)(void *), void **arg)
{
    int i = _next_pending_call(pending, func, arg);
    if (i >= 0) {
        pending->calls[i] = (struct _pending_call){0};
        pending->first = (i + 1) % NPENDINGCALLS;
    }
}

/* This implementation is thread-safe.  It allows
   scheduling to be made from any thread, and even from an executing
   callback.
 */





static int
handle_signals(PyThreadState *tstate)
{
    assert(_PyThreadState_CheckConsistency(tstate));
    if (!_Py_ThreadCanHandleSignals(tstate->interp)) {
        return 0;
    }

    UNSIGNAL_PENDING_SIGNALS(tstate->interp);
    if (_PyErr_CheckSignalsTstate(tstate) < 0) {
        /* On failure, re-schedule a call to handle_signals(). */
        SIGNAL_PENDING_SIGNALS(tstate->interp, 0);
        return -1;
    }
    return 0;
}

static inline int
maybe_has_pending_calls(PyInterpreterState *interp)
{
    struct _pending_calls *pending = &interp->ceval.pending;
    if (_Py_atomic_load_relaxed_int32(&pending->calls_to_do)) {
        return 1;
    }
    if (!_Py_IsMainThread() || !_Py_IsMainInterpreter(interp)) {
        return 0;
    }
    pending = &_PyRuntime.ceval.pending_mainthread;
    return _Py_atomic_load_relaxed_int32(&pending->calls_to_do);
}

static int
_make_pending_calls(struct _pending_calls *pending)
{
    /* perform a bounded number of calls, in case of recursion */
    for (int i=0; i<NPENDINGCALLS; i++) {
        int (*func)(void *) = NULL;
        void *arg = NULL;

        /* pop one item off the queue while holding the lock */
        PyThread_acquire_lock(pending->lock, WAIT_LOCK);
        _pop_pending_call(pending, &func, &arg);
        PyThread_release_lock(pending->lock);

        /* having released the lock, perform the callback */
        if (func == NULL) {
            break;
        }
        if (func(arg) != 0) {
            return -1;
        }
    }
    return 0;
}

static int
make_pending_calls(PyInterpreterState *interp)
{
    struct _pending_calls *pending = &interp->ceval.pending;
    struct _pending_calls *pending_main = &_PyRuntime.ceval.pending_mainthread;

    /* Only one thread (per interpreter) may run the pending calls
       at once.  In the same way, we don't do recursive pending calls. */
    PyThread_acquire_lock(pending->lock, WAIT_LOCK);
    if (pending->busy) {
        /* A pending call was added after another thread was already
           handling the pending calls (and had already "unsignaled").
           Once that thread is done, it may have taken care of all the
           pending calls, or there might be some still waiting.
           Regardless, this interpreter's pending calls will stay
           "signaled" until that first thread has finished.  At that
           point the next thread to trip the eval breaker will take
           care of any remaining pending calls.  Until then, though,
           all the interpreter's threads will be tripping the eval
           breaker every time it's checked. */
        PyThread_release_lock(pending->lock);
        return 0;
    }
    pending->busy = 1;
    PyThread_release_lock(pending->lock);

    /* unsignal before starting to call callbacks, so that any callback
       added in-between re-signals */
    UNSIGNAL_PENDING_CALLS(interp);

    if (_make_pending_calls(pending) != 0) {
        pending->busy = 0;
        /* There might not be more calls to make, but we play it safe. */
        SIGNAL_PENDING_CALLS(pending, interp);
        return -1;
    }

    if (_Py_IsMainThread() && _Py_IsMainInterpreter(interp)) {
        if (_make_pending_calls(pending_main) != 0) {
            pending->busy = 0;
            /* There might not be more calls to make, but we play it safe. */
            SIGNAL_PENDING_CALLS(pending_main, interp);
            return -1;
        }
    }

    pending->busy = 0;
    return 0;
}





/* Py_MakePendingCalls() is a simple wrapper for the sake
   of backward-compatibility. */






/* Handle signals, pending calls, GIL drop request
   and asynchronous exception */
int
_Py_HandlePending(PyThreadState *tstate)
{
    _PyRuntimeState * const runtime = &_PyRuntime;
    struct _ceval_runtime_state *ceval = &runtime->ceval;
    struct _ceval_state *interp_ceval_state = &tstate->interp->ceval;

    /* Pending signals */
    if (_Py_atomic_load_relaxed_int32(&ceval->signals_pending)) {
        if (handle_signals(tstate) != 0) {
            return -1;
        }
    }

    /* Pending calls */
    if (maybe_has_pending_calls(tstate->interp)) {
        if (make_pending_calls(tstate->interp) != 0) {
            return -1;
        }
    }

    /* GC scheduled to run */
    if (_Py_atomic_load_relaxed_int32(&interp_ceval_state->gc_scheduled)) {
        _Py_atomic_store_relaxed(&interp_ceval_state->gc_scheduled, 0);
        COMPUTE_EVAL_BREAKER(tstate->interp, ceval, interp_ceval_state);
        _Py_RunGC(tstate);
    }

    /* GIL drop request */
    if (_Py_atomic_load_relaxed_int32(&interp_ceval_state->gil_drop_request)) {
        /* Give another thread a chance */
        if (PyEval_SaveThread() != tstate) {
            Py_FatalError("tstate mix-up");
        }

        /* Other threads may run now */

        PyEval_RestoreThread(tstate);
    }

    /* Check for asynchronous exception. */
    if (tstate->async_exc != NULL) {
        PyObject *exc = tstate->async_exc;
        tstate->async_exc = NULL;
        UNSIGNAL_ASYNC_EXC(tstate->interp);
        _PyErr_SetNone(tstate, exc);
        Py_DECREF(exc);
        return -1;
    }


    // It is possible that some of the conditions that trigger the eval breaker
    // are called in a different thread than the Python thread. An example of
    // this is bpo-42296: On Windows, _PyEval_SignalReceived() can be called in
    // a different thread than the Python thread, in which case
    // _Py_ThreadCanHandleSignals() is wrong. Recompute eval_breaker in the
    // current Python thread with the correct _Py_ThreadCanHandleSignals()
    // value. It prevents to interrupt the eval loop at every instruction if
    // the current Python thread cannot handle signals (if
    // _Py_ThreadCanHandleSignals() is false).
    COMPUTE_EVAL_BREAKER(tstate->interp, ceval, interp_ceval_state);

    return 0;
}

