/* Dictionary object implementation using a hash table */

/* The distribution includes a separate file, Objects/dictnotes.txt,
   describing explorations into dictionary design and optimization.
   It covers typical dictionary use patterns, the parameters for
   tuning dictionaries, and several ideas for possible optimizations.
*/

/* PyDictKeysObject

This implements the dictionary's hashtable.

As of Python 3.6, this is compact and ordered. Basic idea is described here:
* https://mail.python.org/pipermail/python-dev/2012-December/123028.html
* https://morepypy.blogspot.com/2015/01/faster-more-memory-efficient-and-more.html

layout:

+---------------------+
| dk_refcnt           |
| dk_log2_size        |
| dk_log2_index_bytes |
| dk_kind             |
| dk_usable           |
| dk_nentries         |
+---------------------+
| dk_indices[]        |
|                     |
+---------------------+
| dk_entries[]        |
|                     |
+---------------------+

dk_indices is actual hashtable.  It holds index in entries, or DKIX_EMPTY(-1)
or DKIX_DUMMY(-2).
Size of indices is dk_size.  Type of each index in indices is vary on dk_size:

* int8  for          dk_size <= 128
* int16 for 256   <= dk_size <= 2**15
* int32 for 2**16 <= dk_size <= 2**31
* int64 for 2**32 <= dk_size

dk_entries is array of PyDictKeyEntry when dk_kind == DICT_KEYS_GENERAL or
PyDictUnicodeEntry otherwise. Its length is USABLE_FRACTION(dk_size).

NOTE: Since negative value is used for DKIX_EMPTY and DKIX_DUMMY, type of
dk_indices entry is signed integer and int16 is used for table which
dk_size == 256.
*/


/*
The DictObject can be in one of two forms.

Either:
  A combined table:
    ma_values == NULL, dk_refcnt == 1.
    Values are stored in the me_value field of the PyDictKeysObject.
Or:
  A split table:
    ma_values != NULL, dk_refcnt >= 1
    Values are stored in the ma_values array.
    Only string (unicode) keys are allowed.

There are four kinds of slots in the table (slot is index, and
DK_ENTRIES(keys)[index] if index >= 0):

1. Unused.  index == DKIX_EMPTY
   Does not hold an active (key, value) pair now and never did.  Unused can
   transition to Active upon key insertion.  This is each slot's initial state.

2. Active.  index >= 0, me_key != NULL and me_value != NULL
   Holds an active (key, value) pair.  Active can transition to Dummy or
   Pending upon key deletion (for combined and split tables respectively).
   This is the only case in which me_value != NULL.

3. Dummy.  index == DKIX_DUMMY  (combined only)
   Previously held an active (key, value) pair, but that was deleted and an
   active pair has not yet overwritten the slot.  Dummy can transition to
   Active upon key insertion.  Dummy slots cannot be made Unused again
   else the probe sequence in case of collision would have no way to know
   they were once active.

4. Pending. index >= 0, key != NULL, and value == NULL  (split only)
   Not yet inserted in split-table.
*/

/*
Preserving insertion order

It's simple for combined table.  Since dk_entries is mostly append only, we can
get insertion order by just iterating dk_entries.

One exception is .popitem().  It removes last item in dk_entries and decrement
dk_nentries to achieve amortized O(1).  Since there are DKIX_DUMMY remains in
dk_indices, we can't increment dk_usable even though dk_nentries is
decremented.

To preserve the order in a split table, a bit vector is used  to record the
insertion order. When a key is inserted the bit vector is shifted up by 4 bits
and the index of the key is stored in the low 4 bits.
As a consequence of this, split keys have a maximum size of 16.
*/

/* PyDict_MINSIZE is the starting size for any new dict.
 * 8 allows dicts with no more than 5 active entries; experiments suggested
 * this suffices for the majority of dicts (consisting mostly of usually-small
 * dicts created to pass keyword arguments).
 * Making this 8, rather than 4 reduces the number of resizes for most
 * dictionaries, without any significant extra memory use.
 */
#define PyDict_LOG_MINSIZE 3
#define PyDict_MINSIZE 8

#include "Python.h"
#include "pycore_bitutils.h"      // _Py_bit_length
#include "pycore_call.h"          // _PyObject_CallNoArgs()
#include "pycore_code.h"          // stats
#include "pycore_dict.h"          // PyDictKeysObject
#include "pycore_gc.h"            // _PyObject_GC_IS_TRACKED()
#include "pycore_object.h"        // _PyObject_GC_TRACK()
#include "pycore_pyerrors.h"      // _PyErr_GetRaisedException()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "stringlib/eq.h"         // unicode_eq()

#include <stdbool.h>

/*[clinic input]
class dict "PyDictObject *" "&PyDict_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=f157a5a0ce9589d6]*/


/*
To ensure the lookup algorithm terminates, there must be at least one Unused
slot (NULL key) in the table.
To avoid slowing down lookups on a near-full table, we resize the table when
it's USABLE_FRACTION (currently two-thirds) full.
*/

#define PERTURB_SHIFT 5

/*
Major subtleties ahead:  Most hash schemes depend on having a "good" hash
function, in the sense of simulating randomness.  Python doesn't:  its most
important hash functions (for ints) are very regular in common
cases:

  >>>[hash(i) for i in range(4)]
  [0, 1, 2, 3]

This isn't necessarily bad!  To the contrary, in a table of size 2**i, taking
the low-order i bits as the initial table index is extremely fast, and there
are no collisions at all for dicts indexed by a contiguous range of ints. So
this gives better-than-random behavior in common cases, and that's very
desirable.

OTOH, when collisions occur, the tendency to fill contiguous slices of the
hash table makes a good collision resolution strategy crucial.  Taking only
the last i bits of the hash code is also vulnerable:  for example, consider
the list [i << 16 for i in range(20000)] as a set of keys.  Since ints are
their own hash codes, and this fits in a dict of size 2**15, the last 15 bits
 of every hash code are all 0:  they *all* map to the same table index.

But catering to unusual cases should not slow the usual ones, so we just take
the last i bits anyway.  It's up to collision resolution to do the rest.  If
we *usually* find the key we're looking for on the first try (and, it turns
out, we usually do -- the table load factor is kept under 2/3, so the odds
are solidly in our favor), then it makes best sense to keep the initial index
computation dirt cheap.

The first half of collision resolution is to visit table indices via this
recurrence:

    j = ((5*j) + 1) mod 2**i

For any initial j in range(2**i), repeating that 2**i times generates each
int in range(2**i) exactly once (see any text on random-number generation for
proof).  By itself, this doesn't help much:  like linear probing (setting
j += 1, or j -= 1, on each loop trip), it scans the table entries in a fixed
order.  This would be bad, except that's not the only thing we do, and it's
actually *good* in the common cases where hash keys are consecutive.  In an
example that's really too small to make this entirely clear, for a table of
size 2**3 the order of indices is:

    0 -> 1 -> 6 -> 7 -> 4 -> 5 -> 2 -> 3 -> 0 [and here it's repeating]

If two things come in at index 5, the first place we look after is index 2,
not 6, so if another comes in at index 6 the collision at 5 didn't hurt it.
Linear probing is deadly in this case because there the fixed probe order
is the *same* as the order consecutive keys are likely to arrive.  But it's
extremely unlikely hash codes will follow a 5*j+1 recurrence by accident,
and certain that consecutive hash codes do not.

The other half of the strategy is to get the other bits of the hash code
into play.  This is done by initializing a (unsigned) vrbl "perturb" to the
full hash code, and changing the recurrence to:

    perturb >>= PERTURB_SHIFT;
    j = (5*j) + 1 + perturb;
    use j % 2**i as the next table index;

Now the probe sequence depends (eventually) on every bit in the hash code,
and the pseudo-scrambling property of recurring on 5*j+1 is more valuable,
because it quickly magnifies small differences in the bits that didn't affect
the initial index.  Note that because perturb is unsigned, if the recurrence
is executed often enough perturb eventually becomes and remains 0.  At that
point (very rarely reached) the recurrence is on (just) 5*j+1 again, and
that's certain to find an empty slot eventually (since it generates every int
in range(2**i), and we make sure there's always at least one empty slot).

Selecting a good value for PERTURB_SHIFT is a balancing act.  You want it
small so that the high bits of the hash code continue to affect the probe
sequence across iterations; but you want it large so that in really bad cases
the high-order hash bits have an effect on early iterations.  5 was "the
best" in minimizing total collisions across experiments Tim Peters ran (on
both normal and pathological cases), but 4 and 6 weren't significantly worse.

Historical: Reimer Behrends contributed the idea of using a polynomial-based
approach, using repeated multiplication by x in GF(2**n) where an irreducible
polynomial for each table size was chosen such that x was a primitive root.
Christian Tismer later extended that to use division by x instead, as an
efficient way to get the high bits of the hash code into play.  This scheme
also gave excellent collision statistics, but was more expensive:  two
if-tests were required inside the loop; computing "the next" index took about
the same number of operations but without as much potential parallelism
(e.g., computing 5*j can go on at the same time as computing 1+perturb in the
above, and then shifting perturb can be done while the table index is being
masked); and the PyDictObject struct required a member to hold the table's
polynomial.  In Tim's experiments the current scheme ran faster, produced
equally good collision statistics, needed less code & used less memory.

*/

static int dictresize(PyInterpreterState *interp, PyDictObject *mp,
                      uint8_t log_newsize, int unicode);

static PyObject* dict_iter(PyDictObject *dict);

#include "clinic/dictobject.c.h"


#if PyDict_MAXFREELIST > 0
static struct _Py_dict_state *
get_dict_state(PyInterpreterState *interp)
{
    return &interp->dict_state;
}
#endif


void
_PyDict_ClearFreeList(PyInterpreterState *interp)
{
#if PyDict_MAXFREELIST > 0
    struct _Py_dict_state *state = &interp->dict_state;
    while (state->numfree) {
        PyDictObject *op = state->free_list[--state->numfree];
        assert(PyDict_CheckExact(op));
        PyObject_GC_Del(op);
    }
    while (state->keys_numfree) {
        PyObject_Free(state->keys_free_list[--state->keys_numfree]);
    }
#endif
}




static inline Py_hash_t
unicode_get_hash(PyObject *o)
{
    assert(PyUnicode_CheckExact(o));
    return _PyASCIIObject_CAST(o)->hash;
}

/* Print summary info about the state of the optimized allocator */


#define DK_MASK(dk) (DK_SIZE(dk)-1)

static void free_keys_object(PyInterpreterState *interp, PyDictKeysObject *keys);

/* PyDictKeysObject has refcounts like PyObject does, so we have the
   following two functions to mirror what Py_INCREF() and Py_DECREF() do.
   (Keep in mind that PyDictKeysObject isn't actually a PyObject.)
   Likewise a PyDictKeysObject can be immortal (e.g. Py_EMPTY_KEYS),
   so we apply a naive version of what Py_INCREF() and Py_DECREF() do
   for immortal objects. */



static inline void
dictkeys_decref(PyInterpreterState *interp, PyDictKeysObject *dk)
{
    if (dk->dk_refcnt == _Py_IMMORTAL_REFCNT) {
        return;
    }
    assert(dk->dk_refcnt > 0);
#ifdef Py_REF_DEBUG
    _Py_DecRefTotal(_PyInterpreterState_GET());
#endif
    if (--dk->dk_refcnt == 0) {
        free_keys_object(interp, dk);
    }
}

/* lookup indices.  returns DKIX_EMPTY, DKIX_DUMMY, or ix >=0 */
static inline Py_ssize_t
dictkeys_get_index(const PyDictKeysObject *keys, Py_ssize_t i)
{
    int log2size = DK_LOG_SIZE(keys);
    Py_ssize_t ix;

    if (log2size < 8) {
        const int8_t *indices = (const int8_t*)(keys->dk_indices);
        ix = indices[i];
    }
    else if (log2size < 16) {
        const int16_t *indices = (const int16_t*)(keys->dk_indices);
        ix = indices[i];
    }
#if SIZEOF_VOID_P > 4
    else if (log2size >= 32) {
        const int64_t *indices = (const int64_t*)(keys->dk_indices);
        ix = indices[i];
    }
#endif
    else {
        const int32_t *indices = (const int32_t*)(keys->dk_indices);
        ix = indices[i];
    }
    assert(ix >= DKIX_DUMMY);
    return ix;
}

/* write to indices. */
static inline void
dictkeys_set_index(PyDictKeysObject *keys, Py_ssize_t i, Py_ssize_t ix)
{
    int log2size = DK_LOG_SIZE(keys);

    assert(ix >= DKIX_DUMMY);
    assert(keys->dk_version == 0);

    if (log2size < 8) {
        int8_t *indices = (int8_t*)(keys->dk_indices);
        assert(ix <= 0x7f);
        indices[i] = (char)ix;
    }
    else if (log2size < 16) {
        int16_t *indices = (int16_t*)(keys->dk_indices);
        assert(ix <= 0x7fff);
        indices[i] = (int16_t)ix;
    }
#if SIZEOF_VOID_P > 4
    else if (log2size >= 32) {
        int64_t *indices = (int64_t*)(keys->dk_indices);
        indices[i] = ix;
    }
#endif
    else {
        int32_t *indices = (int32_t*)(keys->dk_indices);
        assert(ix <= 0x7fffffff);
        indices[i] = (int32_t)ix;
    }
}


/* USABLE_FRACTION is the maximum dictionary load.
 * Increasing this ratio makes dictionaries more dense resulting in more
 * collisions.  Decreasing it improves sparseness at the expense of spreading
 * indices over more cache lines and at the cost of total memory consumed.
 *
 * USABLE_FRACTION must obey the following:
 *     (0 < USABLE_FRACTION(n) < n) for all n >= 2
 *
 * USABLE_FRACTION should be quick to calculate.
 * Fractions around 1/2 to 2/3 seem to work well in practice.
 */
#define USABLE_FRACTION(n) (((n) << 1)/3)

/* Find the smallest dk_size >= minsize. */
static inline uint8_t
calculate_log2_keysize(Py_ssize_t minsize)
{
#if SIZEOF_LONG == SIZEOF_SIZE_T
    minsize = (minsize | PyDict_MINSIZE) - 1;
    return _Py_bit_length(minsize | (PyDict_MINSIZE-1));
#elif defined(_MSC_VER)
    // On 64bit Windows, sizeof(long) == 4.
    minsize = (minsize | PyDict_MINSIZE) - 1;
    unsigned long msb;
    _BitScanReverse64(&msb, (uint64_t)minsize);
    return (uint8_t)(msb + 1);
#else
    uint8_t log2_size;
    for (log2_size = PyDict_LOG_MINSIZE;
            (((Py_ssize_t)1) << log2_size) < minsize;
            log2_size++)
        ;
    return log2_size;
#endif
}

/* estimate_keysize is reverse function of USABLE_FRACTION.
 *
 * This can be used to reserve enough size to insert n entries without
 * resizing.
 */
static inline uint8_t
estimate_log2_keysize(Py_ssize_t n)
{
    return calculate_log2_keysize((n*3 + 1) / 2);
}


/* GROWTH_RATE. Growth rate upon hitting maximum load.
 * Currently set to used*3.
 * This means that dicts double in size when growing without deletions,
 * but have more head room when the number of deletions is on a par with the
 * number of insertions.  See also bpo-17563 and bpo-33205.
 *
 * GROWTH_RATE was set to used*4 up to version 3.2.
 * GROWTH_RATE was set to used*2 in version 3.3.0
 * GROWTH_RATE was set to used*2 + capacity/2 in 3.4.0-3.6.0.
 */
#define GROWTH_RATE(d) ((d)->ma_used*3)

/* This immutable, empty PyDictKeysObject is used for PyDict_Clear()
 * (which cannot fail and thus can do no allocation).
 */
static PyDictKeysObject empty_keys_struct = {
        _Py_IMMORTAL_REFCNT, /* dk_refcnt */
        0, /* dk_log2_size */
        0, /* dk_log2_index_bytes */
        DICT_KEYS_UNICODE, /* dk_kind */
        1, /* dk_version */
        0, /* dk_usable (immutable) */
        0, /* dk_nentries */
        {DKIX_EMPTY, DKIX_EMPTY, DKIX_EMPTY, DKIX_EMPTY,
         DKIX_EMPTY, DKIX_EMPTY, DKIX_EMPTY, DKIX_EMPTY}, /* dk_indices */
};

#define Py_EMPTY_KEYS &empty_keys_struct

/* Uncomment to check the dict content in _PyDict_CheckConsistency() */
// #define DEBUG_PYDICT

#ifdef DEBUG_PYDICT
#  define ASSERT_CONSISTENT(op) assert(_PyDict_CheckConsistency((PyObject *)(op), 1))
#else
#  define ASSERT_CONSISTENT(op) assert(_PyDict_CheckConsistency((PyObject *)(op), 0))
#endif

static inline int
get_index_from_order(PyDictObject *mp, Py_ssize_t i)
{
    assert(mp->ma_used <= SHARED_KEYS_MAX_SIZE);
    assert(i < (((char *)mp->ma_values)[-2]));
    return ((char *)mp->ma_values)[-3-i];
}

#ifdef DEBUG_PYDICT
static void
dump_entries(PyDictKeysObject *dk)
{
    for (Py_ssize_t i = 0; i < dk->dk_nentries; i++) {
        if (DK_IS_UNICODE(dk)) {
            PyDictUnicodeEntry *ep = &DK_UNICODE_ENTRIES(dk)[i];
            printf("key=%p value=%p\n", ep->me_key, ep->me_value);
        }
        else {
            PyDictKeyEntry *ep = &DK_ENTRIES(dk)[i];
            printf("key=%p hash=%lx value=%p\n", ep->me_key, ep->me_hash, ep->me_value);
        }
    }
}
#endif




static PyDictKeysObject*
new_keys_object(PyInterpreterState *interp, uint8_t log2_size, bool unicode)
{
    PyDictKeysObject *dk;
    Py_ssize_t usable;
    int log2_bytes;
    size_t entry_size = unicode ? sizeof(PyDictUnicodeEntry) : sizeof(PyDictKeyEntry);

    assert(log2_size >= PyDict_LOG_MINSIZE);

    usable = USABLE_FRACTION((size_t)1<<log2_size);
    if (log2_size < 8) {
        log2_bytes = log2_size;
    }
    else if (log2_size < 16) {
        log2_bytes = log2_size + 1;
    }
#if SIZEOF_VOID_P > 4
    else if (log2_size >= 32) {
        log2_bytes = log2_size + 3;
    }
#endif
    else {
        log2_bytes = log2_size + 2;
    }

#if PyDict_MAXFREELIST > 0
    struct _Py_dict_state *state = get_dict_state(interp);
#ifdef Py_DEBUG
    // new_keys_object() must not be called after _PyDict_Fini()
    assert(state->keys_numfree != -1);
#endif
    if (log2_size == PyDict_LOG_MINSIZE && unicode && state->keys_numfree > 0) {
        dk = state->keys_free_list[--state->keys_numfree];
        OBJECT_STAT_INC(from_freelist);
    }
    else
#endif
    {
        dk = PyObject_Malloc(sizeof(PyDictKeysObject)
                             + ((size_t)1 << log2_bytes)
                             + entry_size * usable);
        if (dk == NULL) {
            PyErr_NoMemory();
            return NULL;
        }
    }
#ifdef Py_REF_DEBUG
    _Py_IncRefTotal(_PyInterpreterState_GET());
#endif
    dk->dk_refcnt = 1;
    dk->dk_log2_size = log2_size;
    dk->dk_log2_index_bytes = log2_bytes;
    dk->dk_kind = unicode ? DICT_KEYS_UNICODE : DICT_KEYS_GENERAL;
    dk->dk_nentries = 0;
    dk->dk_usable = usable;
    dk->dk_version = 0;
    memset(&dk->dk_indices[0], 0xff, ((size_t)1 << log2_bytes));
    memset(&dk->dk_indices[(size_t)1 << log2_bytes], 0, entry_size * usable);
    return dk;
}

static void
free_keys_object(PyInterpreterState *interp, PyDictKeysObject *keys)
{
    assert(keys != Py_EMPTY_KEYS);
    if (DK_IS_UNICODE(keys)) {
        PyDictUnicodeEntry *entries = DK_UNICODE_ENTRIES(keys);
        Py_ssize_t i, n;
        for (i = 0, n = keys->dk_nentries; i < n; i++) {
            Py_XDECREF(entries[i].me_key);
            Py_XDECREF(entries[i].me_value);
        }
    }
    else {
        PyDictKeyEntry *entries = DK_ENTRIES(keys);
        Py_ssize_t i, n;
        for (i = 0, n = keys->dk_nentries; i < n; i++) {
            Py_XDECREF(entries[i].me_key);
            Py_XDECREF(entries[i].me_value);
        }
    }
#if PyDict_MAXFREELIST > 0
    struct _Py_dict_state *state = get_dict_state(interp);
#ifdef Py_DEBUG
    // free_keys_object() must not be called after _PyDict_Fini()
    assert(state->keys_numfree != -1);
#endif
    if (DK_LOG_SIZE(keys) == PyDict_LOG_MINSIZE
            && state->keys_numfree < PyDict_MAXFREELIST
            && DK_IS_UNICODE(keys)) {
        state->keys_free_list[state->keys_numfree++] = keys;
        OBJECT_STAT_INC(to_freelist);
        return;
    }
#endif
    PyObject_Free(keys);
}



static inline void
free_values(PyDictValues *values)
{
    int prefix_size = ((uint8_t *)values)[-1];
    PyMem_Free(((char *)values)-prefix_size);
}

/* Consumes a reference to the keys object */
static PyObject *
new_dict(PyInterpreterState *interp,
         PyDictKeysObject *keys, PyDictValues *values,
         Py_ssize_t used, int free_values_on_failure)
{
    PyDictObject *mp;
    assert(keys != NULL);
#if PyDict_MAXFREELIST > 0
    struct _Py_dict_state *state = get_dict_state(interp);
#ifdef Py_DEBUG
    // new_dict() must not be called after _PyDict_Fini()
    assert(state->numfree != -1);
#endif
    if (state->numfree) {
        mp = state->free_list[--state->numfree];
        assert (mp != NULL);
        assert (Py_IS_TYPE(mp, &PyDict_Type));
        OBJECT_STAT_INC(from_freelist);
        _Py_NewReference((PyObject *)mp);
    }
    else
#endif
    {
        mp = PyObject_GC_New(PyDictObject, &PyDict_Type);
        if (mp == NULL) {
            dictkeys_decref(interp, keys);
            if (free_values_on_failure) {
                free_values(values);
            }
            return NULL;
        }
    }
    mp->ma_keys = keys;
    mp->ma_values = values;
    mp->ma_used = used;
    mp->ma_version_tag = DICT_NEXT_VERSION(interp);
    ASSERT_CONSISTENT(mp);
    return (PyObject *)mp;
}



/* Consumes a reference to the keys object */







/* Search index of hash table from offset of entry table */


// Search non-Unicode key from Unicode table
static Py_ssize_t
unicodekeys_lookup_generic(PyDictObject *mp, PyDictKeysObject* dk, PyObject *key, Py_hash_t hash)
{
    PyDictUnicodeEntry *ep0 = DK_UNICODE_ENTRIES(dk);
    size_t mask = DK_MASK(dk);
    size_t perturb = hash;
    size_t i = (size_t)hash & mask;
    Py_ssize_t ix;
    for (;;) {
        ix = dictkeys_get_index(dk, i);
        if (ix >= 0) {
            PyDictUnicodeEntry *ep = &ep0[ix];
            assert(ep->me_key != NULL);
            assert(PyUnicode_CheckExact(ep->me_key));
            if (ep->me_key == key) {
                return ix;
            }
            if (unicode_get_hash(ep->me_key) == hash) {
                PyObject *startkey = ep->me_key;
                Py_INCREF(startkey);
                int cmp = PyObject_RichCompareBool(startkey, key, Py_EQ);
                Py_DECREF(startkey);
                if (cmp < 0) {
                    return DKIX_ERROR;
                }
                if (dk == mp->ma_keys && ep->me_key == startkey) {
                    if (cmp > 0) {
                        return ix;
                    }
                }
                else {
                    /* The dict was mutated, restart */
                    return DKIX_KEY_CHANGED;
                }
            }
        }
        else if (ix == DKIX_EMPTY) {
            return DKIX_EMPTY;
        }
        perturb >>= PERTURB_SHIFT;
        i = mask & (i*5 + perturb + 1);
    }
    Py_UNREACHABLE();
}

// Search Unicode key from Unicode table.
static Py_ssize_t _Py_HOT_FUNCTION
unicodekeys_lookup_unicode(PyDictKeysObject* dk, PyObject *key, Py_hash_t hash)
{
    PyDictUnicodeEntry *ep0 = DK_UNICODE_ENTRIES(dk);
    size_t mask = DK_MASK(dk);
    size_t perturb = hash;
    size_t i = (size_t)hash & mask;
    Py_ssize_t ix;
    for (;;) {
        ix = dictkeys_get_index(dk, i);
        if (ix >= 0) {
            PyDictUnicodeEntry *ep = &ep0[ix];
            assert(ep->me_key != NULL);
            assert(PyUnicode_CheckExact(ep->me_key));
            if (ep->me_key == key ||
                    (unicode_get_hash(ep->me_key) == hash && unicode_eq(ep->me_key, key))) {
                return ix;
            }
        }
        else if (ix == DKIX_EMPTY) {
            return DKIX_EMPTY;
        }
        perturb >>= PERTURB_SHIFT;
        i = mask & (i*5 + perturb + 1);
        // Manual loop unrolling
        ix = dictkeys_get_index(dk, i);
        if (ix >= 0) {
            PyDictUnicodeEntry *ep = &ep0[ix];
            assert(ep->me_key != NULL);
            assert(PyUnicode_CheckExact(ep->me_key));
            if (ep->me_key == key ||
                    (unicode_get_hash(ep->me_key) == hash && unicode_eq(ep->me_key, key))) {
                return ix;
            }
        }
        else if (ix == DKIX_EMPTY) {
            return DKIX_EMPTY;
        }
        perturb >>= PERTURB_SHIFT;
        i = mask & (i*5 + perturb + 1);
    }
    Py_UNREACHABLE();
}

// Search key from Generic table.
static Py_ssize_t
dictkeys_generic_lookup(PyDictObject *mp, PyDictKeysObject* dk, PyObject *key, Py_hash_t hash)
{
    PyDictKeyEntry *ep0 = DK_ENTRIES(dk);
    size_t mask = DK_MASK(dk);
    size_t perturb = hash;
    size_t i = (size_t)hash & mask;
    Py_ssize_t ix;
    for (;;) {
        ix = dictkeys_get_index(dk, i);
        if (ix >= 0) {
            PyDictKeyEntry *ep = &ep0[ix];
            assert(ep->me_key != NULL);
            if (ep->me_key == key) {
                return ix;
            }
            if (ep->me_hash == hash) {
                PyObject *startkey = ep->me_key;
                Py_INCREF(startkey);
                int cmp = PyObject_RichCompareBool(startkey, key, Py_EQ);
                Py_DECREF(startkey);
                if (cmp < 0) {
                    return DKIX_ERROR;
                }
                if (dk == mp->ma_keys && ep->me_key == startkey) {
                    if (cmp > 0) {
                        return ix;
                    }
                }
                else {
                    /* The dict was mutated, restart */
                    return DKIX_KEY_CHANGED;
                }
            }
        }
        else if (ix == DKIX_EMPTY) {
            return DKIX_EMPTY;
        }
        perturb >>= PERTURB_SHIFT;
        i = mask & (i*5 + perturb + 1);
    }
    Py_UNREACHABLE();
}

/* Lookup a string in a (all unicode) dict keys.
 * Returns DKIX_ERROR if key is not a string,
 * or if the dict keys is not all strings.
 * If the keys is present then return the index of key.
 * If the key is not present then return DKIX_EMPTY.
 */
Py_ssize_t
_PyDictKeys_StringLookup(PyDictKeysObject* dk, PyObject *key)
{
    DictKeysKind kind = dk->dk_kind;
    if (!PyUnicode_CheckExact(key) || kind == DICT_KEYS_GENERAL) {
        return DKIX_ERROR;
    }
    Py_hash_t hash = unicode_get_hash(key);
    if (hash == -1) {
        hash = PyUnicode_Type.tp_hash(key);
        if (hash == -1) {
            PyErr_Clear();
            return DKIX_ERROR;
        }
    }
    return unicodekeys_lookup_unicode(dk, key, hash);
}

/*
The basic lookup function used by all operations.
This is based on Algorithm D from Knuth Vol. 3, Sec. 6.4.
Open addressing is preferred over chaining since the link overhead for
chaining would be substantial (100% with typical malloc overhead).

The initial probe index is computed as hash mod the table size. Subsequent
probe indices are computed as explained earlier.

All arithmetic on hash should ignore overflow.

_Py_dict_lookup() is general-purpose, and may return DKIX_ERROR if (and only if) a
comparison raises an exception.
When the key isn't found a DKIX_EMPTY is returned.
*/
Py_ssize_t
_Py_dict_lookup(PyDictObject *mp, PyObject *key, Py_hash_t hash, PyObject **value_addr)
{
    PyDictKeysObject *dk;
    DictKeysKind kind;
    Py_ssize_t ix;

start:
    dk = mp->ma_keys;
    kind = dk->dk_kind;

    if (kind != DICT_KEYS_GENERAL) {
        if (PyUnicode_CheckExact(key)) {
            ix = unicodekeys_lookup_unicode(dk, key, hash);
        }
        else {
            ix = unicodekeys_lookup_generic(mp, dk, key, hash);
            if (ix == DKIX_KEY_CHANGED) {
                goto start;
            }
        }

        if (ix >= 0) {
            if (kind == DICT_KEYS_SPLIT) {
                *value_addr = mp->ma_values->values[ix];
            }
            else {
                *value_addr = DK_UNICODE_ENTRIES(dk)[ix].me_value;
            }
        }
        else {
            *value_addr = NULL;
        }
    }
    else {
        ix = dictkeys_generic_lookup(mp, dk, key, hash);
        if (ix == DKIX_KEY_CHANGED) {
            goto start;
        }
        if (ix >= 0) {
            *value_addr = DK_ENTRIES(dk)[ix].me_value;
        }
        else {
            *value_addr = NULL;
        }
    }

    return ix;
}



#define MAINTAIN_TRACKING(mp, key, value) \
    do { \
        if (!_PyObject_GC_IS_TRACKED(mp)) { \
            if (_PyObject_GC_MAY_BE_TRACKED(key) || \
                _PyObject_GC_MAY_BE_TRACKED(value)) { \
                _PyObject_GC_TRACK(mp); \
            } \
        } \
    } while(0)



/* Internal function to find slot for an item from its hash
   when it is known that the key is not present in the dict.

   The dict must be combined. */
static Py_ssize_t
find_empty_slot(PyDictKeysObject *keys, Py_hash_t hash)
{
    assert(keys != NULL);

    const size_t mask = DK_MASK(keys);
    size_t i = hash & mask;
    Py_ssize_t ix = dictkeys_get_index(keys, i);
    for (size_t perturb = hash; ix >= 0;) {
        perturb >>= PERTURB_SHIFT;
        i = (i*5 + perturb + 1) & mask;
        ix = dictkeys_get_index(keys, i);
    }
    return i;
}

static int
insertion_resize(PyInterpreterState *interp, PyDictObject *mp, int unicode)
{
    return dictresize(interp, mp, calculate_log2_keysize(GROWTH_RATE(mp)), unicode);
}



/*
Internal routine to insert a new item into the table.
Used both by the internal resize routine and by the public insert routine.
Returns -1 if an error occurred, or 0 on success.
Consumes key and value references.
*/
static int
insertdict(PyInterpreterState *interp, PyDictObject *mp,
           PyObject *key, Py_hash_t hash, PyObject *value)
{
    PyObject *old_value;

    if (DK_IS_UNICODE(mp->ma_keys) && !PyUnicode_CheckExact(key)) {
        if (insertion_resize(interp, mp, 0) < 0)
            goto Fail;
        assert(mp->ma_keys->dk_kind == DICT_KEYS_GENERAL);
    }

    Py_ssize_t ix = _Py_dict_lookup(mp, key, hash, &old_value);
    if (ix == DKIX_ERROR)
        goto Fail;

    MAINTAIN_TRACKING(mp, key, value);

    if (ix == DKIX_EMPTY) {
        assert(old_value == NULL);
        if (mp->ma_keys->dk_usable <= 0) {
            /* Need to resize. */
            if (insertion_resize(interp, mp, 1) < 0)
                goto Fail;
        }

        uint64_t new_version = _PyDict_NotifyEvent(
                interp, PyDict_EVENT_ADDED, mp, key, value);
        /* Insert into new slot. */
        mp->ma_keys->dk_version = 0;

        Py_ssize_t hashpos = find_empty_slot(mp->ma_keys, hash);
        dictkeys_set_index(mp->ma_keys, hashpos, mp->ma_keys->dk_nentries);

        if (DK_IS_UNICODE(mp->ma_keys)) {
            PyDictUnicodeEntry *ep;
            ep = &DK_UNICODE_ENTRIES(mp->ma_keys)[mp->ma_keys->dk_nentries];
            ep->me_key = key;
            if (mp->ma_values) {
                Py_ssize_t index = mp->ma_keys->dk_nentries;
                _PyDictValues_AddToInsertionOrder(mp->ma_values, index);
                assert (mp->ma_values->values[index] == NULL);
                mp->ma_values->values[index] = value;
            }
            else {
                ep->me_value = value;
            }
        }
        else {
            PyDictKeyEntry *ep;
            ep = &DK_ENTRIES(mp->ma_keys)[mp->ma_keys->dk_nentries];
            ep->me_key = key;
            ep->me_hash = hash;
            ep->me_value = value;
        }
        mp->ma_used++;
        mp->ma_version_tag = new_version;
        mp->ma_keys->dk_usable--;
        mp->ma_keys->dk_nentries++;
        assert(mp->ma_keys->dk_usable >= 0);
        ASSERT_CONSISTENT(mp);
        return 0;
    }

    if (old_value != value) {
        uint64_t new_version = _PyDict_NotifyEvent(
                interp, PyDict_EVENT_MODIFIED, mp, key, value);
        if (_PyDict_HasSplitTable(mp)) {
            mp->ma_values->values[ix] = value;
            if (old_value == NULL) {
                _PyDictValues_AddToInsertionOrder(mp->ma_values, ix);
                mp->ma_used++;
            }
        }
        else {
            assert(old_value != NULL);
            if (DK_IS_UNICODE(mp->ma_keys)) {
                DK_UNICODE_ENTRIES(mp->ma_keys)[ix].me_value = value;
            }
            else {
                DK_ENTRIES(mp->ma_keys)[ix].me_value = value;
            }
        }
        mp->ma_version_tag = new_version;
    }
    Py_XDECREF(old_value); /* which **CAN** re-enter (see issue #22653) */
    ASSERT_CONSISTENT(mp);
    Py_DECREF(key);
    return 0;

Fail:
    Py_DECREF(value);
    Py_DECREF(key);
    return -1;
}

// Same to insertdict but specialized for ma_keys = Py_EMPTY_KEYS.
// Consumes key and value references.
static int
insert_to_emptydict(PyInterpreterState *interp, PyDictObject *mp,
                    PyObject *key, Py_hash_t hash, PyObject *value)
{
    assert(mp->ma_keys == Py_EMPTY_KEYS);

    int unicode = PyUnicode_CheckExact(key);
    PyDictKeysObject *newkeys = new_keys_object(
            interp, PyDict_LOG_MINSIZE, unicode);
    if (newkeys == NULL) {
        Py_DECREF(key);
        Py_DECREF(value);
        return -1;
    }
    uint64_t new_version = _PyDict_NotifyEvent(
            interp, PyDict_EVENT_ADDED, mp, key, value);

    /* We don't decref Py_EMPTY_KEYS here because it is immortal. */
    mp->ma_keys = newkeys;
    mp->ma_values = NULL;

    MAINTAIN_TRACKING(mp, key, value);

    size_t hashpos = (size_t)hash & (PyDict_MINSIZE-1);
    dictkeys_set_index(mp->ma_keys, hashpos, 0);
    if (unicode) {
        PyDictUnicodeEntry *ep = DK_UNICODE_ENTRIES(mp->ma_keys);
        ep->me_key = key;
        ep->me_value = value;
    }
    else {
        PyDictKeyEntry *ep = DK_ENTRIES(mp->ma_keys);
        ep->me_key = key;
        ep->me_hash = hash;
        ep->me_value = value;
    }
    mp->ma_used++;
    mp->ma_version_tag = new_version;
    mp->ma_keys->dk_usable--;
    mp->ma_keys->dk_nentries++;
    return 0;
}

/*
Internal routine used by dictresize() to build a hashtable of entries.
*/
static void
build_indices_generic(PyDictKeysObject *keys, PyDictKeyEntry *ep, Py_ssize_t n)
{
    size_t mask = DK_MASK(keys);
    for (Py_ssize_t ix = 0; ix != n; ix++, ep++) {
        Py_hash_t hash = ep->me_hash;
        size_t i = hash & mask;
        for (size_t perturb = hash; dictkeys_get_index(keys, i) != DKIX_EMPTY;) {
            perturb >>= PERTURB_SHIFT;
            i = mask & (i*5 + perturb + 1);
        }
        dictkeys_set_index(keys, i, ix);
    }
}

static void
build_indices_unicode(PyDictKeysObject *keys, PyDictUnicodeEntry *ep, Py_ssize_t n)
{
    size_t mask = DK_MASK(keys);
    for (Py_ssize_t ix = 0; ix != n; ix++, ep++) {
        Py_hash_t hash = unicode_get_hash(ep->me_key);
        assert(hash != -1);
        size_t i = hash & mask;
        for (size_t perturb = hash; dictkeys_get_index(keys, i) != DKIX_EMPTY;) {
            perturb >>= PERTURB_SHIFT;
            i = mask & (i*5 + perturb + 1);
        }
        dictkeys_set_index(keys, i, ix);
    }
}

/*
Restructure the table by allocating a new table and reinserting all
items again.  When entries have been deleted, the new table may
actually be smaller than the old one.
If a table is split (its keys and hashes are shared, its values are not),
then the values are temporarily copied into the table, it is resized as
a combined table, then the me_value slots in the old table are NULLed out.
After resizing a table is always combined.

This function supports:
 - Unicode split -> Unicode combined or Generic
 - Unicode combined -> Unicode combined or Generic
 - Generic -> Generic
*/
static int
dictresize(PyInterpreterState *interp, PyDictObject *mp,
           uint8_t log2_newsize, int unicode)
{
    PyDictKeysObject *oldkeys;
    PyDictValues *oldvalues;

    if (log2_newsize >= SIZEOF_SIZE_T*8) {
        PyErr_NoMemory();
        return -1;
    }
    assert(log2_newsize >= PyDict_LOG_MINSIZE);

    oldkeys = mp->ma_keys;
    oldvalues = mp->ma_values;

    if (!DK_IS_UNICODE(oldkeys)) {
        unicode = 0;
    }

    /* NOTE: Current odict checks mp->ma_keys to detect resize happen.
     * So we can't reuse oldkeys even if oldkeys->dk_size == newsize.
     * TODO: Try reusing oldkeys when reimplement odict.
     */

    /* Allocate a new table. */
    mp->ma_keys = new_keys_object(interp, log2_newsize, unicode);
    if (mp->ma_keys == NULL) {
        mp->ma_keys = oldkeys;
        return -1;
    }
    // New table must be large enough.
    assert(mp->ma_keys->dk_usable >= mp->ma_used);

    Py_ssize_t numentries = mp->ma_used;

    if (oldvalues != NULL) {
         PyDictUnicodeEntry *oldentries = DK_UNICODE_ENTRIES(oldkeys);
        /* Convert split table into new combined table.
         * We must incref keys; we can transfer values.
         */
        if (mp->ma_keys->dk_kind == DICT_KEYS_GENERAL) {
            // split -> generic
            PyDictKeyEntry *newentries = DK_ENTRIES(mp->ma_keys);

            for (Py_ssize_t i = 0; i < numentries; i++) {
                int index = get_index_from_order(mp, i);
                PyDictUnicodeEntry *ep = &oldentries[index];
                assert(oldvalues->values[index] != NULL);
                newentries[i].me_key = Py_NewRef(ep->me_key);
                newentries[i].me_hash = unicode_get_hash(ep->me_key);
                newentries[i].me_value = oldvalues->values[index];
            }
            build_indices_generic(mp->ma_keys, newentries, numentries);
        }
        else { // split -> combined unicode
            PyDictUnicodeEntry *newentries = DK_UNICODE_ENTRIES(mp->ma_keys);

            for (Py_ssize_t i = 0; i < numentries; i++) {
                int index = get_index_from_order(mp, i);
                PyDictUnicodeEntry *ep = &oldentries[index];
                assert(oldvalues->values[index] != NULL);
                newentries[i].me_key = Py_NewRef(ep->me_key);
                newentries[i].me_value = oldvalues->values[index];
            }
            build_indices_unicode(mp->ma_keys, newentries, numentries);
        }
        dictkeys_decref(interp, oldkeys);
        mp->ma_values = NULL;
        free_values(oldvalues);
    }
    else {  // oldkeys is combined.
        if (oldkeys->dk_kind == DICT_KEYS_GENERAL) {
            // generic -> generic
            assert(mp->ma_keys->dk_kind == DICT_KEYS_GENERAL);
            PyDictKeyEntry *oldentries = DK_ENTRIES(oldkeys);
            PyDictKeyEntry *newentries = DK_ENTRIES(mp->ma_keys);
            if (oldkeys->dk_nentries == numentries) {
                memcpy(newentries, oldentries, numentries * sizeof(PyDictKeyEntry));
            }
            else {
                PyDictKeyEntry *ep = oldentries;
                for (Py_ssize_t i = 0; i < numentries; i++) {
                    while (ep->me_value == NULL)
                        ep++;
                    newentries[i] = *ep++;
                }
            }
            build_indices_generic(mp->ma_keys, newentries, numentries);
        }
        else {  // oldkeys is combined unicode
            PyDictUnicodeEntry *oldentries = DK_UNICODE_ENTRIES(oldkeys);
            if (unicode) { // combined unicode -> combined unicode
                PyDictUnicodeEntry *newentries = DK_UNICODE_ENTRIES(mp->ma_keys);
                if (oldkeys->dk_nentries == numentries && mp->ma_keys->dk_kind == DICT_KEYS_UNICODE) {
                    memcpy(newentries, oldentries, numentries * sizeof(PyDictUnicodeEntry));
                }
                else {
                    PyDictUnicodeEntry *ep = oldentries;
                    for (Py_ssize_t i = 0; i < numentries; i++) {
                        while (ep->me_value == NULL)
                            ep++;
                        newentries[i] = *ep++;
                    }
                }
                build_indices_unicode(mp->ma_keys, newentries, numentries);
            }
            else { // combined unicode -> generic
                PyDictKeyEntry *newentries = DK_ENTRIES(mp->ma_keys);
                PyDictUnicodeEntry *ep = oldentries;
                for (Py_ssize_t i = 0; i < numentries; i++) {
                    while (ep->me_value == NULL)
                        ep++;
                    newentries[i].me_key = ep->me_key;
                    newentries[i].me_hash = unicode_get_hash(ep->me_key);
                    newentries[i].me_value = ep->me_value;
                    ep++;
                }
                build_indices_generic(mp->ma_keys, newentries, numentries);
            }
        }

        // We can not use free_keys_object here because key's reference
        // are moved already.
        if (oldkeys != Py_EMPTY_KEYS) {
#ifdef Py_REF_DEBUG
            _Py_DecRefTotal(_PyInterpreterState_GET());
#endif
            assert(oldkeys->dk_kind != DICT_KEYS_SPLIT);
            assert(oldkeys->dk_refcnt == 1);
#if PyDict_MAXFREELIST > 0
            struct _Py_dict_state *state = get_dict_state(interp);
#ifdef Py_DEBUG
            // dictresize() must not be called after _PyDict_Fini()
            assert(state->keys_numfree != -1);
#endif
            if (DK_LOG_SIZE(oldkeys) == PyDict_LOG_MINSIZE &&
                    DK_IS_UNICODE(oldkeys) &&
                    state->keys_numfree < PyDict_MAXFREELIST)
            {
                state->keys_free_list[state->keys_numfree++] = oldkeys;
                OBJECT_STAT_INC(to_freelist);
            }
            else
#endif
            {
                PyObject_Free(oldkeys);
            }
        }
    }

    mp->ma_keys->dk_usable -= numentries;
    mp->ma_keys->dk_nentries = numentries;
    ASSERT_CONSISTENT(mp);
    return 0;
}

static PyObject *
dict_new_presized(PyInterpreterState *interp, Py_ssize_t minused, bool unicode)
{
    const uint8_t log2_max_presize = 17;
    const Py_ssize_t max_presize = ((Py_ssize_t)1) << log2_max_presize;
    uint8_t log2_newsize;
    PyDictKeysObject *new_keys;

    if (minused <= USABLE_FRACTION(PyDict_MINSIZE)) {
        return PyDict_New();
    }
    /* There are no strict guarantee that returned dict can contain minused
     * items without resize.  So we create medium size dict instead of very
     * large dict or MemoryError.
     */
    if (minused > USABLE_FRACTION(max_presize)) {
        log2_newsize = log2_max_presize;
    }
    else {
        log2_newsize = estimate_log2_keysize(minused);
    }

    new_keys = new_keys_object(interp, log2_newsize, unicode);
    if (new_keys == NULL)
        return NULL;
    return new_dict(interp, new_keys, NULL, 0, 0);
}



PyObject *
_PyDict_FromItems(PyObject *const *keys, Py_ssize_t keys_offset,
                  PyObject *const *values, Py_ssize_t values_offset,
                  Py_ssize_t length)
{
    bool unicode = true;
    PyObject *const *ks = keys;
    PyInterpreterState *interp = _PyInterpreterState_GET();

    for (Py_ssize_t i = 0; i < length; i++) {
        if (!PyUnicode_CheckExact(*ks)) {
            unicode = false;
            break;
        }
        ks += keys_offset;
    }

    PyObject *dict = dict_new_presized(interp, length, unicode);
    if (dict == NULL) {
        return NULL;
    }

    ks = keys;
    PyObject *const *vs = values;

    for (Py_ssize_t i = 0; i < length; i++) {
        PyObject *key = *ks;
        PyObject *value = *vs;
        if (PyDict_SetItem(dict, key, value) < 0) {
            Py_DECREF(dict);
            return NULL;
        }
        ks += keys_offset;
        vs += values_offset;
    }

    return dict;
}

/* Note that, for historical reasons, PyDict_GetItem() suppresses all errors
 * that may occur (originally dicts supported only string keys, and exceptions
 * weren't possible).  So, while the original intent was that a NULL return
 * meant the key wasn't present, in reality it can mean that, or that an error
 * (suppressed) occurred while computing the key's hash, or that some error
 * (suppressed) occurred when comparing keys in the dict's internal probe
 * sequence.  A nasty example of the latter is when a Python-coded comparison
 * function hits a stack-depth error, which can cause this to return NULL
 * even if the key is present.
 */


Py_ssize_t
_PyDict_LookupIndex(PyDictObject *mp, PyObject *key)
{
    PyObject *value;
    assert(PyDict_CheckExact((PyObject*)mp));
    assert(PyUnicode_CheckExact(key));

    Py_hash_t hash = unicode_get_hash(key);
    if (hash == -1) {
        hash = PyObject_Hash(key);
        if (hash == -1) {
            return -1;
        }
    }

    return _Py_dict_lookup(mp, key, hash, &value);
}

/* Same as PyDict_GetItemWithError() but with hash supplied by caller.
   This returns NULL *with* an exception set if an exception occurred.
   It returns NULL *without* an exception set if the key wasn't present.
*/


/* Variant of PyDict_GetItem() that doesn't suppress exceptions.
   This returns NULL *with* an exception set if an exception occurred.
   It returns NULL *without* an exception set if the key wasn't present.
*/








/* Fast version of global value lookup (LOAD_GLOBAL).
 * Lookup in globals, then builtins.
 *
 *
 *
 *
 * Raise an exception and return NULL if an error occurred (ex: computing the
 * key hash failed, key comparison failed, ...). Return NULL if the key doesn't
 * exist. Return the value if the key exists.
 */
PyObject *
_PyDict_LoadGlobal(PyDictObject *globals, PyDictObject *builtins, PyObject *key)
{
    Py_ssize_t ix;
    Py_hash_t hash;
    PyObject *value;

    if (!PyUnicode_CheckExact(key) || (hash = unicode_get_hash(key)) == -1) {
        hash = PyObject_Hash(key);
        if (hash == -1)
            return NULL;
    }

    /* namespace 1: globals */
    ix = _Py_dict_lookup(globals, key, hash, &value);
    if (ix == DKIX_ERROR)
        return NULL;
    if (ix != DKIX_EMPTY && value != NULL)
        return value;

    /* namespace 2: builtins */
    ix = _Py_dict_lookup(builtins, key, hash, &value);
    assert(ix >= 0 || value == NULL);
    return value;
}

/* Consumes references to key and value */
int
_PyDict_SetItem_Take2(PyDictObject *mp, PyObject *key, PyObject *value)
{
    assert(key);
    assert(value);
    assert(PyDict_Check(mp));
    Py_hash_t hash;
    if (!PyUnicode_CheckExact(key) || (hash = unicode_get_hash(key)) == -1) {
        hash = PyObject_Hash(key);
        if (hash == -1) {
            Py_DECREF(key);
            Py_DECREF(value);
            return -1;
        }
    }
    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (mp->ma_keys == Py_EMPTY_KEYS) {
        return insert_to_emptydict(interp, mp, key, hash, value);
    }
    /* insertdict() handles any resizing that might be necessary */
    return insertdict(interp, mp, key, hash, value);
}

/* CAUTION: PyDict_SetItem() must guarantee that it won't resize the
 * dictionary if it's merely replacing the value for an existing key.
 * This means that it's safe to loop over a dictionary with PyDict_Next()
 * and occasionally replace a value -- but you can't insert new keys or
 * remove them.
 */












/* This function promises that the predicate -> deletion sequence is atomic
 * (i.e. protected by the GIL), assuming the predicate itself doesn't
 * release the GIL.
 */





/* Internal version of PyDict_Next that returns a hash value in addition
 * to the key and value.
 * Return 1 on success, return 0 when the reached the end of the dictionary
 * (or if op is not a dictionary)
 */


/*
 * Iterate over a dict.  Use like so:
 *
 *     Py_ssize_t i;
 *     PyObject *key, *value;
 *     i = 0;   # important!  i should not otherwise be changed by you
 *     while (PyDict_Next(yourdict, &i, &key, &value)) {
 *         Refer to borrowed references in key and value.
 *     }
 *
 * Return 1 on success, return 0 when the reached the end of the dictionary
 * (or if op is not a dictionary)
 *
 * CAUTION:  In general, it isn't safe to use PyDict_Next in a loop that
 * mutates the dict.  One exception:  it is safe if the loop merely changes
 * the values associated with the keys (but doesn't insert new keys or
 * delete keys), via PyDict_SetItem().
 */


/* Internal version of dict.pop(). */




/* Internal version of dict.from_keys().  It is subclass-friendly. */


/* Methods */












;







/*[clinic input]
@classmethod
dict.fromkeys
    iterable: object
    value: object=None
    /

Create a new dictionary with keys from iterable and values set to value.
[clinic start generated code]*/



/* Single-arg dict update; used by dict_update_common and operators. */




/* Note: dict.update() uses the METH_VARARGS|METH_KEYWORDS calling convention.
   Using METH_FASTCALL|METH_KEYWORDS would make dict.update(**dict2) calls
   slower, see the issue #29312. */


/* Update unconditionally replaces existing items.
   Merge has a 3rd argument 'override'; if set, it acts like Update,
   otherwise it leaves existing items unchanged.

   PyDict_{Update,Merge} update/merge from a mapping object.

   PyDict_MergeFromSeq2 updates/merges from any iterable object
   producing iterable objects of length 2.
*/























/* Return 1 if dicts equal, 0 if not, -1 if error.
 * Gets out as soon as any difference is detected.
 * Uses only Py_EQ comparison.
 */




/*[clinic input]

@coexist
dict.__contains__

  key: object
  /

True if the dictionary has the specified key, else False.
[clinic start generated code]*/



/*[clinic input]
dict.get

    key: object
    default: object = None
    /

Return the value for key if key is in the dictionary, else default.
[clinic start generated code]*/





/*[clinic input]
dict.setdefault

    key: object
    default: object = None
    /

Insert key with a value of default if key is not in the dictionary.

Return the value for key if key is in the dictionary, else default.
[clinic start generated code]*/





/*[clinic input]
dict.pop

    key: object
    default: object = NULL
    /

D.pop(k[,d]) -> v, remove specified key and return the corresponding value.

If the key is not found, return the default if given; otherwise,
raise a KeyError.
[clinic start generated code]*/



/*[clinic input]
dict.popitem

Remove and return a (key, value) pair as a 2-tuple.

Pairs are returned in LIFO (last-in, first-out) order.
Raises KeyError if the dict is empty.
[clinic start generated code]*/







static PyObject *dictiter_new(PyDictObject *, PyTypeObject *);











PyDoc_STRVAR(getitem__doc__,
"__getitem__($self, key, /)\n--\n\nReturn self[key].");

PyDoc_STRVAR(sizeof__doc__,
"D.__sizeof__() -> size of D in memory, in bytes");

PyDoc_STRVAR(update__doc__,
"D.update([E, ]**F) -> None.  Update D from mapping/iterable E and F.\n\
If E is present and has a .keys() method, then does:  for k in E.keys(): D[k] = E[k]\n\
If E is present and lacks a .keys() method, then does:  for k, v in E: D[k] = v\n\
In either case, this is followed by: for k in F:  D[k] = F[k]");

PyDoc_STRVAR(clear__doc__,
"D.clear() -> None.  Remove all items from D.");

PyDoc_STRVAR(copy__doc__,
"D.copy() -> a shallow copy of D");

/* Forward */
static PyObject *dictkeys_new(PyObject *, PyObject *);
static PyObject *dictitems_new(PyObject *, PyObject *);
static PyObject *dictvalues_new(PyObject *, PyObject *);

PyDoc_STRVAR(keys__doc__,
             "D.keys() -> a set-like object providing a view on D's keys");
PyDoc_STRVAR(items__doc__,
             "D.items() -> a set-like object providing a view on D's items");
PyDoc_STRVAR(values__doc__,
             "D.values() -> an object providing a view on D's values");

;

/* Return 1 if `key` is in dict `op`, 0 if not, and -1 on error. */


/* Internal version of PyDict_Contains used when the hash value is already known */




/* Hack to implement "key in dict" */
;

;









PyDoc_STRVAR(dictionary_doc,
"dict() -> new empty dictionary\n"
"dict(mapping) -> new dictionary initialized from a mapping object's\n"
"    (key, value) pairs\n"
"dict(iterable) -> new dictionary initialized as if via:\n"
"    d = {}\n"
"    for k, v in iterable:\n"
"        d[k] = v\n"
"dict(**kwargs) -> new dictionary initialized with the name=value pairs\n"
"    in the keyword argument list.  For example:  dict(one=1, two=2)");

;

/* For backward compatibility with old dictionary interface */











/* Dictionary iterator types */

typedef struct {
    PyObject_HEAD
    PyDictObject *di_dict; /* Set to NULL when iterator is exhausted */
    Py_ssize_t di_used;
    Py_ssize_t di_pos;
    PyObject* di_result; /* reusable result tuple for iteritems */
    Py_ssize_t len;
} dictiterobject;









PyDoc_STRVAR(length_hint_doc,
             "Private method returning an estimate of len(list(it)).");

static PyObject *
dictiter_reduce(dictiterobject *di, PyObject *Py_UNUSED(ignored));

PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");

;



;



;



;


/* dictreviter */



;


/*[clinic input]
dict.__reversed__

Return a reverse iterator over the dict keys.
[clinic start generated code]*/





;

;

/***********************************************/
/* View objects for keys(), items(), values(). */
/***********************************************/

/* The instance lay-out is the same for all three; but the type differs. */











;

/* TODO(guido): The views objects are not complete:

 * support more set operations
 * support arbitrary mappings?
   - either these should be static or exported in dictobject.h
   - if public then they should probably be in builtins
*/

/* Return 1 if self is a subset of other, iterating over self;
   0 if not; -1 if an error occurred. */






/*** dict_keys ***/





;

// Create a set object from dictviews object.
// Returns a new reference.
// This utility function is used by set operations.




static int
dictitems_contains(_PyDictViewObject *dv, PyObject *obj);









;



PyDoc_STRVAR(isdisjoint_doc,
"Return True if the view and the given iterable have a null intersection.");

static PyObject* dictkeys_reversed(_PyDictViewObject *dv, PyObject *Py_UNUSED(ignored));

PyDoc_STRVAR(reversed_keys_doc,
"Return a reverse iterator over the dict keys.");

;

;





/*** dict_items ***/





;

static PyObject* dictitems_reversed(_PyDictViewObject *dv, PyObject *Py_UNUSED(ignored));

PyDoc_STRVAR(reversed_items_doc,
"Return a reverse iterator over the dict items.");

;

;





/*** dict_values ***/



;

static PyObject* dictvalues_reversed(_PyDictViewObject *dv, PyObject *Py_UNUSED(ignored));

PyDoc_STRVAR(reversed_values_doc,
"Return a reverse iterator over the dict values.");

;

;






/* Returns NULL if cannot allocate a new PyDictKeysObject,
   but does not set an error */


#define CACHED_KEYS(tp) (((PyHeapTypeObject*)tp)->ht_cached_keys)











/* Sanity check for managed dicts */
#if 0
#define CHECK(val) assert(val); if (!(val)) { return 0; }

int
_PyObject_ManagedDictValidityCheck(PyObject *obj)
{
    PyTypeObject *tp = Py_TYPE(obj);
    CHECK(tp->tp_flags & Py_TPFLAGS_MANAGED_DICT);
    PyDictOrValues *dorv_ptr = _PyObject_DictOrValuesPointer(obj);
    if (_PyDictOrValues_IsValues(*dorv_ptr)) {
        PyDictValues *values = _PyDictOrValues_GetValues(*dorv_ptr);
        int size = ((uint8_t *)values)[-2];
        int count = 0;
        PyDictKeysObject *keys = CACHED_KEYS(tp);
        for (Py_ssize_t i = 0; i < keys->dk_nentries; i++) {
            if (values->values[i] != NULL) {
                count++;
            }
        }
        CHECK(size == count);
    }
    else {
        if (dorv_ptr->dict != NULL) {
            CHECK(PyDict_Check(dorv_ptr->dict));
        }
    }
    return 1;
}
#endif

















uint32_t _PyDictKeys_GetVersionForCurrentState(PyInterpreterState *interp,
                                               PyDictKeysObject *dictkeys)
{
    if (dictkeys->dk_version != 0) {
        return dictkeys->dk_version;
    }
    if (interp->dict_state.next_keys_version == 0) {
        return 0;
    }
    uint32_t v = interp->dict_state.next_keys_version++;
    dictkeys->dk_version = v;
    return v;
}











static const char *
dict_event_name(PyDict_WatchEvent event) {
    switch (event) {
        #define CASE(op)                \
        case PyDict_EVENT_##op:         \
            return "PyDict_EVENT_" #op;
        PY_FOREACH_DICT_EVENT(CASE)
        #undef CASE
    }
    Py_UNREACHABLE();
}

void
_PyDict_SendEvent(int watcher_bits,
                  PyDict_WatchEvent event,
                  PyDictObject *mp,
                  PyObject *key,
                  PyObject *value)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    for (int i = 0; i < DICT_MAX_WATCHERS; i++) {
        if (watcher_bits & 1) {
            PyDict_WatchCallback cb = interp->dict_state.watchers[i];
            if (cb && (cb(event, (PyObject*)mp, key, value) < 0)) {
                // We don't want to resurrect the dict by potentially having an
                // unraisablehook keep a reference to it, so we don't pass the
                // dict as context, just an informative string message.  Dict
                // repr can call arbitrary code, so we invent a simpler version.
                PyObject *context = PyUnicode_FromFormat(
                    "%s watcher callback for <dict at %p>",
                    dict_event_name(event), mp);
                if (context == NULL) {
                    context = Py_NewRef(Py_None);
                }
                PyErr_WriteUnraisable(context);
                Py_DECREF(context);
            }
        }
        watcher_bits >>= 1;
    }
}
