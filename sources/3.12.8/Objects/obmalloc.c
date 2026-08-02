/* Python's malloc wrappers (see pymem.h) */

#include "Python.h"
#include "pycore_code.h"          // stats
#include "pycore_pystate.h"       // _PyInterpreterState_GET

#include "pycore_obmalloc.h"
#include "pycore_pymem.h"

#include <stdlib.h>               // malloc()
#include <stdbool.h>

#undef  uint
#define uint pymem_uint


/* Defined in tracemalloc.c */
extern void _PyMem_DumpTraceback(int fd, const void *ptr);

static void _PyObject_DebugDumpAddress(const void *p);
static void _PyMem_DebugCheckAddress(const char *func, char api_id, const void *p);


static void set_up_debug_hooks_domain_unlocked(PyMemAllocatorDomain domain);
static void set_up_debug_hooks_unlocked(void);
static void get_allocator_unlocked(PyMemAllocatorDomain, PyMemAllocatorEx *);
static void set_allocator_unlocked(PyMemAllocatorDomain, PyMemAllocatorEx *);


/***************************************/
/* low-level allocator implementations */
/***************************************/

/* the default raw allocator (wraps malloc) */









#define MALLOC_ALLOC {NULL, _PyMem_RawMalloc, _PyMem_RawCalloc, _PyMem_RawRealloc, _PyMem_RawFree}
#define PYRAW_ALLOC MALLOC_ALLOC

/* the default object allocator */

// The actual implementation is further down.

#ifdef WITH_PYMALLOC
void* _PyObject_Malloc(void *ctx, size_t size);
void* _PyObject_Calloc(void *ctx, size_t nelem, size_t elsize);
void _PyObject_Free(void *ctx, void *p);
void* _PyObject_Realloc(void *ctx, void *ptr, size_t size);
#  define PYMALLOC_ALLOC {NULL, _PyObject_Malloc, _PyObject_Calloc, _PyObject_Realloc, _PyObject_Free}
#  define PYOBJ_ALLOC PYMALLOC_ALLOC
#else
#  define PYOBJ_ALLOC MALLOC_ALLOC
#endif  // WITH_PYMALLOC

#define PYMEM_ALLOC PYOBJ_ALLOC

/* the default debug allocators */

// The actual implementation is further down.

void* _PyMem_DebugRawMalloc(void *ctx, size_t size);
void* _PyMem_DebugRawCalloc(void *ctx, size_t nelem, size_t elsize);
void* _PyMem_DebugRawRealloc(void *ctx, void *ptr, size_t size);
void _PyMem_DebugRawFree(void *ctx, void *ptr);

void* _PyMem_DebugMalloc(void *ctx, size_t size);
void* _PyMem_DebugCalloc(void *ctx, size_t nelem, size_t elsize);
void* _PyMem_DebugRealloc(void *ctx, void *ptr, size_t size);
void _PyMem_DebugFree(void *ctx, void *p);

#define PYDBGRAW_ALLOC \
    {&_PyRuntime.allocators.debug.raw, _PyMem_DebugRawMalloc, _PyMem_DebugRawCalloc, _PyMem_DebugRawRealloc, _PyMem_DebugRawFree}
#define PYDBGMEM_ALLOC \
    {&_PyRuntime.allocators.debug.mem, _PyMem_DebugMalloc, _PyMem_DebugCalloc, _PyMem_DebugRealloc, _PyMem_DebugFree}
#define PYDBGOBJ_ALLOC \
    {&_PyRuntime.allocators.debug.obj, _PyMem_DebugMalloc, _PyMem_DebugCalloc, _PyMem_DebugRealloc, _PyMem_DebugFree}

/* the low-level virtual memory allocator */

#ifdef WITH_PYMALLOC
#  ifdef MS_WINDOWS
#    include <windows.h>
#  elif defined(HAVE_MMAP)
#    include <sys/mman.h>
#    ifdef MAP_ANONYMOUS
#      define ARENAS_USE_MMAP
#    endif
#  endif
#endif





/*******************************************/
/* end low-level allocator implementations */
/*******************************************/


#if defined(__has_feature)  /* Clang */
#  if __has_feature(address_sanitizer) /* is ASAN enabled? */
#    define _Py_NO_SANITIZE_ADDRESS \
        __attribute__((no_sanitize("address")))
#  endif
#  if __has_feature(thread_sanitizer)  /* is TSAN enabled? */
#    define _Py_NO_SANITIZE_THREAD __attribute__((no_sanitize_thread))
#  endif
#  if __has_feature(memory_sanitizer)  /* is MSAN enabled? */
#    define _Py_NO_SANITIZE_MEMORY __attribute__((no_sanitize_memory))
#  endif
#elif defined(__GNUC__)
#  if defined(__SANITIZE_ADDRESS__)    /* GCC 4.8+, is ASAN enabled? */
#    define _Py_NO_SANITIZE_ADDRESS \
        __attribute__((no_sanitize_address))
#  endif
   // TSAN is supported since GCC 5.1, but __SANITIZE_THREAD__ macro
   // is provided only since GCC 7.
#  if __GNUC__ > 5 || (__GNUC__ == 5 && __GNUC_MINOR__ >= 1)
#    define _Py_NO_SANITIZE_THREAD __attribute__((no_sanitize_thread))
#  endif
#endif

#ifndef _Py_NO_SANITIZE_ADDRESS
#  define _Py_NO_SANITIZE_ADDRESS
#endif
#ifndef _Py_NO_SANITIZE_THREAD
#  define _Py_NO_SANITIZE_THREAD
#endif
#ifndef _Py_NO_SANITIZE_MEMORY
#  define _Py_NO_SANITIZE_MEMORY
#endif


#define ALLOCATORS_MUTEX (_PyRuntime.allocators.mutex)
#define _PyMem_Raw (_PyRuntime.allocators.standard.raw)
#define _PyMem (_PyRuntime.allocators.standard.mem)
#define _PyObject (_PyRuntime.allocators.standard.obj)
#define _PyMem_Debug (_PyRuntime.allocators.debug)
#define _PyObject_Arena (_PyRuntime.allocators.obj_arena)


/***************************/
/* managing the allocators */
/***************************/




#ifdef Py_DEBUG
static const int pydebug = 1;
#else
;
#endif




















#ifdef WITH_PYMALLOC



#endif






















/* Note that there is a possible, but very unlikely, race in any place
 * below where we call one of the allocator functions.  We access two
 * fields in each case:  "malloc", etc. and "ctx".
 *
 * It is unlikely that the allocator will be changed while one of those
 * calls is happening, much less in that very narrow window.
 * Furthermore, the likelihood of a race is drastically reduced by the
 * fact that the allocator may not be changed after runtime init
 * (except with a wrapper).
 *
 * With the above in mind, we currently don't worry about locking
 * around these uses of the runtime-global allocators state. */


/*************************/
/* the "arena" allocator */
/*************************/

void *
_PyObject_VirtualAlloc(size_t size)
{
    return _PyObject_Arena.alloc(_PyObject_Arena.ctx, size);
}

void
_PyObject_VirtualFree(void *obj, size_t size)
{
    _PyObject_Arena.free(_PyObject_Arena.ctx, obj, size);
}


/***********************/
/* the "raw" allocator */
/***********************/










/***********************/
/* the "mem" allocator */
/***********************/










/***************************/
/* pymem utility functions */
/***************************/








/**************************/
/* the "object" allocator */
/**************************/










/* If we're using GCC, use __builtin_expect() to reduce overhead of
   the valgrind checks */
#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#  define UNLIKELY(value) __builtin_expect((value), 0)
#  define LIKELY(value) __builtin_expect((value), 1)
#else
#  define UNLIKELY(value) (value)
#  define LIKELY(value) (value)
#endif

#ifdef WITH_PYMALLOC

#ifdef WITH_VALGRIND
#include <valgrind/valgrind.h>

/* -1 indicates that we haven't checked that we're running on valgrind yet. */
static int running_on_valgrind = -1;
#endif

typedef struct _obmalloc_state OMState;





// These macros all rely on a local "state" variable.
#define usedpools (state->pools.used)
#define allarenas (state->mgmt.arenas)
#define maxarenas (state->mgmt.maxarenas)
#define unused_arena_objects (state->mgmt.unused_arena_objects)
#define usable_arenas (state->mgmt.usable_arenas)
#define nfp2lasta (state->mgmt.nfp2lasta)
#define narenas_currently_allocated (state->mgmt.narenas_currently_allocated)
#define ntimes_arena_allocated (state->mgmt.ntimes_arena_allocated)
#define narenas_highwater (state->mgmt.narenas_highwater)
#define raw_allocated_blocks (state->mgmt.raw_allocated_blocks)





static Py_ssize_t get_num_global_allocated_blocks(_PyRuntimeState *);

/* We preserve the number of blockss leaked during runtime finalization,
   so they can be reported if the runtime is initialized again. */
// XXX We don't lose any information by dropping this,
// so we should consider doing so.
;







#if WITH_PYMALLOC_RADIX_TREE
/*==========================================================================*/
/* radix tree for tracking arena usage. */

#define arena_map_root (state->usage.arena_map_root)
#ifdef USE_INTERIOR_NODES
#define arena_map_mid_count (state->usage.arena_map_mid_count)
#define arena_map_bot_count (state->usage.arena_map_bot_count)
#endif

/* Return a pointer to a bottom tree node, return NULL if it doesn't exist or
 * it cannot be created */



/* The radix tree only tracks arenas.  So, for 16 MiB arenas, we throw
 * away 24 bits of the address.  That reduces the space requirement of
 * the tree compared to similar radix tree page-map schemes.  In
 * exchange for slashing the space requirement, it needs more
 * computation to check an address.
 *
 * Tracking coverage is done by "ideal" arena address.  It is easier to
 * explain in decimal so let's say that the arena size is 100 bytes.
 * Then, ideal addresses are 100, 200, 300, etc.  For checking if a
 * pointer address is inside an actual arena, we have to check two ideal
 * arena addresses.  E.g. if pointer is 357, we need to check 200 and
 * 300.  In the rare case that an arena is aligned in the ideal way
 * (e.g. base address of arena is 200) then we only have to check one
 * ideal address.
 *
 * The tree nodes for 200 and 300 both store the address of arena.
 * There are two cases: the arena starts at a lower ideal arena and
 * extends to this one, or the arena starts in this arena and extends to
 * the next ideal arena.  The tail_lo and tail_hi members correspond to
 * these two cases.
 */


/* mark or unmark addresses covered by arena */


/* Return true if 'p' is a pointer inside an obmalloc arena.
 * _PyObject_Free() calls this so it needs to be very fast. */


/* end of radix tree logic */
/*==========================================================================*/
#endif /* WITH_PYMALLOC_RADIX_TREE */


/* Allocate a new arena.  If we run out of memory, return NULL.  Else
 * allocate a new arena, and return the address of an arena_object
 * describing the new arena.  It's expected that the caller will set
 * `usable_arenas` to the return value.
 */




#if WITH_PYMALLOC_RADIX_TREE
/* Return true if and only if P is an address that was allocated by
   pymalloc.  When the radix tree is used, 'poolp' is unused.
 */

#else
/*
address_in_range(P, POOL)

Return true if and only if P is an address that was allocated by pymalloc.
POOL must be the pool address associated with P, i.e., POOL = POOL_ADDR(P)
(the caller is asked to compute this because the macro expands POOL more than
once, and for efficiency it's best for the caller to assign POOL_ADDR(P) to a
variable and pass the latter to the macro; because address_in_range is
called on every alloc/realloc/free, micro-efficiency is important here).

Tricky:  Let B be the arena base address associated with the pool, B =
arenas[(POOL)->arenaindex].address.  Then P belongs to the arena if and only if

    B <= P < B + ARENA_SIZE

Subtracting B throughout, this is true iff

    0 <= P-B < ARENA_SIZE

By using unsigned arithmetic, the "0 <=" half of the test can be skipped.

Obscure:  A PyMem "free memory" function can call the pymalloc free or realloc
before the first arena has been allocated.  `arenas` is still NULL in that
case.  We're relying on that maxarenas is also 0 in that case, so that
(POOL)->arenaindex < maxarenas  must be false, saving us from trying to index
into a NULL arenas.

Details:  given P and POOL, the arena_object corresponding to P is AO =
arenas[(POOL)->arenaindex].  Suppose obmalloc controls P.  Then (barring wild
stores, etc), POOL is the correct address of P's pool, AO.address is the
correct base address of the pool's arena, and P must be within ARENA_SIZE of
AO.address.  In addition, AO.address is not 0 (no arena can start at address 0
(NULL)).  Therefore address_in_range correctly reports that obmalloc
controls P.

Now suppose obmalloc does not control P (e.g., P was obtained via a direct
call to the system malloc() or realloc()).  (POOL)->arenaindex may be anything
in this case -- it may even be uninitialized trash.  If the trash arenaindex
is >= maxarenas, the macro correctly concludes at once that obmalloc doesn't
control P.

Else arenaindex is < maxarena, and AO is read up.  If AO corresponds to an
allocated arena, obmalloc controls all the memory in slice AO.address :
AO.address+ARENA_SIZE.  By case assumption, P is not controlled by obmalloc,
so P doesn't lie in that slice, so the macro correctly reports that P is not
controlled by obmalloc.

Finally, if P is not controlled by obmalloc and AO corresponds to an unused
arena_object (one not currently associated with an allocated arena),
AO.address is 0, and the second test in the macro reduces to:

    P < ARENA_SIZE

If P >= ARENA_SIZE (extremely likely), the macro again correctly concludes
that P is not controlled by obmalloc.  However, if P < ARENA_SIZE, this part
of the test still passes, and the third clause (AO.address != 0) is necessary
to get the correct result:  AO.address is 0 in this case, so the macro
correctly reports that P is not controlled by obmalloc (despite that P lies in
slice AO.address : AO.address + ARENA_SIZE).

Note:  The third (AO.address != 0) clause was added in Python 2.5.  Before
2.5, arenas were never free()'ed, and an arenaindex < maxarena always
corresponded to a currently-allocated arena, so the "P is not controlled by
obmalloc, AO corresponds to an unused arena_object, and P < ARENA_SIZE" case
was impossible.

Note that the logic is excruciating, and reading up possibly uninitialized
memory when P is not controlled by obmalloc (to get at (POOL)->arenaindex)
creates problems for some memory debuggers.  The overwhelming advantage is
that this test determines whether an arbitrary address is controlled by
obmalloc in a small constant time, independent of the number of arenas
obmalloc controls.  Since this test is needed at every entry point, it's
extremely desirable that it be this fast.
*/

static bool _Py_NO_SANITIZE_ADDRESS
            _Py_NO_SANITIZE_THREAD
            _Py_NO_SANITIZE_MEMORY
address_in_range(OMState *state, void *p, poolp pool)
{
    // Since address_in_range may be reading from memory which was not allocated
    // by Python, it is important that pool->arenaindex is read only once, as
    // another thread may be concurrently modifying the value without holding
    // the GIL. The following dance forces the compiler to read pool->arenaindex
    // only once.
    uint arenaindex = *((volatile uint *)&pool->arenaindex);
    return arenaindex < maxarenas &&
        (uintptr_t)p - allarenas[arenaindex].address < ARENA_SIZE &&
        allarenas[arenaindex].address != 0;
}

#endif /* !WITH_PYMALLOC_RADIX_TREE */

/*==========================================================================*/

// Called when freelist is exhausted.  Extend the freelist if there is
// space for a block.  Otherwise, remove this pool from usedpools.


/* called when pymalloc_alloc can not allocate a block from usedpool.
 * This function takes new pool and allocate a block from it.
 */


/* pymalloc allocator

   Return a pointer to newly allocated memory if pymalloc allocated memory.

   Return NULL if pymalloc failed to allocate the memory block: on bigger
   requests, on error in the code below (as a last chance to serve the request)
   or when the max memory limit has been reached.
*/













/* Free a memory block allocated by pymalloc_alloc().
   Return 1 if it was freed.
   Return 0 if the block was not allocated by pymalloc_alloc(). */






/* pymalloc realloc.

   If nbytes==0, then as the Python docs promise, we do not treat this like
   free(p), and return a non-NULL result.

   Return 1 if pymalloc reallocated memory and wrote the new pointer into
   newptr_p.

   Return 0 if pymalloc didn't allocated p. */





#else   /* ! WITH_PYMALLOC */

/*==========================================================================*/
/* pymalloc not enabled:  Redirect the entry points to malloc.  These will
 * only be used by extensions that are compiled with pymalloc enabled. */

Py_ssize_t
_PyInterpreterState_GetAllocatedBlocks(PyInterpreterState *Py_UNUSED(interp))
{
    return 0;
}

Py_ssize_t
_Py_GetGlobalAllocatedBlocks(void)
{
    return 0;
}

void
_PyInterpreterState_FinalizeAllocatedBlocks(PyInterpreterState *Py_UNUSED(interp))
{
    return;
}

void
_Py_FinalizeAllocatedBlocks(_PyRuntimeState *Py_UNUSED(runtime))
{
    return;
}

#endif /* WITH_PYMALLOC */


/*==========================================================================*/
/* A x-platform debugging allocator.  This doesn't manage memory directly,
 * it wraps a real allocator, adding extra debugging info to the memory blocks.
 */

/* Uncomment this define to add the "serialno" field */
/* #define PYMEM_DEBUG_SERIALNO */

#ifdef PYMEM_DEBUG_SERIALNO
static size_t serialno = 0;     /* incremented on each debug {m,re}alloc */

/* serialno is always incremented via calling this routine.  The point is
 * to supply a single place to set a breakpoint.
 */
static void
bumpserialno(void)
{
    ++serialno;
}
#endif

#define SST SIZEOF_SIZE_T

#ifdef PYMEM_DEBUG_SERIALNO
#  define PYMEM_DEBUG_EXTRA_BYTES 4 * SST
#else
#  define PYMEM_DEBUG_EXTRA_BYTES 3 * SST
#endif

/* Read sizeof(size_t) bytes at p as a big-endian size_t. */


/* Write n as a big-endian size_t, MSB at address p, LSB at
 * p + sizeof(size_t) - 1.
 */


/* Let S = sizeof(size_t).  The debug malloc asks for 4 * S extra bytes and
   fills them with useful stuff, here calling the underlying malloc's result p:

p[0: S]
    Number of bytes originally asked for.  This is a size_t, big-endian (easier
    to read in a memory dump).
p[S]
    API ID.  See PEP 445.  This is a character, but seems undocumented.
p[S+1: 2*S]
    Copies of PYMEM_FORBIDDENBYTE.  Used to catch under- writes and reads.
p[2*S: 2*S+n]
    The requested memory, filled with copies of PYMEM_CLEANBYTE.
    Used to catch reference to uninitialized memory.
    &p[2*S] is returned.  Note that this is 8-byte aligned if pymalloc
    handled the request itself.
p[2*S+n: 2*S+n+S]
    Copies of PYMEM_FORBIDDENBYTE.  Used to catch over- writes and reads.
p[2*S+n+S: 2*S+n+2*S]
    A serial number, incremented by 1 on each call to _PyMem_DebugMalloc
    and _PyMem_DebugRealloc.
    This is a big-endian size_t.
    If "bad memory" is detected later, the serial number gives an
    excellent way to set a breakpoint on the next run, to capture the
    instant at which this block was passed out.

If PYMEM_DEBUG_SERIALNO is not defined (default), the debug malloc only asks
for 3 * S extra bytes, and omits the last serialno field.
*/








/* The debug free first checks the 2*SST bytes on each end for sanity (in
   particular, that the FORBIDDENBYTEs with the api ID are still intact).
   Then fills the original bytes with PYMEM_DEADBYTE.
   Then calls the underlying free.
*/

















/* Check the forbidden bytes on both ends of the memory allocated for p.
 * If anything is wrong, print info to stderr via _PyObject_DebugDumpAddress,
 * and call Py_FatalError to kill the program.
 * The API id, is also checked.
 */


/* Display info to stderr about the memory block at p. */








#ifdef WITH_PYMALLOC

#ifdef Py_DEBUG
/* Is target in the list?  The list is traversed via the nextpool pointers.
 * The list may be NULL-terminated, or circular.  Return 1 if target is in
 * list, else 0.
 */
static int
pool_is_in_list(const poolp target, poolp list)
{
    poolp origlist = list;
    assert(target != NULL);
    if (list == NULL)
        return 0;
    do {
        if (target == list)
            return 1;
        list = list->nextpool;
    } while (list != NULL && list != origlist);
    return 0;
}
#endif

/* Print summary info to "out" about the state of pymalloc's structures.
 * In Py_DEBUG mode, also perform some expensive internal consistency
 * checks.
 *
 * Return 0 if the memory debug hooks are not installed or no statistics was
 * written into out, return 1 otherwise.
 */


#endif /* #ifdef WITH_PYMALLOC */
