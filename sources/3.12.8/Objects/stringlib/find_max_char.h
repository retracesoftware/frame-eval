/* Finding the optimal width of unicode characters in a buffer */

#if !STRINGLIB_IS_UNICODE
# error "find_max_char.h is specific to Unicode"
#endif

/* Mask to quickly check whether a C 'size_t' contains a
   non-ASCII, UTF8-encoded char. */
#if (SIZEOF_SIZE_T == 8)
# define UCS1_ASCII_CHAR_MASK 0x8080808080808080ULL
#elif (SIZEOF_SIZE_T == 4)
# define UCS1_ASCII_CHAR_MASK 0x80808080U
#else
# error C 'size_t' size should be either 4 or 8!
#endif

#if STRINGLIB_SIZEOF_CHAR == 1



#undef ASCII_CHAR_MASK

#else /* STRINGLIB_SIZEOF_CHAR == 1 */

#define MASK_ASCII 0xFFFFFF80
#define MASK_UCS1 0xFFFFFF00
#define MASK_UCS2 0xFFFF0000

#define MAX_CHAR_ASCII 0x7f
#define MAX_CHAR_UCS1  0xff
#define MAX_CHAR_UCS2  0xffff
#define MAX_CHAR_UCS4  0x10ffff



#undef MASK_ASCII
#undef MASK_UCS1
#undef MASK_UCS2
#undef MAX_CHAR_ASCII
#undef MAX_CHAR_UCS1
#undef MAX_CHAR_UCS2
#undef MAX_CHAR_UCS4

#endif /* STRINGLIB_SIZEOF_CHAR == 1 */

