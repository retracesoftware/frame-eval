/* stringlib: count implementation */

#ifndef STRINGLIB_FASTSEARCH_H
#error must include "stringlib/fastsearch.h" before including this module
#endif

// gh-97982: Implementing asciilib_count() is not worth it, FASTSEARCH() does
// not specialize the code for ASCII strings. Use ucs1lib_count() for ASCII and
// UCS1 strings: it's the same than asciilib_count().
#if !STRINGLIB_IS_UNICODE || STRINGLIB_MAX_CHAR > 0x7Fu



#endif
