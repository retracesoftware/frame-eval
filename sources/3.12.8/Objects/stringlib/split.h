/* stringlib: split implementation */

#ifndef STRINGLIB_FASTSEARCH_H
#error must include "stringlib/fastsearch.h" before including this module
#endif

/* Overallocate the initial list to reduce the number of reallocs for small
   split sizes.  Eg, "A A A A A A A A A A".split() (10 elements) has three
   resizes, to sizes 4, 8, then 16.  Most observed string splits are for human
   text (roughly 11 words per line) and field delimited data (usually 1-10
   fields).  For large strings the split algorithms are bandwidth limited
   so increasing the preallocation likely will not improve things.*/

#define MAX_PREALLOC 12

/* 5 splits gives 6 elements */
#define PREALLOC_SIZE(maxsplit) \
    (maxsplit >= MAX_PREALLOC ? MAX_PREALLOC : maxsplit+1)

#define SPLIT_APPEND(data, left, right)         \
    sub = STRINGLIB_NEW((data) + (left),        \
                        (right) - (left));      \
    if (sub == NULL)                            \
        goto onError;                           \
    if (PyList_Append(list, sub)) {             \
        Py_DECREF(sub);                         \
        goto onError;                           \
    }                                           \
    else                                        \
        Py_DECREF(sub);

#define SPLIT_ADD(data, left, right) {          \
    sub = STRINGLIB_NEW((data) + (left),        \
                        (right) - (left));      \
    if (sub == NULL)                            \
        goto onError;                           \
    if (count < MAX_PREALLOC) {                 \
        PyList_SET_ITEM(list, count, sub);      \
    } else {                                    \
        if (PyList_Append(list, sub)) {         \
            Py_DECREF(sub);                     \
            goto onError;                       \
        }                                       \
        else                                    \
            Py_DECREF(sub);                     \
    }                                           \
    count++; }


/* Always force the list to the expected size. */
#define FIX_PREALLOC_SIZE(list) Py_SET_SIZE(list, count)















