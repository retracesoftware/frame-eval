/* stringlib: find/index implementation */

#ifndef STRINGLIB_FASTSEARCH_H
#error must include "stringlib/fastsearch.h" before including this module
#endif









#ifdef STRINGLIB_WANT_CONTAINS_OBJ

Py_LOCAL_INLINE(int)
STRINGLIB(contains_obj)(PyObject* str, PyObject* sub)
{
    return STRINGLIB(find)(
        STRINGLIB_STR(str), STRINGLIB_LEN(str),
        STRINGLIB_STR(sub), STRINGLIB_LEN(sub), 0
        ) != -1;
}

#endif /* STRINGLIB_WANT_CONTAINS_OBJ */

/*
This function is a helper for the "find" family (find, rfind, index,
rindex) and for count, startswith and endswith, because they all have
the same behaviour for the arguments.

It does not touch the variables received until it knows everything
is ok.
*/

#define FORMAT_BUFFER_SIZE 50



#undef FORMAT_BUFFER_SIZE
