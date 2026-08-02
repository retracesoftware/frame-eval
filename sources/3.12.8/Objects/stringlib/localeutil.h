/* _PyUnicode_InsertThousandsGrouping() helper functions */

typedef struct {
    const char *grouping;
    char previous;
    Py_ssize_t i; /* Where we're currently pointing in grouping. */
} GroupGenerator;





/* Returns the next grouping, or 0 to signify end. */



/* Fill in some digits, leading zeros, and thousands separator. All
   are optional, depending on when we're called. */

