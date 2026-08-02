/* stringlib: codec implementations */

#if !STRINGLIB_IS_UNICODE
# error "codecs.h is specific to Unicode"
#endif

#include "pycore_bitutils.h"      // _Py_bswap32()

/* Mask to quickly check whether a C 'size_t' contains a
   non-ASCII, UTF8-encoded char. */
#if (SIZEOF_SIZE_T == 8)
# define ASCII_CHAR_MASK 0x8080808080808080ULL
#elif (SIZEOF_SIZE_T == 4)
# define ASCII_CHAR_MASK 0x80808080U
#else
# error C 'size_t' size should be either 4 or 8!
#endif

/* 10xxxxxx */
#define IS_CONTINUATION_BYTE(ch) ((ch) >= 0x80 && (ch) < 0xC0)



#undef ASCII_CHAR_MASK


/* UTF-8 encoder specialized for a Unicode kind to avoid the slow
   PyUnicode_READ() macro. Delete some parts of the code depending on the kind:
   UCS-1 strings don't need to handle surrogates for example. */


/* The pattern for constructing UCS2-repeated masks. */
#if SIZEOF_LONG == 8
# define UCS2_REPEAT_MASK 0x0001000100010001ul
#elif SIZEOF_LONG == 4
# define UCS2_REPEAT_MASK 0x00010001ul
#else
# error C 'long' size should be either 4 or 8!
#endif

/* The mask for fast checking. */
#if STRINGLIB_SIZEOF_CHAR == 1
/* The mask for fast checking of whether a C 'long' contains a
   non-ASCII or non-Latin1 UTF16-encoded characters. */
# define FAST_CHAR_MASK         (UCS2_REPEAT_MASK * (0xFFFFu & ~STRINGLIB_MAX_CHAR))
#else
/* The mask for fast checking of whether a C 'long' may contain
   UTF16-encoded surrogate characters. This is an efficient heuristic,
   assuming that non-surrogate characters with a code point >= 0x8000 are
   rare in most input.
*/
# define FAST_CHAR_MASK         (UCS2_REPEAT_MASK * 0x8000u)
#endif
/* The mask for fast byte-swapping. */
#define STRIPPED_MASK           (UCS2_REPEAT_MASK * 0x00FFu)
/* Swap bytes. */
#define SWAB(value)             ((((value) >> 8) & STRIPPED_MASK) | \
                                 (((value) & STRIPPED_MASK) << 8))


#undef UCS2_REPEAT_MASK
#undef FAST_CHAR_MASK
#undef STRIPPED_MASK
#undef SWAB


#if STRINGLIB_MAX_CHAR >= 0x80






#endif
