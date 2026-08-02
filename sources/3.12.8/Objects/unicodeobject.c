/*

Unicode implementation based on original code by Fredrik Lundh,
modified by Marc-Andre Lemburg <mal@lemburg.com>.

Major speed upgrades to the method implementations at the Reykjavik
NeedForSpeed sprint, by Fredrik Lundh and Andrew Dalke.

Copyright (c) Corporation for National Research Initiatives.

--------------------------------------------------------------------
The original string type implementation is:

  Copyright (c) 1999 by Secret Labs AB
  Copyright (c) 1999 by Fredrik Lundh

By obtaining, using, and/or copying this software and/or its
associated documentation, you agree that you have read, understood,
and will comply with the following terms and conditions:

Permission to use, copy, modify, and distribute this software and its
associated documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies, and that both that copyright notice and this permission notice
appear in supporting documentation, and that the name of Secret Labs
AB or the author not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

SECRET LABS AB AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS.  IN NO EVENT SHALL SECRET LABS AB OR THE AUTHOR BE LIABLE FOR
ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
--------------------------------------------------------------------

*/

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "pycore_abstract.h"      // _PyIndex_Check()
#include "pycore_atomic_funcs.h"  // _Py_atomic_size_get()
#include "pycore_bytesobject.h"   // _PyBytes_Repeat()
#include "pycore_bytes_methods.h" // _Py_bytes_lower()
#include "pycore_format.h"        // F_LJUST
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_interp.h"        // PyInterpreterState.fs_codec
#include "pycore_long.h"          // _PyLong_FormatWriter()
#include "pycore_object.h"        // _PyObject_GC_TRACK(), _Py_FatalRefcountError()
#include "pycore_pathconfig.h"    // _Py_DumpPathConfig()
#include "pycore_pylifecycle.h"   // _Py_SetFileSystemEncoding()
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_ucnhash.h"       // _PyUnicode_Name_CAPI
#include "pycore_unicodeobject.h" // struct _Py_unicode_state
#include "pycore_unicodeobject_generated.h"  // _PyUnicode_InitStaticStrings()
#include "stringlib/eq.h"         // unicode_eq()
#include <stddef.h>               // ptrdiff_t

#ifdef MS_WINDOWS
#include <windows.h>
#endif

#ifdef HAVE_NON_UNICODE_WCHAR_T_REPRESENTATION
#  include "pycore_fileutils.h"   // _Py_LocaleUsesNonUnicodeWchar()
#endif

/* Uncomment to display statistics on interned strings at exit
   in _PyUnicode_ClearInterned(). */
/* #define INTERNED_STATS 1 */


/*[clinic input]
class str "PyObject *" "&PyUnicode_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=4884c934de622cf6]*/

/*[python input]
class Py_UCS4_converter(CConverter):
    type = 'Py_UCS4'
    converter = 'convert_uc'

    def converter_init(self):
        if self.default is not unspecified:
            self.c_default = ascii(self.default)
            if len(self.c_default) > 4 or self.c_default[0] != "'":
                self.c_default = hex(ord(self.default))

[python start generated code]*/
/*[python end generated code: output=da39a3ee5e6b4b0d input=88f5dd06cd8e7a61]*/

/* --- Globals ------------------------------------------------------------

NOTE: In the interpreter's initialization phase, some globals are currently
      initialized dynamically as needed. In the process Unicode objects may
      be created before the Unicode type is ready.

*/


#ifdef __cplusplus
extern "C" {
#endif

// Maximum code point of Unicode 6.0: 0x10ffff (1,114,111).
// The value must be the same in fileutils.c.
#define MAX_UNICODE 0x10ffff

#ifdef Py_DEBUG
#  define _PyUnicode_CHECK(op) _PyUnicode_CheckConsistency(op, 0)
#else
#  define _PyUnicode_CHECK(op) PyUnicode_Check(op)
#endif

#define _PyUnicode_UTF8(op)                             \
    (_PyCompactUnicodeObject_CAST(op)->utf8)
#define PyUnicode_UTF8(op)                              \
    (assert(_PyUnicode_CHECK(op)),                      \
     PyUnicode_IS_COMPACT_ASCII(op) ?                   \
         ((char*)(_PyASCIIObject_CAST(op) + 1)) :       \
         _PyUnicode_UTF8(op))
#define _PyUnicode_UTF8_LENGTH(op)                      \
    (_PyCompactUnicodeObject_CAST(op)->utf8_length)
#define PyUnicode_UTF8_LENGTH(op)                       \
    (assert(_PyUnicode_CHECK(op)),                      \
     PyUnicode_IS_COMPACT_ASCII(op) ?                   \
         _PyASCIIObject_CAST(op)->length :              \
         _PyUnicode_UTF8_LENGTH(op))

#define _PyUnicode_LENGTH(op)                           \
    (_PyASCIIObject_CAST(op)->length)
#define _PyUnicode_STATE(op)                            \
    (_PyASCIIObject_CAST(op)->state)
#define _PyUnicode_HASH(op)                             \
    (_PyASCIIObject_CAST(op)->hash)
#define _PyUnicode_KIND(op)                             \
    (assert(_PyUnicode_CHECK(op)),                      \
     _PyASCIIObject_CAST(op)->state.kind)
#define _PyUnicode_GET_LENGTH(op)                       \
    (assert(_PyUnicode_CHECK(op)),                      \
     _PyASCIIObject_CAST(op)->length)
#define _PyUnicode_DATA_ANY(op)                         \
    (_PyUnicodeObject_CAST(op)->data.any)

#define _PyUnicode_SHARE_UTF8(op)                       \
    (assert(_PyUnicode_CHECK(op)),                      \
     assert(!PyUnicode_IS_COMPACT_ASCII(op)),           \
     (_PyUnicode_UTF8(op) == PyUnicode_DATA(op)))

/* true if the Unicode object has an allocated UTF-8 memory block
   (not shared with other data) */
#define _PyUnicode_HAS_UTF8_MEMORY(op)                  \
    ((!PyUnicode_IS_COMPACT_ASCII(op)                   \
      && _PyUnicode_UTF8(op)                            \
      && _PyUnicode_UTF8(op) != PyUnicode_DATA(op)))

/* Generic helper macro to convert characters of different types.
   from_type and to_type have to be valid type names, begin and end
   are pointers to the source characters which should be of type
   "from_type *".  to is a pointer of type "to_type *" and points to the
   buffer where the result characters are written to. */
#define _PyUnicode_CONVERT_BYTES(from_type, to_type, begin, end, to) \
    do {                                                \
        to_type *_to = (to_type *)(to);                 \
        const from_type *_iter = (const from_type *)(begin);\
        const from_type *_end = (const from_type *)(end);\
        Py_ssize_t n = (_end) - (_iter);                \
        const from_type *_unrolled_end =                \
            _iter + _Py_SIZE_ROUND_DOWN(n, 4);          \
        while (_iter < (_unrolled_end)) {               \
            _to[0] = (to_type) _iter[0];                \
            _to[1] = (to_type) _iter[1];                \
            _to[2] = (to_type) _iter[2];                \
            _to[3] = (to_type) _iter[3];                \
            _iter += 4; _to += 4;                       \
        }                                               \
        while (_iter < (_end))                          \
            *_to++ = (to_type) *_iter++;                \
    } while (0)

#define LATIN1 _Py_LATIN1_CHR

#ifdef MS_WINDOWS
   /* On Windows, overallocate by 50% is the best factor */
#  define OVERALLOCATE_FACTOR 2
#else
   /* On Linux, overallocate by 25% is the best factor */
#  define OVERALLOCATE_FACTOR 4
#endif

/* Forward declaration */
static inline int
_PyUnicodeWriter_WriteCharInline(_PyUnicodeWriter *writer, Py_UCS4 ch);
static inline void
_PyUnicodeWriter_InitWithBuffer(_PyUnicodeWriter *writer, PyObject *buffer);
static PyObject *
unicode_encode_utf8(PyObject *unicode, _Py_error_handler error_handler,
                    const char *errors);
static PyObject *
unicode_decode_utf8(const char *s, Py_ssize_t size,
                    _Py_error_handler error_handler, const char *errors,
                    Py_ssize_t *consumed);
#ifdef Py_DEBUG
static inline int unicode_is_finalizing(void);
static int unicode_is_singleton(PyObject *unicode);
#endif


// Return a borrowed reference to the empty string singleton.



// Return a strong reference to the empty string singleton.


/* This dictionary holds per-interpreter interned strings.
 * See InternalDocs/string_interning.md for details.
 */
static inline PyObject *get_interned_dict(PyInterpreterState *interp)
{
    return _Py_INTERP_CACHED_OBJECT(interp, interned_strings);
}

/* This hashtable holds statically allocated interned strings.
 * See InternalDocs/string_interning.md for details.
 */
#define INTERNED_STRINGS _PyRuntime.cached_objects.interned_strings

/* Get number of all interned strings for the current interpreter. */


/* Get number of immortal interned strings for the current interpreter. */


static Py_hash_t unicode_hash(PyObject *);
static int unicode_compare_eq(PyObject *, PyObject *);





/* Return true if this interpreter should share the main interpreter's
   intern_dict.  That's important for interpreters which load basic
   single-phase init extension modules (m_size == -1).  There could be interned
   immortal strings that are shared between interpreters, due to the
   PyDict_Update(mdict, m_copy) call in import_find_extension().

   It's not safe to deallocate those strings until all interpreters that
   potentially use them are freed.  By storing them in the main interpreter, we
   ensure they get freed after all other interpreters are freed.
*/










#define _Py_RETURN_UNICODE_EMPTY()   \
    do {                             \
        return unicode_new_empty();  \
    } while (0)




/* Fast detection of the most frequent whitespace characters */
;

/* forward */
static PyObject* get_latin1_char(unsigned char ch);
static int unicode_modifiable(PyObject *unicode);


static PyObject *
_PyUnicode_FromUCS1(const Py_UCS1 *s, Py_ssize_t size);
static PyObject *
_PyUnicode_FromUCS2(const Py_UCS2 *s, Py_ssize_t size);
static PyObject *
_PyUnicode_FromUCS4(const Py_UCS4 *s, Py_ssize_t size);

static PyObject *
unicode_encode_call_errorhandler(const char *errors,
       PyObject **errorHandler,const char *encoding, const char *reason,
       PyObject *unicode, PyObject **exceptionObject,
       Py_ssize_t startpos, Py_ssize_t endpos, Py_ssize_t *newpos);

static void
raise_encode_exception(PyObject **exceptionObject,
                       const char *encoding,
                       PyObject *unicode,
                       Py_ssize_t startpos, Py_ssize_t endpos,
                       const char *reason);

/* Same for linebreaks */
;

static int convert_uc(PyObject *obj, void *addr);

struct encoding_map;
#include "clinic/unicodeobject.c.h"
















/* Implementation of the "backslashreplace" error handler for 8-bit encodings:
   ASCII, Latin1, UTF-8, etc. */


/* Implementation of the "xmlcharrefreplace" error handler for 8-bit encodings:
   ASCII, Latin1, UTF-8, etc. */


/* --- Bloom Filters ----------------------------------------------------- */

/* stuff to implement simple "bloom filters" for Unicode characters.
   to keep things simple, we use a single bitmask, using the least 5
   bits from each unicode characters as the bit index. */

/* the linebreak mask is set up by _PyUnicode_Init() below */

#if LONG_BIT >= 128
#define BLOOM_WIDTH 128
#elif LONG_BIT >= 64
#define BLOOM_WIDTH 64
#elif LONG_BIT >= 32
#define BLOOM_WIDTH 32
#else
#error "LONG_BIT is smaller than 32"
#endif

#define BLOOM_MASK unsigned long

;

#define BLOOM(mask, ch)     ((mask &  (1UL << ((ch) & (BLOOM_WIDTH - 1)))))

#define BLOOM_LINEBREAK(ch)                                             \
    ((ch) < 128U ? ascii_linebreak[(ch)] :                              \
     (BLOOM(bloom_linebreak, (ch)) && Py_UNICODE_ISLINEBREAK(ch)))





/* Compilation of templated routines */

#define STRINGLIB_GET_EMPTY() unicode_get_empty()

#include "stringlib/asciilib.h"
#include "stringlib/fastsearch.h"
#include "stringlib/partition.h"
#include "stringlib/split.h"
#include "stringlib/count.h"
#include "stringlib/find.h"
#include "stringlib/find_max_char.h"
#include "stringlib/undef.h"

#include "stringlib/ucs1lib.h"
#include "stringlib/fastsearch.h"
#include "stringlib/partition.h"
#include "stringlib/split.h"
#include "stringlib/count.h"
#include "stringlib/find.h"
#include "stringlib/replace.h"
#include "stringlib/find_max_char.h"
#include "stringlib/undef.h"

#include "stringlib/ucs2lib.h"
#include "stringlib/fastsearch.h"
#include "stringlib/partition.h"
#include "stringlib/split.h"
#include "stringlib/count.h"
#include "stringlib/find.h"
#include "stringlib/replace.h"
#include "stringlib/find_max_char.h"
#include "stringlib/undef.h"

#include "stringlib/ucs4lib.h"
#include "stringlib/fastsearch.h"
#include "stringlib/partition.h"
#include "stringlib/split.h"
#include "stringlib/count.h"
#include "stringlib/find.h"
#include "stringlib/replace.h"
#include "stringlib/find_max_char.h"
#include "stringlib/undef.h"

#undef STRINGLIB_GET_EMPTY

/* --- Unicode Object ----------------------------------------------------- */



#ifdef Py_DEBUG
/* Fill the data of a Unicode string with invalid characters to detect bugs
   earlier.

   _PyUnicode_CheckConsistency(str, 1) detects invalid characters, at least for
   ASCII and UCS-4 strings. U+00FF is invalid in ASCII and U+FFFFFFFF is an
   invalid character in Unicode 6.0. */
static void
unicode_fill_invalid(PyObject *unicode, Py_ssize_t old_length)
{
    int kind = PyUnicode_KIND(unicode);
    Py_UCS1 *data = PyUnicode_1BYTE_DATA(unicode);
    Py_ssize_t length = _PyUnicode_LENGTH(unicode);
    if (length <= old_length)
        return;
    memset(data + old_length * kind, 0xff, (length - old_length) * kind);
}
#endif









#ifdef Py_DEBUG
/* Functions wrapping macros for use in debugger */
const char *_PyUnicode_utf8(void *unicode_raw){
    PyObject *unicode = _PyObject_CAST(unicode_raw);
    return PyUnicode_UTF8(unicode);
}

const void *_PyUnicode_compact_data(void *unicode_raw) {
    PyObject *unicode = _PyObject_CAST(unicode_raw);
    return _PyUnicode_COMPACT_DATA(unicode);
}
const void *_PyUnicode_data(void *unicode_raw) {
    PyObject *unicode = _PyObject_CAST(unicode_raw);
    printf("obj %p\n", (void*)unicode);
    printf("compact %d\n", PyUnicode_IS_COMPACT(unicode));
    printf("compact ascii %d\n", PyUnicode_IS_COMPACT_ASCII(unicode));
    printf("ascii op %p\n", (void*)(_PyASCIIObject_CAST(unicode) + 1));
    printf("compact op %p\n", (void*)(_PyCompactUnicodeObject_CAST(unicode) + 1));
    printf("compact data %p\n", _PyUnicode_COMPACT_DATA(unicode));
    return PyUnicode_DATA(unicode);
}

void
_PyUnicode_Dump(PyObject *op)
{
    PyASCIIObject *ascii = _PyASCIIObject_CAST(op);
    PyCompactUnicodeObject *compact = _PyCompactUnicodeObject_CAST(op);
    PyUnicodeObject *unicode = _PyUnicodeObject_CAST(op);
    const void *data;

    if (ascii->state.compact)
    {
        if (ascii->state.ascii)
            data = (ascii + 1);
        else
            data = (compact + 1);
    }
    else
        data = unicode->data.any;
    printf("%s: len=%zu, ", unicode_kind_name(op), ascii->length);

    if (!ascii->state.ascii) {
        printf("utf8=%p (%zu)", (void *)compact->utf8, compact->utf8_length);
    }
    printf(", data=%p\n", data);
}
#endif




#if SIZEOF_WCHAR_T == 2
/* Helper function to convert a 16-bits wchar_t representation to UCS4, this
   will decode surrogate pairs, the other conversions are implemented as macros
   for efficiency.

   This function assumes that unicode can hold one more code point than wstr
   characters for a terminating null character. */
static void
unicode_convert_wchar_to_ucs4(const wchar_t *begin, const wchar_t *end,
                              PyObject *unicode)
{
    const wchar_t *iter;
    Py_UCS4 *ucs4_out;

    assert(unicode != NULL);
    assert(_PyUnicode_CHECK(unicode));
    assert(_PyUnicode_KIND(unicode) == PyUnicode_4BYTE_KIND);
    ucs4_out = PyUnicode_4BYTE_DATA(unicode);

    for (iter = begin; iter < end; ) {
        assert(ucs4_out < (PyUnicode_4BYTE_DATA(unicode) +
                           _PyUnicode_GET_LENGTH(unicode)));
        if (Py_UNICODE_IS_HIGH_SURROGATE(iter[0])
            && (iter+1) < end
            && Py_UNICODE_IS_LOW_SURROGATE(iter[1]))
        {
            *ucs4_out++ = Py_UNICODE_JOIN_SURROGATES(iter[0], iter[1]);
            iter += 2;
        }
        else {
            *ucs4_out++ = *iter;
            iter++;
        }
    }
    assert(ucs4_out == (PyUnicode_4BYTE_DATA(unicode) +
                        _PyUnicode_GET_LENGTH(unicode)));

}
#endif









/* Find the maximum code point and count the number of surrogate pairs so a
   correct string length can be computed before converting a string to UCS4.
   This function counts single surrogates as a character and not as a pair.

   Return 0 on success, or -1 on error. */


static void
unicode_dealloc(PyObject *unicode)
{
#ifdef Py_DEBUG
    if (!unicode_is_finalizing() && unicode_is_singleton(unicode)) {
        _Py_FatalRefcountError("deallocating an Unicode singleton");
    }
#endif
    if (_PyUnicode_STATE(unicode).statically_allocated) {
        /* This should never get called, but we also don't want to SEGV if
        * we accidentally decref an immortal string out of existence. Since
        * the string is an immortal object, just re-set the reference count.
        */
#ifdef Py_DEBUG
        Py_UNREACHABLE();
#endif
        _Py_SetImmortal(unicode);
        return;
    }
    switch (_PyUnicode_STATE(unicode).interned) {
        case SSTATE_NOT_INTERNED:
            break;
        case SSTATE_INTERNED_MORTAL:
            /* Remove the object from the intern dict.
             * Before doing so, we set the refcount to 3: the key and value
             * in the interned_dict, plus one to work with.
             */
            assert(Py_REFCNT(unicode) == 0);
            Py_SET_REFCNT(unicode, 3);
#ifdef Py_REF_DEBUG
            /* let's be pedantic with the ref total */
            _Py_IncRefTotal(_PyInterpreterState_GET());
            _Py_IncRefTotal(_PyInterpreterState_GET());
            _Py_IncRefTotal(_PyInterpreterState_GET());
#endif
            PyInterpreterState *interp = _PyInterpreterState_GET();
            PyObject *interned = get_interned_dict(interp);
            assert(interned != NULL);
            int r = PyDict_DelItem(interned, unicode);
            if (r == -1) {
                PyErr_WriteUnraisable(unicode);
                // We don't know what happened to the string. It's probably
                // best to leak it:
                // - if it was not found, something is very wrong
                // - if it was deleted, there are no more references to it
                //   so it can't cause trouble (except wasted memory)
                // - if it wasn't deleted, it'll remain interned
                _Py_SetImmortal(unicode);
                _PyUnicode_STATE(unicode).interned = SSTATE_INTERNED_IMMORTAL;
                return;
            }
            // Only our work reference should be left; remove it too.
            assert(Py_REFCNT(unicode) == 1);
            Py_SET_REFCNT(unicode, 0);
#ifdef Py_REF_DEBUG
            /* let's be pedantic with the ref total */
            _Py_DecRefTotal(_PyInterpreterState_GET());
#endif
            break;
        default:
            // As with `statically_allocated` above.
#ifdef Py_REF_DEBUG
            Py_UNREACHABLE();
#endif
            _Py_SetImmortal(unicode);
            return;
    }
    if (_PyUnicode_HAS_UTF8_MEMORY(unicode)) {
        PyObject_Free(_PyUnicode_UTF8(unicode));
    }
    if (!PyUnicode_IS_COMPACT(unicode) && _PyUnicode_DATA_ANY(unicode)) {
        PyObject_Free(_PyUnicode_DATA_ANY(unicode));
    }

    Py_TYPE(unicode)->tp_free(unicode);
}

#ifdef Py_DEBUG
static int
unicode_is_singleton(PyObject *unicode)
{
    if (unicode == &_Py_STR(empty)) {
        return 1;
    }

    PyASCIIObject *ascii = _PyASCIIObject_CAST(unicode);
    if (ascii->length == 1) {
        Py_UCS4 ch = PyUnicode_READ_CHAR(unicode, 0);
        if (ch < 256 && LATIN1(ch) == unicode) {
            return 1;
        }
    }
    return 0;
}
#endif







/* Copy an ASCII or latin1 char* string into a Python Unicode string.

   WARNING: The function doesn't copy the terminating null character and
   doesn't check the maximum character (may write a latin1 character in an
   ASCII string). */



















/* Internal function, doesn't check maximum character */















/* Ensure that a string uses the most efficient storage, if it is not the
   case: create a new string with of the right kind. Write NULL into *p_unicode
   on error. */





/* Widen Unicode objects to larger buffers. Don't write terminating null
   character. Return NULL on error. */









/* maximum number of characters required for output of %jo or %jd or %p.
   We need at most ceil(log8(256)*sizeof(intmax_t)) digits,
   plus 1 for the sign, plus 2 for the 0x prefix (for %p),
   plus 1 for the terminal NUL. */
#define MAX_INTMAX_CHARS (5 + (sizeof(intmax_t)*8-1) / 3)







#define F_LONG 1
#define F_LONGLONG 2
#define F_SIZE 3
#define F_PTRDIFF 4
#define F_INTMAX 5
;
;
;
;
;











#ifdef HAVE_WCHAR_H

/* Convert a Unicode object to a wide character string.

   - If w is NULL: return the number of wide characters (including the null
     character) required to convert the unicode object. Ignore size argument.

   - Otherwise: return the number of wide characters (excluding the null
     character) written into w. Write at most size wide characters (including
     the null character). */




#endif /* HAVE_WCHAR_H */











/* Normalize an encoding name: similar to encodings.normalize_encoding(), but
   also convert to lowercase. Return 1 on success, or 0 on error (encoding is
   longer than lower_len-1). */







































static int unicode_fill_utf8(PyObject *unicode);







/*
PyUnicode_GetSize() has been deprecated since Python 3.3
because it returned length of Py_UNICODE.

But this function is part of stable abi, because it don't
include Py_UNICODE in signature and it was not excluded from
stable abi in PEP 384.
*/










/* create or adjust a UnicodeDecodeError */


#ifdef MS_WINDOWS
static int
widechar_resize(wchar_t **buf, Py_ssize_t *size, Py_ssize_t newsize)
{
    if (newsize > *size) {
        wchar_t *newbuf = *buf;
        if (PyMem_Resize(newbuf, wchar_t, newsize) == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        *buf = newbuf;
    }
    *size = newsize;
    return 0;
}

/* error handling callback helper:
   build arguments, call the callback and check the arguments,
   if no exception occurred, copy the replacement to the output
   and adjust various state variables.
   return 0 on success, -1 on error
*/

static int
unicode_decode_call_errorhandler_wchar(
    const char *errors, PyObject **errorHandler,
    const char *encoding, const char *reason,
    const char **input, const char **inend, Py_ssize_t *startinpos,
    Py_ssize_t *endinpos, PyObject **exceptionObject, const char **inptr,
    wchar_t **buf, Py_ssize_t *bufsize, Py_ssize_t *outpos)
{
    static const char *argparse = "Un;decoding error handler must return (str, int) tuple";

    PyObject *restuple = NULL;
    PyObject *repunicode = NULL;
    Py_ssize_t outsize;
    Py_ssize_t insize;
    Py_ssize_t requiredsize;
    Py_ssize_t newpos;
    PyObject *inputobj = NULL;
    Py_ssize_t repwlen;

    if (*errorHandler == NULL) {
        *errorHandler = PyCodec_LookupError(errors);
        if (*errorHandler == NULL)
            goto onError;
    }

    make_decode_exception(exceptionObject,
        encoding,
        *input, *inend - *input,
        *startinpos, *endinpos,
        reason);
    if (*exceptionObject == NULL)
        goto onError;

    restuple = PyObject_CallOneArg(*errorHandler, *exceptionObject);
    if (restuple == NULL)
        goto onError;
    if (!PyTuple_Check(restuple)) {
        PyErr_SetString(PyExc_TypeError, &argparse[3]);
        goto onError;
    }
    if (!PyArg_ParseTuple(restuple, argparse, &repunicode, &newpos))
        goto onError;

    /* Copy back the bytes variables, which might have been modified by the
       callback */
    inputobj = PyUnicodeDecodeError_GetObject(*exceptionObject);
    if (!inputobj)
        goto onError;
    *input = PyBytes_AS_STRING(inputobj);
    insize = PyBytes_GET_SIZE(inputobj);
    *inend = *input + insize;
    /* we can DECREF safely, as the exception has another reference,
       so the object won't go away. */
    Py_DECREF(inputobj);

    if (newpos<0)
        newpos = insize+newpos;
    if (newpos<0 || newpos>insize) {
        PyErr_Format(PyExc_IndexError, "position %zd from error handler out of bounds", newpos);
        goto onError;
    }

    repwlen = PyUnicode_AsWideChar(repunicode, NULL, 0);
    if (repwlen < 0)
        goto onError;
    repwlen--;
    /* need more space? (at least enough for what we
       have+the replacement+the rest of the string (starting
       at the new input position), so we won't have to check space
       when there are no errors in the rest of the string) */
    requiredsize = *outpos;
    if (requiredsize > PY_SSIZE_T_MAX - repwlen)
        goto overflow;
    requiredsize += repwlen;
    if (requiredsize > PY_SSIZE_T_MAX - (insize - newpos))
        goto overflow;
    requiredsize += insize - newpos;
    outsize = *bufsize;
    if (requiredsize > outsize) {
        if (outsize <= PY_SSIZE_T_MAX/2 && requiredsize < 2*outsize)
            requiredsize = 2*outsize;
        if (widechar_resize(buf, bufsize, requiredsize) < 0) {
            goto onError;
        }
    }
    PyUnicode_AsWideChar(repunicode, *buf + *outpos, repwlen);
    *outpos += repwlen;
    *endinpos = newpos;
    *inptr = *input + newpos;

    /* we made it! */
    Py_DECREF(restuple);
    return 0;

  overflow:
    PyErr_SetString(PyExc_OverflowError,
                    "decoded result is too long for a Python string");

  onError:
    Py_XDECREF(restuple);
    return -1;
}
#endif   /* MS_WINDOWS */



/* --- UTF-7 Codec -------------------------------------------------------- */

/* See RFC2152 for details.  We encode conservatively and decode liberally. */

/* Three simple macros defining base-64. */

/* Is c a base-64 character? */

#define IS_BASE64(c) \
    (((c) >= 'A' && (c) <= 'Z') ||     \
     ((c) >= 'a' && (c) <= 'z') ||     \
     ((c) >= '0' && (c) <= '9') ||     \
     (c) == '+' || (c) == '/')

/* given that c is a base-64 character, what is its base-64 value? */

#define FROM_BASE64(c)                                                  \
    (((c) >= 'A' && (c) <= 'Z') ? (c) - 'A' :                           \
     ((c) >= 'a' && (c) <= 'z') ? (c) - 'a' + 26 :                      \
     ((c) >= '0' && (c) <= '9') ? (c) - '0' + 52 :                      \
     (c) == '+' ? 62 : 63)

/* What is the base-64 character of the bottom 6 bits of n? */

#define TO_BASE64(n)  \
    ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(n) & 0x3f])

/* DECODE_DIRECT: this byte encountered in a UTF-7 string should be
 * decoded as itself.  We are permissive on decoding; the only ASCII
 * byte not decoding to itself is the + which begins a base64
 * string. */

#define DECODE_DIRECT(c)                                \
    ((c) <= 127 && (c) != '+')

/* The UTF-7 encoder treats ASCII characters differently according to
 * whether they are Set D, Set O, Whitespace, or special (i.e. none of
 * the above).  See RFC2152.  This array identifies these different
 * sets:
 * 0 : "Set D"
 *     alphanumeric and '(),-./:?
 * 1 : "Set O"
 *     !"#$%&*;<=>@[]^_`{|}
 * 2 : "whitespace"
 *     ht nl cr sp
 * 3 : special (must be base64 encoded)
 *     everything else (i.e. +\~ and non-printing codes 0-8 11-12 14-31 127)
 */

;

/* ENCODE_DIRECT: this character should be encoded as itself.  The
 * answer depends on whether we are encoding set O as itself, and also
 * on whether we are encoding whitespace as itself.  RFC2152 makes it
 * clear that the answers to these questions vary between
 * applications, so this code needs to be flexible.  */

#define ENCODE_DIRECT(c, directO, directWS)             \
    ((c) < 128 && (c) > 0 &&                            \
     ((utf7_category[(c)] == 0) ||                      \
      (directWS && (utf7_category[(c)] == 2)) ||        \
      (directO && (utf7_category[(c)] == 1))))



/* The decoder.  The only state we preserve is our read position,
 * i.e. how many characters we have consumed.  So if we end in the
 * middle of a shift sequence we have to back off the read position
 * and the output to the beginning of the sequence, otherwise we lose
 * all the shift state (seen bits, number of bits seen, high
 * surrogate). */






#undef IS_BASE64
#undef FROM_BASE64
#undef TO_BASE64
#undef DECODE_DIRECT
#undef ENCODE_DIRECT

/* --- UTF-8 Codec -------------------------------------------------------- */



#include "stringlib/asciilib.h"
#include "stringlib/codecs.h"
#include "stringlib/undef.h"

#include "stringlib/ucs1lib.h"
#include "stringlib/codecs.h"
#include "stringlib/undef.h"

#include "stringlib/ucs2lib.h"
#include "stringlib/codecs.h"
#include "stringlib/undef.h"

#include "stringlib/ucs4lib.h"
#include "stringlib/codecs.h"
#include "stringlib/undef.h"

/* Mask to quickly check whether a C 'size_t' contains a
   non-ASCII, UTF8-encoded char. */
#if (SIZEOF_SIZE_T == 8)
# define ASCII_CHAR_MASK 0x8080808080808080ULL
#elif (SIZEOF_SIZE_T == 4)
# define ASCII_CHAR_MASK 0x80808080U
#else
# error C 'size_t' size should be either 4 or 8!
#endif









/* UTF-8 decoder: use surrogateescape error handler if 'surrogateescape' is
   non-zero, use strict error handler otherwise.

   On success, write a pointer to a newly allocated wide character string into
   *wstr (use PyMem_RawFree() to free the memory) and write the output length
   (in number of wchar_t units) into *wlen (if wlen is set).

   On memory allocation failure, return -1.

   On decoding error (if surrogateescape is zero), return -2. If wlen is
   non-NULL, write the start of the illegal byte sequence into *wlen. If reason
   is not NULL, write the decoding error message into *reason. */






/* UTF-8 encoder using the surrogateescape error handler .

   On success, return 0 and write the newly allocated character string (use
   PyMem_Free() to free the memory) into *str.

   On encoding failure, return -2 and write the position of the invalid
   surrogate character into *error_pos (if error_pos is set) and the decoding
   error message into *reason (if reason is set).

   On memory allocation failure, return -1. */



/* Primary internal function which creates utf8 encoded bytes objects.

   Allocation strategy:  if the string is short, convert into a stack buffer
   and allocate exactly as much space needed at the end.  Else allocate the
   maximum possible needed (4 result bytes per Unicode character), and return
   the excess memory at the end.
*/









/* --- UTF-32 Codec ------------------------------------------------------- */









/* --- UTF-16 Codec ------------------------------------------------------- */









/* --- Unicode Escape Codec ----------------------------------------------- */







/* Return a Unicode-Escape string version of the Unicode object. */



/* --- Raw Unicode Escape Codec ------------------------------------------- */








/* --- Latin-1 Codec ------------------------------------------------------ */



/* create or adjust a UnicodeEncodeError */


/* raises a UnicodeEncodeError */


/* error handling callback helper:
   build arguments, call the callback and check the arguments,
   put the result into newpos and return the replacement string, which
   has to be freed by the caller */








/* --- 7-bit ASCII Codec -------------------------------------------------- */







#ifdef MS_WINDOWS

/* --- MBCS codecs for Windows -------------------------------------------- */

#if SIZEOF_INT < SIZEOF_SIZE_T
#define NEED_RETRY
#endif

/* INT_MAX is the theoretical largest chunk (or INT_MAX / 2 when
   transcoding from UTF-16), but INT_MAX / 4 performs better in
   both cases also and avoids partial characters overrunning the
   length limit in MultiByteToWideChar on Windows */
#define DECODING_CHUNK_SIZE (INT_MAX/4)

#ifndef WC_ERR_INVALID_CHARS
#  define WC_ERR_INVALID_CHARS 0x0080
#endif

static const char*
code_page_name(UINT code_page, PyObject **obj)
{
    *obj = NULL;
    if (code_page == CP_ACP)
        return "mbcs";
    if (code_page == CP_UTF7)
        return "CP_UTF7";
    if (code_page == CP_UTF8)
        return "CP_UTF8";

    *obj = PyBytes_FromFormat("cp%u", code_page);
    if (*obj == NULL)
        return NULL;
    return PyBytes_AS_STRING(*obj);
}

static DWORD
decode_code_page_flags(UINT code_page)
{
    if (code_page == CP_UTF7) {
        /* The CP_UTF7 decoder only supports flags=0 */
        return 0;
    }
    else
        return MB_ERR_INVALID_CHARS;
}

/*
 * Decode a byte string from a Windows code page into unicode object in strict
 * mode.
 *
 * Returns consumed size if succeed, returns -2 on decode error, or raise an
 * OSError and returns -1 on other error.
 */
static int
decode_code_page_strict(UINT code_page,
                        wchar_t **buf,
                        Py_ssize_t *bufsize,
                        const char *in,
                        int insize)
{
    DWORD flags = MB_ERR_INVALID_CHARS;
    wchar_t *out;
    DWORD outsize;

    /* First get the size of the result */
    assert(insize > 0);
    while ((outsize = MultiByteToWideChar(code_page, flags,
                                          in, insize, NULL, 0)) <= 0)
    {
        if (!flags || GetLastError() != ERROR_INVALID_FLAGS) {
            goto error;
        }
        /* For some code pages (e.g. UTF-7) flags must be set to 0. */
        flags = 0;
    }

    /* Extend a wchar_t* buffer */
    Py_ssize_t n = *bufsize;   /* Get the current length */
    if (widechar_resize(buf, bufsize, n + outsize) < 0) {
        return -1;
    }
    out = *buf + n;

    /* Do the conversion */
    outsize = MultiByteToWideChar(code_page, flags, in, insize, out, outsize);
    if (outsize <= 0)
        goto error;
    return insize;

error:
    if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
        return -2;
    PyErr_SetFromWindowsErr(0);
    return -1;
}

/*
 * Decode a byte string from a code page into unicode object with an error
 * handler.
 *
 * Returns consumed size if succeed, or raise an OSError or
 * UnicodeDecodeError exception and returns -1 on error.
 */
static int
decode_code_page_errors(UINT code_page,
                        wchar_t **buf,
                        Py_ssize_t *bufsize,
                        const char *in, const int size,
                        const char *errors, int final)
{
    const char *startin = in;
    const char *endin = in + size;
    DWORD flags = MB_ERR_INVALID_CHARS;
    /* Ideally, we should get reason from FormatMessage. This is the Windows
       2000 English version of the message. */
    const char *reason = "No mapping for the Unicode character exists "
                         "in the target code page.";
    /* each step cannot decode more than 1 character, but a character can be
       represented as a surrogate pair */
    wchar_t buffer[2], *out;
    int insize;
    Py_ssize_t outsize;
    PyObject *errorHandler = NULL;
    PyObject *exc = NULL;
    PyObject *encoding_obj = NULL;
    const char *encoding;
    DWORD err;
    int ret = -1;

    assert(size > 0);

    encoding = code_page_name(code_page, &encoding_obj);
    if (encoding == NULL)
        return -1;

    if ((errors == NULL || strcmp(errors, "strict") == 0) && final) {
        /* The last error was ERROR_NO_UNICODE_TRANSLATION, then we raise a
           UnicodeDecodeError. */
        make_decode_exception(&exc, encoding, in, size, 0, 0, reason);
        if (exc != NULL) {
            PyCodec_StrictErrors(exc);
            Py_CLEAR(exc);
        }
        goto error;
    }

    /* Extend a wchar_t* buffer */
    Py_ssize_t n = *bufsize;   /* Get the current length */
    if (size > (PY_SSIZE_T_MAX - n) / (Py_ssize_t)Py_ARRAY_LENGTH(buffer)) {
        PyErr_NoMemory();
        goto error;
    }
    if (widechar_resize(buf, bufsize, n + size * Py_ARRAY_LENGTH(buffer)) < 0) {
        goto error;
    }
    out = *buf + n;

    /* Decode the byte string character per character */
    while (in < endin)
    {
        /* Decode a character */
        insize = 1;
        do
        {
            outsize = MultiByteToWideChar(code_page, flags,
                                          in, insize,
                                          buffer, Py_ARRAY_LENGTH(buffer));
            if (outsize > 0)
                break;
            err = GetLastError();
            if (err == ERROR_INVALID_FLAGS && flags) {
                /* For some code pages (e.g. UTF-7) flags must be set to 0. */
                flags = 0;
                continue;
            }
            if (err != ERROR_NO_UNICODE_TRANSLATION
                && err != ERROR_INSUFFICIENT_BUFFER)
            {
                PyErr_SetFromWindowsErr(err);
                goto error;
            }
            insize++;
        }
        /* 4=maximum length of a UTF-8 sequence */
        while (insize <= 4 && (in + insize) <= endin);

        if (outsize <= 0) {
            Py_ssize_t startinpos, endinpos, outpos;

            /* last character in partial decode? */
            if (in + insize >= endin && !final)
                break;

            startinpos = in - startin;
            endinpos = startinpos + 1;
            outpos = out - *buf;
            if (unicode_decode_call_errorhandler_wchar(
                    errors, &errorHandler,
                    encoding, reason,
                    &startin, &endin, &startinpos, &endinpos, &exc, &in,
                    buf, bufsize, &outpos))
            {
                goto error;
            }
            out = *buf + outpos;
        }
        else {
            in += insize;
            memcpy(out, buffer, outsize * sizeof(wchar_t));
            out += outsize;
        }
    }

    /* Shrink the buffer */
    assert(out - *buf <= *bufsize);
    *bufsize = out - *buf;
    /* (in - startin) <= size and size is an int */
    ret = Py_SAFE_DOWNCAST(in - startin, Py_ssize_t, int);

error:
    Py_XDECREF(encoding_obj);
    Py_XDECREF(errorHandler);
    Py_XDECREF(exc);
    return ret;
}

static PyObject *
decode_code_page_stateful(int code_page,
                          const char *s, Py_ssize_t size,
                          const char *errors, Py_ssize_t *consumed)
{
    wchar_t *buf = NULL;
    Py_ssize_t bufsize = 0;
    int chunk_size, final, converted, done;

    if (code_page < 0) {
        PyErr_SetString(PyExc_ValueError, "invalid code page number");
        return NULL;
    }
    if (size < 0) {
        PyErr_BadInternalCall();
        return NULL;
    }

    if (consumed)
        *consumed = 0;

    do
    {
#ifdef NEED_RETRY
        if (size > DECODING_CHUNK_SIZE) {
            chunk_size = DECODING_CHUNK_SIZE;
            final = 0;
            done = 0;
        }
        else
#endif
        {
            chunk_size = (int)size;
            final = (consumed == NULL);
            done = 1;
        }

        if (chunk_size == 0 && done) {
            if (buf != NULL)
                break;
            _Py_RETURN_UNICODE_EMPTY();
        }

        converted = decode_code_page_strict(code_page, &buf, &bufsize,
                                            s, chunk_size);
        if (converted == -2)
            converted = decode_code_page_errors(code_page, &buf, &bufsize,
                                                s, chunk_size,
                                                errors, final);
        assert(converted != 0 || done);

        if (converted < 0) {
            PyMem_Free(buf);
            return NULL;
        }

        if (consumed)
            *consumed += converted;

        s += converted;
        size -= converted;
    } while (!done);

    PyObject *v = PyUnicode_FromWideChar(buf, bufsize);
    PyMem_Free(buf);
    return v;
}

PyObject *
PyUnicode_DecodeCodePageStateful(int code_page,
                                 const char *s,
                                 Py_ssize_t size,
                                 const char *errors,
                                 Py_ssize_t *consumed)
{
    return decode_code_page_stateful(code_page, s, size, errors, consumed);
}

PyObject *
PyUnicode_DecodeMBCSStateful(const char *s,
                             Py_ssize_t size,
                             const char *errors,
                             Py_ssize_t *consumed)
{
    return decode_code_page_stateful(CP_ACP, s, size, errors, consumed);
}

PyObject *
PyUnicode_DecodeMBCS(const char *s,
                     Py_ssize_t size,
                     const char *errors)
{
    return PyUnicode_DecodeMBCSStateful(s, size, errors, NULL);
}

static DWORD
encode_code_page_flags(UINT code_page, const char *errors)
{
    if (code_page == CP_UTF8) {
        return WC_ERR_INVALID_CHARS;
    }
    else if (code_page == CP_UTF7) {
        /* CP_UTF7 only supports flags=0 */
        return 0;
    }
    else {
        if (errors != NULL && strcmp(errors, "replace") == 0)
            return 0;
        else
            return WC_NO_BEST_FIT_CHARS;
    }
}

/*
 * Encode a Unicode string to a Windows code page into a byte string in strict
 * mode.
 *
 * Returns consumed characters if succeed, returns -2 on encode error, or raise
 * an OSError and returns -1 on other error.
 */
static int
encode_code_page_strict(UINT code_page, PyObject **outbytes,
                        PyObject *unicode, Py_ssize_t offset, int len,
                        const char* errors)
{
    BOOL usedDefaultChar = FALSE;
    BOOL *pusedDefaultChar = &usedDefaultChar;
    int outsize;
    wchar_t *p;
    Py_ssize_t size;
    const DWORD flags = encode_code_page_flags(code_page, NULL);
    char *out;
    /* Create a substring so that we can get the UTF-16 representation
       of just the slice under consideration. */
    PyObject *substring;
    int ret = -1;

    assert(len > 0);

    if (code_page != CP_UTF8 && code_page != CP_UTF7)
        pusedDefaultChar = &usedDefaultChar;
    else
        pusedDefaultChar = NULL;

    substring = PyUnicode_Substring(unicode, offset, offset+len);
    if (substring == NULL)
        return -1;
    p = PyUnicode_AsWideCharString(substring, &size);
    Py_CLEAR(substring);
    if (p == NULL) {
        return -1;
    }
    assert(size <= INT_MAX);

    /* First get the size of the result */
    outsize = WideCharToMultiByte(code_page, flags,
                                  p, (int)size,
                                  NULL, 0,
                                  NULL, pusedDefaultChar);
    if (outsize <= 0)
        goto error;
    /* If we used a default char, then we failed! */
    if (pusedDefaultChar && *pusedDefaultChar) {
        ret = -2;
        goto done;
    }

    if (*outbytes == NULL) {
        /* Create string object */
        *outbytes = PyBytes_FromStringAndSize(NULL, outsize);
        if (*outbytes == NULL) {
            goto done;
        }
        out = PyBytes_AS_STRING(*outbytes);
    }
    else {
        /* Extend string object */
        const Py_ssize_t n = PyBytes_Size(*outbytes);
        if (outsize > PY_SSIZE_T_MAX - n) {
            PyErr_NoMemory();
            goto done;
        }
        if (_PyBytes_Resize(outbytes, n + outsize) < 0) {
            goto done;
        }
        out = PyBytes_AS_STRING(*outbytes) + n;
    }

    /* Do the conversion */
    outsize = WideCharToMultiByte(code_page, flags,
                                  p, (int)size,
                                  out, outsize,
                                  NULL, pusedDefaultChar);
    if (outsize <= 0)
        goto error;
    if (pusedDefaultChar && *pusedDefaultChar) {
        ret = -2;
        goto done;
    }
    ret = 0;

done:
    PyMem_Free(p);
    return ret;

error:
    if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
        ret = -2;
        goto done;
    }
    PyErr_SetFromWindowsErr(0);
    goto done;
}

/*
 * Encode a Unicode string to a Windows code page into a byte string using an
 * error handler.
 *
 * Returns consumed characters if succeed, or raise an OSError and returns
 * -1 on other error.
 */
static int
encode_code_page_errors(UINT code_page, PyObject **outbytes,
                        PyObject *unicode, Py_ssize_t unicode_offset,
                        Py_ssize_t insize, const char* errors)
{
    const DWORD flags = encode_code_page_flags(code_page, errors);
    Py_ssize_t pos = unicode_offset;
    Py_ssize_t endin = unicode_offset + insize;
    /* Ideally, we should get reason from FormatMessage. This is the Windows
       2000 English version of the message. */
    const char *reason = "invalid character";
    /* 4=maximum length of a UTF-8 sequence */
    char buffer[4];
    BOOL usedDefaultChar = FALSE, *pusedDefaultChar;
    Py_ssize_t outsize;
    char *out;
    PyObject *errorHandler = NULL;
    PyObject *exc = NULL;
    PyObject *encoding_obj = NULL;
    const char *encoding;
    Py_ssize_t newpos, newoutsize;
    PyObject *rep;
    int ret = -1;

    assert(insize > 0);

    encoding = code_page_name(code_page, &encoding_obj);
    if (encoding == NULL)
        return -1;

    if (errors == NULL || strcmp(errors, "strict") == 0) {
        /* The last error was ERROR_NO_UNICODE_TRANSLATION,
           then we raise a UnicodeEncodeError. */
        make_encode_exception(&exc, encoding, unicode, 0, 0, reason);
        if (exc != NULL) {
            PyCodec_StrictErrors(exc);
            Py_DECREF(exc);
        }
        Py_XDECREF(encoding_obj);
        return -1;
    }

    if (code_page != CP_UTF8 && code_page != CP_UTF7)
        pusedDefaultChar = &usedDefaultChar;
    else
        pusedDefaultChar = NULL;

    if (Py_ARRAY_LENGTH(buffer) > PY_SSIZE_T_MAX / insize) {
        PyErr_NoMemory();
        goto error;
    }
    outsize = insize * Py_ARRAY_LENGTH(buffer);

    if (*outbytes == NULL) {
        /* Create string object */
        *outbytes = PyBytes_FromStringAndSize(NULL, outsize);
        if (*outbytes == NULL)
            goto error;
        out = PyBytes_AS_STRING(*outbytes);
    }
    else {
        /* Extend string object */
        Py_ssize_t n = PyBytes_Size(*outbytes);
        if (n > PY_SSIZE_T_MAX - outsize) {
            PyErr_NoMemory();
            goto error;
        }
        if (_PyBytes_Resize(outbytes, n + outsize) < 0)
            goto error;
        out = PyBytes_AS_STRING(*outbytes) + n;
    }

    /* Encode the string character per character */
    while (pos < endin)
    {
        Py_UCS4 ch = PyUnicode_READ_CHAR(unicode, pos);
        wchar_t chars[2];
        int charsize;
        if (ch < 0x10000) {
            chars[0] = (wchar_t)ch;
            charsize = 1;
        }
        else {
            chars[0] = Py_UNICODE_HIGH_SURROGATE(ch);
            chars[1] = Py_UNICODE_LOW_SURROGATE(ch);
            charsize = 2;
        }

        outsize = WideCharToMultiByte(code_page, flags,
                                      chars, charsize,
                                      buffer, Py_ARRAY_LENGTH(buffer),
                                      NULL, pusedDefaultChar);
        if (outsize > 0) {
            if (pusedDefaultChar == NULL || !(*pusedDefaultChar))
            {
                pos++;
                memcpy(out, buffer, outsize);
                out += outsize;
                continue;
            }
        }
        else if (GetLastError() != ERROR_NO_UNICODE_TRANSLATION) {
            PyErr_SetFromWindowsErr(0);
            goto error;
        }

        rep = unicode_encode_call_errorhandler(
                  errors, &errorHandler, encoding, reason,
                  unicode, &exc,
                  pos, pos + 1, &newpos);
        if (rep == NULL)
            goto error;

        Py_ssize_t morebytes = pos - newpos;
        if (PyBytes_Check(rep)) {
            outsize = PyBytes_GET_SIZE(rep);
            morebytes += outsize;
            if (morebytes > 0) {
                Py_ssize_t offset = out - PyBytes_AS_STRING(*outbytes);
                newoutsize = PyBytes_GET_SIZE(*outbytes) + morebytes;
                if (_PyBytes_Resize(outbytes, newoutsize) < 0) {
                    Py_DECREF(rep);
                    goto error;
                }
                out = PyBytes_AS_STRING(*outbytes) + offset;
            }
            memcpy(out, PyBytes_AS_STRING(rep), outsize);
            out += outsize;
        }
        else {
            Py_ssize_t i;
            int kind;
            const void *data;

            outsize = PyUnicode_GET_LENGTH(rep);
            morebytes += outsize;
            if (morebytes > 0) {
                Py_ssize_t offset = out - PyBytes_AS_STRING(*outbytes);
                newoutsize = PyBytes_GET_SIZE(*outbytes) + morebytes;
                if (_PyBytes_Resize(outbytes, newoutsize) < 0) {
                    Py_DECREF(rep);
                    goto error;
                }
                out = PyBytes_AS_STRING(*outbytes) + offset;
            }
            kind = PyUnicode_KIND(rep);
            data = PyUnicode_DATA(rep);
            for (i=0; i < outsize; i++) {
                Py_UCS4 ch = PyUnicode_READ(kind, data, i);
                if (ch > 127) {
                    raise_encode_exception(&exc,
                        encoding, unicode,
                        pos, pos + 1,
                        "unable to encode error handler result to ASCII");
                    Py_DECREF(rep);
                    goto error;
                }
                *out = (unsigned char)ch;
                out++;
            }
        }
        pos = newpos;
        Py_DECREF(rep);
    }
    /* write a NUL byte */
    *out = 0;
    outsize = out - PyBytes_AS_STRING(*outbytes);
    assert(outsize <= PyBytes_GET_SIZE(*outbytes));
    if (_PyBytes_Resize(outbytes, outsize) < 0)
        goto error;
    ret = 0;

error:
    Py_XDECREF(encoding_obj);
    Py_XDECREF(errorHandler);
    Py_XDECREF(exc);
    return ret;
}

static PyObject *
encode_code_page(int code_page,
                 PyObject *unicode,
                 const char *errors)
{
    Py_ssize_t len;
    PyObject *outbytes = NULL;
    Py_ssize_t offset;
    int chunk_len, ret, done;

    if (!PyUnicode_Check(unicode)) {
        PyErr_BadArgument();
        return NULL;
    }

    len = PyUnicode_GET_LENGTH(unicode);

    if (code_page < 0) {
        PyErr_SetString(PyExc_ValueError, "invalid code page number");
        return NULL;
    }

    if (len == 0)
        return PyBytes_FromStringAndSize(NULL, 0);

    offset = 0;
    do
    {
#ifdef NEED_RETRY
        if (len > DECODING_CHUNK_SIZE) {
            chunk_len = DECODING_CHUNK_SIZE;
            done = 0;
        }
        else
#endif
        {
            chunk_len = (int)len;
            done = 1;
        }

        ret = encode_code_page_strict(code_page, &outbytes,
                                      unicode, offset, chunk_len,
                                      errors);
        if (ret == -2)
            ret = encode_code_page_errors(code_page, &outbytes,
                                          unicode, offset,
                                          chunk_len, errors);
        if (ret < 0) {
            Py_XDECREF(outbytes);
            return NULL;
        }

        offset += chunk_len;
        len -= chunk_len;
    } while (!done);

    return outbytes;
}

PyObject *
PyUnicode_EncodeCodePage(int code_page,
                         PyObject *unicode,
                         const char *errors)
{
    return encode_code_page(code_page, unicode, errors);
}

PyObject *
PyUnicode_AsMBCSString(PyObject *unicode)
{
    return PyUnicode_EncodeCodePage(CP_ACP, unicode, NULL);
}

#undef NEED_RETRY

#endif /* MS_WINDOWS */

/* --- Character Mapping Codec -------------------------------------------- */







/* Charmap encoding: the lookup table */

/*[clinic input]
class EncodingMap "struct encoding_map *" "&EncodingMapType"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=14e46bbb6c522d22]*/

struct encoding_map {
    PyObject_HEAD
    unsigned char level1[32];
    int count2, count3;
    unsigned char level23[1];
};

/*[clinic input]
EncodingMap.size

Return the size (in bytes) of this object.
[clinic start generated code]*/



;

;





/* Lookup the character ch in the mapping. If the character
   can't be found, Py_None is returned (or NULL, if another
   error occurred). */




typedef enum charmapencode_result {
    enc_SUCCESS, enc_FAILED, enc_EXCEPTION
} charmapencode_result;
/* lookup the character, put the result in the output string and adjust
   various state variables. Resize the output bytes object if not enough
   space is available. Return a new reference to the object that
   was put in the output buffer, or Py_None, if the mapping was undefined
   (in which case no character was written) or NULL, if a
   reallocation error occurred. The caller must decref the result */


/* handle an error in PyUnicode_EncodeCharmap
   Return 0 on success, -1 on error */






/* create or adjust a UnicodeTranslateError */


/* error handling callback helper:
   build arguments, call the callback and check the arguments,
   put the result into newpos and return the replacement string, which
   has to be freed by the caller */


/* Lookup the character ch in the mapping and put the result in result,
   which must be decrefed by the caller.
   Return 0 on success, -1 on error */


/* lookup the character, write the result into the writer.
   Return 1 if the result was written into the writer, return 0 if the mapping
   was undefined, raise an exception return -1 on error. */




/* Fast path for ascii => ascii translation. Return 1 if the whole string
   was translated into writer, return 0 if the input string was partially
   translated into writer, raise an exception and return -1 on error. */








/* --- Helpers ------------------------------------------------------------ */

/* helper macro to fixup start/end slice values */
#define ADJUST_INDICES(start, end, len)         \
    if (end > len)                              \
        end = len;                              \
    else if (end < 0) {                         \
        end += len;                             \
        if (end < 0)                            \
            end = 0;                            \
    }                                           \
    if (start < 0) {                            \
        start += len;                           \
        if (start < 0)                          \
            start = 0;                          \
    }



/* _PyUnicode_InsertThousandsGrouping() helper functions */
#include "stringlib/localeutil.h"

/**
 * InsertThousandsGrouping:
 * @writer: Unicode writer.
 * @n_buffer: Number of characters in @buffer.
 * @digits: Digits we're reading from. If count is non-NULL, this is unused.
 * @d_pos: Start of digits string.
 * @n_digits: The number of digits in the string, in which we want
 *            to put the grouping chars.
 * @min_width: The minimum width of the digits in the output string.
 *             Output will be zero-padded on the left to fill.
 * @grouping: see definition in localeconv().
 * @thousands_sep: see definition in localeconv().
 *
 * There are 2 modes: counting and filling. If @writer is NULL,
 *  we are in counting mode, else filling mode.
 * If counting, the required buffer size is returned.
 * If filling, we know the buffer will be large enough, so we don't
 *  need to pass in the buffer size.
 * Inserts thousand grouping characters (as defined by grouping and
 *  thousands_sep) into @writer.
 *
 * Return value: -1 on error, number of characters otherwise.
 **/




























































/* --- Unicode Object Methods --------------------------------------------- */

/*[clinic input]
str.title as unicode_title

Return a version of the string where each word is titlecased.

More specifically, words start with uppercased characters and all remaining
cased characters have lower case.
[clinic start generated code]*/



/*[clinic input]
str.capitalize as unicode_capitalize

Return a capitalized version of the string.

More specifically, make the first character have upper case and the rest lower
case.
[clinic start generated code]*/



/*[clinic input]
str.casefold as unicode_casefold

Return a version of the string suitable for caseless comparisons.
[clinic start generated code]*/




/* Argument converter. Accepts a single Unicode character. */



/*[clinic input]
str.center as unicode_center

    width: Py_ssize_t
    fillchar: Py_UCS4 = ' '
    /

Return a centered string of length width.

Padding is done using the specified fill character (default is a space).
[clinic start generated code]*/



/* This function assumes that str1 and str2 are readied by the caller. */






















/* Concat to string or Unicode object giving a new Unicode object. */







/*
Wraps asciilib_parse_args_finds() and additionally ensures that the
first argument is a unicode object.
*/



PyDoc_STRVAR(count__doc__,
             "S.count(sub[, start[, end]]) -> int\n\
\n\
Return the number of non-overlapping occurrences of substring sub in\n\
string S[start:end].  Optional arguments start and end are\n\
interpreted as in slice notation.");



/*[clinic input]
str.encode as unicode_encode

    encoding: str(c_default="NULL") = 'utf-8'
        The encoding in which to encode the string.
    errors: str(c_default="NULL") = 'strict'
        The error handling scheme to use for encoding errors.
        The default is 'strict' meaning that encoding errors raise a
        UnicodeEncodeError.  Other possible values are 'ignore', 'replace' and
        'xmlcharrefreplace' as well as any other name registered with
        codecs.register_error that can handle UnicodeEncodeErrors.

Encode the string using the codec registered for encoding.
[clinic start generated code]*/



/*[clinic input]
str.expandtabs as unicode_expandtabs

    tabsize: int = 8

Return a copy where all tab characters are expanded using spaces.

If tabsize is not given, a tab size of 8 characters is assumed.
[clinic start generated code]*/



PyDoc_STRVAR(find__doc__,
             "S.find(sub[, start[, end]]) -> int\n\
\n\
Return the lowest index in S where substring sub is found,\n\
such that sub is contained within S[start:end].  Optional\n\
arguments start and end are interpreted as in slice notation.\n\
\n\
Return -1 on failure.");





/* Believe it or not, this produces the same value for ASCII strings
   as bytes_hash(). */


PyDoc_STRVAR(index__doc__,
             "S.index(sub[, start[, end]]) -> int\n\
\n\
Return the lowest index in S where substring sub is found,\n\
such that sub is contained within S[start:end].  Optional\n\
arguments start and end are interpreted as in slice notation.\n\
\n\
Raises ValueError when the substring is not found.");



/*[clinic input]
str.isascii as unicode_isascii

Return True if all characters in the string are ASCII, False otherwise.

ASCII characters have code points in the range U+0000-U+007F.
Empty string is ASCII too.
[clinic start generated code]*/



/*[clinic input]
str.islower as unicode_islower

Return True if the string is a lowercase string, False otherwise.

A string is lowercase if all cased characters in the string are lowercase and
there is at least one cased character in the string.
[clinic start generated code]*/



/*[clinic input]
str.isupper as unicode_isupper

Return True if the string is an uppercase string, False otherwise.

A string is uppercase if all cased characters in the string are uppercase and
there is at least one cased character in the string.
[clinic start generated code]*/



/*[clinic input]
str.istitle as unicode_istitle

Return True if the string is a title-cased string, False otherwise.

In a title-cased string, upper- and title-case characters may only
follow uncased characters and lowercase characters only cased ones.
[clinic start generated code]*/



/*[clinic input]
str.isspace as unicode_isspace

Return True if the string is a whitespace string, False otherwise.

A string is whitespace if all characters in the string are whitespace and there
is at least one character in the string.
[clinic start generated code]*/



/*[clinic input]
str.isalpha as unicode_isalpha

Return True if the string is an alphabetic string, False otherwise.

A string is alphabetic if all characters in the string are alphabetic and there
is at least one character in the string.
[clinic start generated code]*/



/*[clinic input]
str.isalnum as unicode_isalnum

Return True if the string is an alpha-numeric string, False otherwise.

A string is alpha-numeric if all characters in the string are alpha-numeric and
there is at least one character in the string.
[clinic start generated code]*/



/*[clinic input]
str.isdecimal as unicode_isdecimal

Return True if the string is a decimal string, False otherwise.

A string is a decimal string if all characters in the string are decimal and
there is at least one character in the string.
[clinic start generated code]*/



/*[clinic input]
str.isdigit as unicode_isdigit

Return True if the string is a digit string, False otherwise.

A string is a digit string if all characters in the string are digits and there
is at least one character in the string.
[clinic start generated code]*/



/*[clinic input]
str.isnumeric as unicode_isnumeric

Return True if the string is a numeric string, False otherwise.

A string is numeric if all characters in the string are numeric and there is at
least one character in the string.
[clinic start generated code]*/







/*[clinic input]
str.isidentifier as unicode_isidentifier

Return True if the string is a valid Python identifier, False otherwise.

Call keyword.iskeyword(s) to test whether string s is a reserved identifier,
such as "def" or "class".
[clinic start generated code]*/



/*[clinic input]
str.isprintable as unicode_isprintable

Return True if the string is printable, False otherwise.

A string is printable if all of its characters are considered printable in
repr() or if it is empty.
[clinic start generated code]*/



/*[clinic input]
str.join as unicode_join

    iterable: object
    /

Concatenate any number of strings.

The string whose method is called is inserted in between each given string.
The result is returned as a new string.

Example: '.'.join(['ab', 'pq', 'rs']) -> 'ab.pq.rs'
[clinic start generated code]*/





/*[clinic input]
str.ljust as unicode_ljust

    width: Py_ssize_t
    fillchar: Py_UCS4 = ' '
    /

Return a left-justified string of length width.

Padding is done using the specified fill character (default is a space).
[clinic start generated code]*/



/*[clinic input]
str.lower as unicode_lower

Return a copy of the string converted to lowercase.
[clinic start generated code]*/



#define LEFTSTRIP 0
#define RIGHTSTRIP 1
#define BOTHSTRIP 2

/* Arrays indexed by above */
;

#define STRIPNAME(i) (stripfuncnames[i])

/* externally visible for str.strip(unicode) */










/*[clinic input]
str.strip as unicode_strip

    chars: object = None
    /

Return a copy of the string with leading and trailing whitespace removed.

If chars is given and not None, remove characters in chars instead.
[clinic start generated code]*/




/*[clinic input]
str.lstrip as unicode_lstrip

    chars: object = None
    /

Return a copy of the string with leading whitespace removed.

If chars is given and not None, remove characters in chars instead.
[clinic start generated code]*/




/*[clinic input]
str.rstrip as unicode_rstrip

    chars: object = None
    /

Return a copy of the string with trailing whitespace removed.

If chars is given and not None, remove characters in chars instead.
[clinic start generated code]*/








/*[clinic input]
str.replace as unicode_replace

    old: unicode
    new: unicode
    count: Py_ssize_t = -1
        Maximum number of occurrences to replace.
        -1 (the default value) means replace all occurrences.
    /

Return a copy with all occurrences of substring old replaced by new.

If the optional argument count is given, only the first count occurrences are
replaced.
[clinic start generated code]*/



/*[clinic input]
str.removeprefix as unicode_removeprefix

    prefix: unicode
    /

Return a str with the given prefix string removed if present.

If the string starts with the prefix string, return string[len(prefix):].
Otherwise, return a copy of the original string.
[clinic start generated code]*/



/*[clinic input]
str.removesuffix as unicode_removesuffix

    suffix: unicode
    /

Return a str with the given suffix string removed if present.

If the string ends with the suffix string and that suffix is not empty,
return string[:-len(suffix)]. Otherwise, return a copy of the original
string.
[clinic start generated code]*/





PyDoc_STRVAR(rfind__doc__,
             "S.rfind(sub[, start[, end]]) -> int\n\
\n\
Return the highest index in S where substring sub is found,\n\
such that sub is contained within S[start:end].  Optional\n\
arguments start and end are interpreted as in slice notation.\n\
\n\
Return -1 on failure.");



PyDoc_STRVAR(rindex__doc__,
             "S.rindex(sub[, start[, end]]) -> int\n\
\n\
Return the highest index in S where substring sub is found,\n\
such that sub is contained within S[start:end].  Optional\n\
arguments start and end are interpreted as in slice notation.\n\
\n\
Raises ValueError when the substring is not found.");



/*[clinic input]
str.rjust as unicode_rjust

    width: Py_ssize_t
    fillchar: Py_UCS4 = ' '
    /

Return a right-justified string of length width.

Padding is done using the specified fill character (default is a space).
[clinic start generated code]*/





/*[clinic input]
str.split as unicode_split

    sep: object = None
        The separator used to split the string.

        When set to None (the default value), will split on any whitespace
        character (including \n \r \t \f and spaces) and will discard
        empty strings from the result.
    maxsplit: Py_ssize_t = -1
        Maximum number of splits.
        -1 (the default value) means no limit.

Return a list of the substrings in the string, using sep as the separator string.

Splitting starts at the front of the string and works to the end.

Note, str.split() is mainly useful for data that has been intentionally
delimited.  With natural text that includes punctuation, consider using
the regular expression module.

[clinic start generated code]*/








/*[clinic input]
str.partition as unicode_partition

    sep: object
    /

Partition the string into three parts using the given separator.

This will search for the separator in the string.  If the separator is found,
returns a 3-tuple containing the part before the separator, the separator
itself, and the part after it.

If the separator is not found, returns a 3-tuple containing the original string
and two empty strings.
[clinic start generated code]*/



/*[clinic input]
str.rpartition as unicode_rpartition = str.partition

Partition the string into three parts using the given separator.

This will search for the separator in the string, starting at the end. If
the separator is found, returns a 3-tuple containing the part before the
separator, the separator itself, and the part after it.

If the separator is not found, returns a 3-tuple containing two empty strings
and the original string.
[clinic start generated code]*/





/*[clinic input]
str.rsplit as unicode_rsplit = str.split

Return a list of the substrings in the string, using sep as the separator string.

Splitting starts at the end of the string and works to the front.
[clinic start generated code]*/



/*[clinic input]
str.splitlines as unicode_splitlines

    keepends: bool = False

Return a list of the lines in the string, breaking at line boundaries.

Line breaks are not included in the resulting list unless keepends is given and
true.
[clinic start generated code]*/





/*[clinic input]
str.swapcase as unicode_swapcase

Convert uppercase characters to lowercase and lowercase characters to uppercase.
[clinic start generated code]*/



/*[clinic input]

@staticmethod
str.maketrans as unicode_maketrans

  x: object

  y: unicode=NULL

  z: unicode=NULL

  /

Return a translation table usable for str.translate().

If there is only one argument, it must be a dictionary mapping Unicode
ordinals (integers) or characters to Unicode ordinals, strings or None.
Character keys will be then converted to ordinals.
If there are two arguments, they must be strings of equal length, and
in the resulting dictionary, each character in x will be mapped to the
character at the same position in y. If there is a third argument, it
must be a string, whose characters will be mapped to None in the result.
[clinic start generated code]*/



/*[clinic input]
str.translate as unicode_translate

    table: object
        Translation table, which must be a mapping of Unicode ordinals to
        Unicode ordinals, strings, or None.
    /

Replace each character in the string using the given translation table.

The table must implement lookup/indexing via __getitem__, for instance a
dictionary or list.  If this operation raises LookupError, the character is
left untouched.  Characters mapped to None are deleted.
[clinic start generated code]*/



/*[clinic input]
str.upper as unicode_upper

Return a copy of the string converted to uppercase.
[clinic start generated code]*/



/*[clinic input]
str.zfill as unicode_zfill

    width: Py_ssize_t
    /

Pad a numeric string with zeros on the left, to fill a field of the given width.

The string is never truncated.
[clinic start generated code]*/



PyDoc_STRVAR(startswith__doc__,
             "S.startswith(prefix[, start[, end]]) -> bool\n\
\n\
Return True if S starts with the specified prefix, False otherwise.\n\
With optional start, test S beginning at that position.\n\
With optional end, stop comparing S at that position.\n\
prefix can also be a tuple of strings to try.");




PyDoc_STRVAR(endswith__doc__,
             "S.endswith(suffix[, start[, end]]) -> bool\n\
\n\
Return True if S ends with the specified suffix, False otherwise.\n\
With optional start, test S beginning at that position.\n\
With optional end, stop comparing S at that position.\n\
suffix can also be a tuple of strings to try.");







// Initialize _PyUnicodeWriter with initial buffer






















#include "stringlib/unicode_format.h"

PyDoc_STRVAR(format__doc__,
             "S.format(*args, **kwargs) -> str\n\
\n\
Return a formatted version of S, using substitutions from args and kwargs.\n\
The substitutions are identified by braces ('{' and '}').");

PyDoc_STRVAR(format_map__doc__,
             "S.format_map(mapping) -> str\n\
\n\
Return a formatted version of S, using substitutions from mapping.\n\
The substitutions are identified by braces ('{' and '}').");

/*[clinic input]
str.__format__ as unicode___format__

    format_spec: unicode
    /

Return a formatted version of the string as described by format_spec.
[clinic start generated code]*/



/*[clinic input]
str.__sizeof__ as unicode_sizeof

Return the size of the string in memory, in bytes.
[clinic start generated code]*/





;



;

;



;


/* Helpers for PyUnicode_Format() */

struct unicode_formatter_t {
    PyObject *args;
    int args_owned;
    Py_ssize_t arglen, argidx;
    PyObject *dict;

    int fmtkind;
    Py_ssize_t fmtcnt, fmtpos;
    const void *fmtdata;
    PyObject *fmtstr;

    _PyUnicodeWriter writer;
};

struct unicode_format_arg_t {
    Py_UCS4 ch;
    int flags;
    Py_ssize_t width;
    int prec;
    int sign;
};



/* Returns a new reference to a PyUnicode object, or NULL on failure. */

/* Format a float into the writer if the writer is not NULL, or into *p_output
   otherwise.

   Return 0 on success, raise an exception and return -1 on error. */


/* formatlong() emulates the format codes d, u, o, x and X, and
 * the F_ALT flag, for Python's long (unbounded) ints.  It's not used for
 * Python's regular ints.
 * Return value:  a new PyUnicodeObject*, or NULL if error.
 *     The output string is of the form
 *         "-"? ("0x" | "0X")? digit+
 *     "0x"/"0X" are present only for x and X conversions, with F_ALT
 *         set in flags.  The case of hex digits will be correct,
 *     There will be at least prec digits, zero-filled on the left if
 *         necessary to get that many.
 * val          object to be converted
 * flags        bitmask of format flags; only F_ALT is looked at
 * prec         minimum number of digits; 0-fill on left if needed
 * type         a character in [duoxX]; u acts the same as d
 *
 * CAUTION:  o, x and X conversions on regular ints can never
 * produce a '-' sign, but can for Python's unbounded ints.
 */


/* Format an integer or a float as an integer.
 * Return 1 if the number has been formatted into the writer,
 *        0 if the number has been formatted into *p_output
 *       -1 and raise an exception on error */




/* Parse options of an argument: flags, width, precision.
   Handle also "%(name)" syntax.

   Return 0 if the argument has been formatted into arg->str.
   Return 1 if the argument has been written into ctx->writer,
   Raise an exception and return -1 on error. */


/* Format one argument. Supported conversion specifiers:

   - "s", "r", "a": any type
   - "i", "d", "u": int or float
   - "o", "x", "X": int
   - "e", "E", "f", "F", "g", "G": float
   - "c": int or str (1 character)

   When possible, the output is written directly into the Unicode writer
   (ctx->writer). A string is created when padding is required.

   Return 0 if the argument has been formatted into *p_str,
          1 if the argument has been written into ctx->writer,
         -1 on error. */




/* Helper of PyUnicode_Format(): format one arg.
   Return 0 on success, raise an exception and return -1 on error. */




static PyObject *
unicode_subtype_new(PyTypeObject *type, PyObject *unicode);

/*[clinic input]
@classmethod
str.__new__ as unicode_new

    object as x: object = NULL
    encoding: str = NULL
    errors: str = NULL

[clinic start generated code]*/





void
_PyUnicode_ExactDealloc(PyObject *op)
{
    assert(PyUnicode_CheckExact(op));
    unicode_dealloc(op);
}

PyDoc_STRVAR(unicode_doc,
"str(object='') -> str\n\
str(bytes_or_buffer[, encoding[, errors]]) -> str\n\
\n\
Create a new string object from the given object. If encoding or\n\
errors is specified, then the object must expose a data buffer\n\
that will be decoded using the given encoding and error handler.\n\
Otherwise, returns the result of object.__str__() (if defined)\n\
or repr(object).\n\
encoding defaults to sys.getdefaultencoding().\n\
errors defaults to 'strict'.");

static PyObject *unicode_iter(PyObject *seq);

;

/* Initialize the Unicode implementation */































// Public-looking name kept for the stable ABI; user should not call this:
PyAPI_FUNC(void) PyUnicode_InternImmortal(PyObject **);








/********************* Unicode Iterator **************************/

typedef struct {
    PyObject_HEAD
    Py_ssize_t it_index;
    PyObject *it_seq;    /* Set to NULL when iterator is exhausted */
} unicodeiterobject;











PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");



PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");



PyDoc_STRVAR(setstate_doc, "Set state information for unpickling.");

;

;

;
























#ifdef MS_WINDOWS
int
_PyUnicode_EnableLegacyWindowsFSEncoding(void)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    PyConfig *config = (PyConfig *)_PyInterpreterState_GetConfig(interp);

    /* Set the filesystem encoding to mbcs/replace (PEP 529) */
    wchar_t *encoding = _PyMem_RawWcsdup(L"mbcs");
    wchar_t *errors = _PyMem_RawWcsdup(L"replace");
    if (encoding == NULL || errors == NULL) {
        PyMem_RawFree(encoding);
        PyMem_RawFree(errors);
        PyErr_NoMemory();
        return -1;
    }

    PyMem_RawFree(config->filesystem_encoding);
    config->filesystem_encoding = encoding;
    PyMem_RawFree(config->filesystem_errors);
    config->filesystem_errors = errors;

    return init_fs_codec(interp);
}
#endif


#ifdef Py_DEBUG
static inline int
unicode_is_finalizing(void)
{
    return (get_interned_dict(_PyInterpreterState_Main()) == NULL);
}
#endif







/* A _string module, to export formatter_parser and formatter_field_name_split
   to the string.Formatter class implemented in Python. */

;

;

;




#ifdef __cplusplus
}
#endif
