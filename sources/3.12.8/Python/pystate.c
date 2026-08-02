
/* Thread and interpreter state structures and their interfaces */

#include "Python.h"
#include "pycore_ceval.h"
#include "pycore_code.h"          // stats
#include "pycore_dtoa.h"          // _dtoa_state_INIT()
#include "pycore_frame.h"
#include "pycore_initconfig.h"
#include "pycore_object.h"        // _PyType_InitCache()
#include "pycore_pyerrors.h"
#include "pycore_pylifecycle.h"
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()
#include "pycore_pystate.h"
#include "pycore_runtime_init.h"  // _PyRuntimeState_INIT
#include "pycore_sysmodule.h"

/* --------------------------------------------------------------------------
CAUTION

Always use PyMem_RawMalloc() and PyMem_RawFree() directly in this file.  A
number of these functions are advertised as safe to call when the GIL isn't
held, and in a debug build Python redirects (e.g.) PyMem_NEW (etc) to Python's
debugging obmalloc functions.  Those aren't thread-safe (they rely on the GIL
to avoid the expense of doing their own locking).
-------------------------------------------------------------------------- */

#ifdef HAVE_DLOPEN
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#if !HAVE_DECL_RTLD_LAZY
#define RTLD_LAZY 1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


/****************************************/
/* helpers for the current thread state */
/****************************************/

// API for the current thread state is further down.

/* "current" means one of:
   - bound to the current OS thread
   - holds the GIL
 */

//-------------------------------------------------
// a highly efficient lookup for the current thread
//-------------------------------------------------

/*
   The stored thread state is set by PyThreadState_Swap().

   For each of these functions, the GIL must be held by the current thread.
 */


#ifdef HAVE_THREAD_LOCAL
;
#endif







#define tstate_verify_not_active(tstate) \
    if (tstate == current_fast_get((tstate)->interp->runtime)) { \
        _Py_FatalErrorFormat(__func__, "tstate %p is still current", tstate); \
    }




//------------------------------------------------
// the thread state bound to the current OS thread
//------------------------------------------------













#ifdef HAVE_FORK
/* Reset the TSS key - called by PyOS_AfterFork_Child().
 * This should not be necessary, but some - buggy - pthread implementations
 * don't reset TSS upon fork(), see issue #10517.
 */

#endif


/*
   The stored thread state is set by bind_tstate() (AKA PyThreadState_Bind().

   The GIL does no need to be held for these.
  */

#define gilstate_tss_initialized(runtime) \
    tstate_tss_initialized(&(runtime)->autoTSSkey)
#define gilstate_tss_init(runtime) \
    tstate_tss_init(&(runtime)->autoTSSkey)
#define gilstate_tss_fini(runtime) \
    tstate_tss_fini(&(runtime)->autoTSSkey)
#define gilstate_tss_get(runtime) \
    tstate_tss_get(&(runtime)->autoTSSkey)
#define _gilstate_tss_set(runtime, tstate) \
    tstate_tss_set(&(runtime)->autoTSSkey, tstate)
#define _gilstate_tss_clear(runtime) \
    tstate_tss_clear(&(runtime)->autoTSSkey)
#define gilstate_tss_reinit(runtime) \
    tstate_tss_reinit(&(runtime)->autoTSSkey)






#ifndef NDEBUG
static inline int tstate_is_alive(PyThreadState *tstate);

static inline int
tstate_is_bound(PyThreadState *tstate)
{
    return tstate->_status.bound && !tstate->_status.unbound;
}
#endif  // !NDEBUG

static void bind_gilstate_tstate(PyThreadState *);
static void unbind_gilstate_tstate(PyThreadState *);






/* Stick the thread state for this thread in thread specific storage.

   When a thread state is created for a thread by some mechanism
   other than PyGILState_Ensure(), it's important that the GILState
   machinery knows about it so it doesn't try to create another
   thread state for the thread.
   (This is a better fix for SF bug #1010677 than the first one attempted.)

   The only situation where you can legitimately have more than one
   thread state for an OS level thread is when there are multiple
   interpreters.

   Before 3.12, the PyGILState_*() APIs didn't work with multiple
   interpreters (see bpo-10915 and bpo-15751), so this function used
   to set TSS only once.  Thus, the first thread state created for that
   given OS level thread would "win", which seemed reasonable behaviour.
*/






//----------------------------------------------
// the thread state that currently holds the GIL
//----------------------------------------------

/* This is not exported, as it is not reliable!  It can only
   ever be compared to the state for the *current* thread.
   * If not equal, then it doesn't matter that the actual
     value may change immediately after comparison, as it can't
     possibly change to the current thread's state.
   * If equal, then the current thread holds the lock, so the value can't
     change until we yield the lock.
*/



/****************************/
/* the global runtime state */
/****************************/

//----------
// lifecycle
//----------

/* Suppress deprecation warning for PyBytesObject.ob_shash */
_Py_COMP_DIAG_PUSH
_Py_COMP_DIAG_IGNORE_DEPR_DECLS
/* We use "initial" if the runtime gets re-used
   (e.g. Py_Finalize() followed by Py_Initialize().
   Note that we initialize "initial" relative to _PyRuntime,
   to ensure pre-initialized pointers point to the active
   runtime state (and not "initial"). */
;
_Py_COMP_DIAG_POP

#define NUMLOCKS 9
#define LOCKS_INIT(runtime) \
    { \
        &(runtime)->interpreters.mutex, \
        &(runtime)->xidregistry.mutex, \
        &(runtime)->getargs.mutex, \
        &(runtime)->unicode_state.ids.lock, \
        &(runtime)->imports.extensions.mutex, \
        &(runtime)->ceval.pending_mainthread.lock, \
        &(runtime)->atexit.mutex, \
        &(runtime)->audit_hooks.mutex, \
        &(runtime)->allocators.mutex, \
    }







static void _xidregistry_clear(struct _xidregistry *);



#ifdef HAVE_FORK
/* This function is called from PyOS_AfterFork_Child to ensure that
   newly created child processes do not share locks with the parent. */

#endif


/*************************************/
/* the per-interpreter runtime state */
/*************************************/

//----------
// lifecycle
//----------

/* Calling this indicates that the runtime is ready to create interpreters. */








/* Get the interpreter state to a minimal consistent state.
   Further init happens in pylifecycle.c before it can be used.
   All fields not initialized here are expected to be zeroed out,
   e.g. by PyMem_RawCalloc() or memset(), or otherwise pre-initialized.
   The runtime state is not manipulated.  Instead it is assumed that
   the interpreter is getting added to the runtime.

   Note that the main interpreter was statically initialized as part
   of the runtime and most state is already set properly.  That leaves
   a small number of fields to initialize dynamically, as well as some
   that are initialized lazily.

   For subinterpreters we memcpy() the main interpreter in
   PyInterpreterState_New(), leaving it in the same mostly-initialized
   state.  The only difference is that the interpreter has some
   self-referential state that is statically initializexd to the
   main interpreter.  We fix those fields here, in addition
   to the other dynamically initialized fields.
  */














static inline void tstate_deactivate(PyThreadState *tstate);
static void zapthreads(PyInterpreterState *interp);




#ifdef HAVE_FORK
/*
 * Delete all interpreter states except the main interpreter.  If there
 * is a current interpreter state, it *must* be the main interpreter.
 */

#endif









//----------
// accessors
//----------





















//-----------------------------
// look up an interpreter state
//-----------------------------

/* Return the interpreter associated with the current OS thread.

   The GIL must be held.
  */






/* Return the interpreter state with the given ID.

   Fail with RuntimeError if the interpreter is not found. */




/********************************/
/* the per-thread runtime state */
/********************************/

#ifndef NDEBUG
static inline int
tstate_is_alive(PyThreadState *tstate)
{
    return (tstate->_status.initialized &&
            !tstate->_status.finalized &&
            !tstate->_status.cleared &&
            !tstate->_status.finalizing);
}
#endif


//----------
// lifecycle
//----------

/* Minimum size of data stack chunk */
#define DATA_STACK_CHUNK_SIZE (16*1024)

static _PyStackChunk*
allocate_chunk(int size_in_bytes, _PyStackChunk* previous)
{
    assert(size_in_bytes % sizeof(PyObject **) == 0);
    _PyStackChunk *res = _PyObject_VirtualAlloc(size_in_bytes);
    if (res == NULL) {
        return NULL;
    }
    res->previous = previous;
    res->size = size_in_bytes;
    res->top = 0;
    return res;
}





/* Get the thread state to a minimal consistent state.
   Further init happens in pylifecycle.c before it can be used.
   All fields not initialized here are expected to be zeroed out,
   e.g. by PyMem_RawCalloc() or memset(), or otherwise pre-initialized.
   The interpreter state is not manipulated.  Instead it is assumed that
   the thread is getting added to the interpreter.
  */









// This must be followed by a call to _PyThreadState_Bind();


// We keep this for stable ABI compabibility.


// We keep this around for (accidental) stable ABI compatibility.
// Realistically, no extensions are using it.







/* Common code for PyThreadState_Delete() and PyThreadState_DeleteCurrent() */













/*
 * Delete all thread states except the one passed as argument.
 * Note that, if there is a current thread state, it *must* be the one
 * passed as argument.  Also, this won't touch any other interpreters
 * than the current one, since we don't know which thread state should
 * be kept in those other interpreters.
 */



//----------
// accessors
//----------

/* An extension mechanism to store arbitrary additional per-thread state.
   PyThreadState_GetDict() returns a dictionary that can be used to hold such
   state; the caller should pick a unique key and store its state there.  If
   PyThreadState_GetDict() returns NULL, an exception has *not* been raised
   and the caller should assume no per-thread state is available. */





















//----------
// other API
//----------

/* Asynchronously raise an exception in a thread.
   Requested by Just van Rossum and Alex Martelli.
   To prevent naive misuse, you must write your own extension
   to call this, or use ctypes.  Must be called with the GIL held.
   Returns the number of tstates modified (normally 1, but 0 if `id` didn't
   match any known thread id).  Can be called with exc=NULL to clear an
   existing async exception.  This raises no exceptions. */

// XXX Move this to Python/ceval_gil.c?
// XXX Deprecate this.



//---------------------------------
// API for the current thread state
//---------------------------------



















/***********************************/
/* routines for advanced debuggers */
/***********************************/

// (requested by David Beazley)
// Don't use unless you know what you are doing!












/********************************************/
/* reporting execution state of all threads */
/********************************************/

/* The implementation of sys._current_frames().  This is intended to be
   called with the GIL held, as it will be when called via
   sys._current_frames().  It's possible it would work fine even without
   the GIL held, but haven't thought enough about that.
*/


/* The implementation of sys._current_exceptions().  This is intended to be
   called with the GIL held, as it will be when called via
   sys._current_exceptions().  It's possible it would work fine even without
   the GIL held, but haven't thought enough about that.
*/



/***********************************/
/* Python "auto thread state" API. */
/***********************************/

/* Internal initialization/finalization functions called by
   Py_Initialize/Py_FinalizeEx
*/





// XXX Drop this.




/* The public functions */










/**************************/
/* cross-interpreter data */
/**************************/

/* cross-interpreter data */













crossinterpdatafunc _PyCrossInterpreterData_Lookup(PyObject *);

/* This is a separate func from _PyCrossInterpreterData_Lookup in order
   to keep the registry code separate. */


















/* registry of {type -> crossinterpdatafunc} */

/* For now we use a global registry of shareable classes.  An
   alternative would be to add a tp_* slot for a class's
   crossinterpdatafunc. It would be simpler and more efficient. */











static void _register_builtins_for_crossinterpreter_data(struct _xidregistry *xidregistry);








/* Cross-interpreter objects are looked up by exact match on the class.
   We can reassess this policy when we move from a global registry to a
   tp_* slot. */



/* cross-interpreter data for builtin types */

struct _shared_bytes_data {
    char *bytes;
    Py_ssize_t len;
};





struct _shared_str_data {
    int kind;
    const void *buffer;
    Py_ssize_t len;
};
















/*************/
/* Other API */
/*************/



















#define MINIMUM_OVERHEAD 1000

static PyObject **
push_chunk(PyThreadState *tstate, int size)
{
    int allocate_size = DATA_STACK_CHUNK_SIZE;
    while (allocate_size < (int)sizeof(PyObject*)*(size + MINIMUM_OVERHEAD)) {
        allocate_size *= 2;
    }
    _PyStackChunk *new = allocate_chunk(allocate_size, tstate->datastack_chunk);
    if (new == NULL) {
        return NULL;
    }
    if (tstate->datastack_chunk) {
        tstate->datastack_chunk->top = tstate->datastack_top -
                                       &tstate->datastack_chunk->data[0];
    }
    tstate->datastack_chunk = new;
    tstate->datastack_limit = (PyObject **)(((char *)new) + allocate_size);
    // When new is the "root" chunk (i.e. new->previous == NULL), we can keep
    // _PyThreadState_PopFrame from freeing it later by "skipping" over the
    // first element:
    PyObject **res = &new->data[new->previous == NULL];
    tstate->datastack_top = res + size;
    return res;
}

_PyInterpreterFrame *
_PyThreadState_PushFrame(PyThreadState *tstate, size_t size)
{
    assert(size < INT_MAX/sizeof(PyObject *));
    if (_PyThreadState_HasStackSpace(tstate, (int)size)) {
        _PyInterpreterFrame *res = (_PyInterpreterFrame *)tstate->datastack_top;
        tstate->datastack_top += size;
        return res;
    }
    return (_PyInterpreterFrame *)push_chunk(tstate, (int)size);
}

void
_PyThreadState_PopFrame(PyThreadState *tstate, _PyInterpreterFrame * frame)
{
    assert(tstate->datastack_chunk);
    PyObject **base = (PyObject **)frame;
    if (base == &tstate->datastack_chunk->data[0]) {
        _PyStackChunk *chunk = tstate->datastack_chunk;
        _PyStackChunk *previous = chunk->previous;
        // push_chunk ensures that the root chunk is never popped:
        assert(previous);
        tstate->datastack_top = &previous->data[previous->top];
        tstate->datastack_chunk = previous;
        _PyObject_VirtualFree(chunk, chunk->size);
        tstate->datastack_limit = (PyObject **)(((char *)previous) + previous->size);
    }
    else {
        assert(tstate->datastack_top);
        assert(tstate->datastack_top >= base);
        tstate->datastack_top = base;
    }
}


#ifndef NDEBUG
// Check that a Python thread state valid. In practice, this function is used
// on a Python debug build to check if 'tstate' is a dangling pointer, if the
// PyThreadState memory has been freed.
//
// Usage:
//
//     assert(_PyThreadState_CheckConsistency(tstate));
int
_PyThreadState_CheckConsistency(PyThreadState *tstate)
{
    assert(!_PyMem_IsPtrFreed(tstate));
    assert(!_PyMem_IsPtrFreed(tstate->interp));
    return 1;
}
#endif


// Check if a Python thread must exit immediately, rather than taking the GIL
// if Py_Finalize() has been called.
//
// When this function is called by a daemon thread after Py_Finalize() has been
// called, the GIL does no longer exist.
//
// tstate can be a dangling pointer (point to freed memory): only tstate value
// is used, the pointer is not deferenced.
//
// tstate must be non-NULL.



#ifdef __cplusplus
}
#endif
