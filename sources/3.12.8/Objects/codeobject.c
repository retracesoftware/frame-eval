#include <stdbool.h>

#include "Python.h"
#include "opcode.h"
#include "structmember.h"         // PyMemberDef
#include "pycore_code.h"          // _PyCodeConstructor
#include "pycore_frame.h"         // FRAME_SPECIALS_SIZE
#include "pycore_interp.h"        // PyInterpreterState.co_extra_freefuncs
#include "pycore_opcode.h"        // _PyOpcode_Deopt
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_tuple.h"         // _PyTuple_ITEMS()
#include "clinic/codeobject.c.h"

static PyObject* code_repr(PyCodeObject *co);











/******************
 * generic helpers
 ******************/

/* all_name_chars(s): true iff s matches [a-zA-Z0-9_]* */




/* Intern selected string constants */


/* Return a shallow copy of a tuple that is
   guaranteed to contain exact strings, by converting string subclasses
   to exact strings and complaining if a non-string is found. */



/******************
 * _PyCode_New()
 ******************/

// This is also used in compile.c.








extern void _PyCode_Quicken(PyCodeObject *code);











/* The caller is responsible for ensuring that the given data is valid. */




/******************
 * the legacy "constructors"
 ******************/





// NOTE: When modifying the construction of PyCode_NewEmpty, please also change
// test.test_code.CodeLocationTest.test_code_new_empty to keep it in sync!

;

;




/******************
 * source location tracking (co_lines/co_positions)
 ******************/



void
_PyLineTable_InitAddressRange(const char *linetable, Py_ssize_t length, int firstlineno, PyCodeAddressRange *range)
{
    range->opaque.lo_next = (const uint8_t *)linetable;
    range->opaque.limit = range->opaque.lo_next + length;
    range->ar_start = -1;
    range->ar_end = 0;
    range->opaque.computed_line = firstlineno;
    range->ar_line = -1;
}

int
_PyCode_InitAddressRange(PyCodeObject* co, PyCodeAddressRange *bounds)
{
    assert(co->co_linetable != NULL);
    const char *linetable = PyBytes_AS_STRING(co->co_linetable);
    Py_ssize_t length = PyBytes_GET_SIZE(co->co_linetable);
    _PyLineTable_InitAddressRange(linetable, length, co->co_firstlineno, bounds);
    return bounds->ar_line;
}

/* Update *bounds to describe the first and one-past-the-last instructions in
   the same line as lasti.  Return the number of that line, or -1 if lasti is out of bounds. */





#define ASSERT_VALID_BOUNDS(bounds) \
    assert(bounds->opaque.lo_next <=  bounds->opaque.limit && \
        (bounds->ar_line == -1 || bounds->ar_line == bounds->opaque.computed_line) && \
        (bounds->opaque.lo_next == bounds->opaque.limit || \
        (*bounds->opaque.lo_next) & 128))
































typedef struct {
    PyObject_HEAD
    PyCodeObject *li_code;
    PyCodeAddressRange li_line;
} lineiterator;








;



/* co_positions iterator object. */
typedef struct {
    PyObject_HEAD
    PyCodeObject* pi_code;
    PyCodeAddressRange pi_range;
    int pi_offset;
    int pi_endline;
    int pi_column;
    int pi_endcolumn;
} positionsiterator;





;




/******************
 * "extra" frame eval info (see PEP 523)
 ******************/

/* Holder for co_extra information */
typedef struct {
    Py_ssize_t ce_size;
    void *ce_extras[1];
} _PyCodeObjectExtra;








/******************
 * other PyCodeObject accessor functions
 ******************/





















/******************
 * PyCode_Type
 ******************/

/*[clinic input]
class code "PyCodeObject *" "&PyCode_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=78aa5d576683bb4b]*/

/*[clinic input]
@classmethod
code.__new__ as code_new

    argcount: int
    posonlyargcount: int
    kwonlyargcount: int
    nlocals: int
    stacksize: int
    flags: int
    codestring as code: object(subclass_of="&PyBytes_Type")
    constants as consts: object(subclass_of="&PyTuple_Type")
    names: object(subclass_of="&PyTuple_Type")
    varnames: object(subclass_of="&PyTuple_Type")
    filename: unicode
    name: unicode
    qualname: unicode
    firstlineno: int
    linetable: object(subclass_of="&PyBytes_Type")
    exceptiontable: object(subclass_of="&PyBytes_Type")
    freevars: object(subclass_of="&PyTuple_Type", c_default="NULL") = ()
    cellvars: object(subclass_of="&PyTuple_Type", c_default="NULL") = ()
    /

Create a code object.  Not for the faint of heart.
[clinic start generated code]*/














#define OFF(x) offsetof(PyCodeObject, x)

;














;






/*[clinic input]
@text_signature "($self, /, **changes)"
code.replace

    *
    co_argcount: int(c_default="self->co_argcount") = unchanged
    co_posonlyargcount: int(c_default="self->co_posonlyargcount") = unchanged
    co_kwonlyargcount: int(c_default="self->co_kwonlyargcount") = unchanged
    co_nlocals: int(c_default="self->co_nlocals") = unchanged
    co_stacksize: int(c_default="self->co_stacksize") = unchanged
    co_flags: int(c_default="self->co_flags") = unchanged
    co_firstlineno: int(c_default="self->co_firstlineno") = unchanged
    co_code: object(subclass_of="&PyBytes_Type", c_default="NULL") = unchanged
    co_consts: object(subclass_of="&PyTuple_Type", c_default="self->co_consts") = unchanged
    co_names: object(subclass_of="&PyTuple_Type", c_default="self->co_names") = unchanged
    co_varnames: object(subclass_of="&PyTuple_Type", c_default="NULL") = unchanged
    co_freevars: object(subclass_of="&PyTuple_Type", c_default="NULL") = unchanged
    co_cellvars: object(subclass_of="&PyTuple_Type", c_default="NULL") = unchanged
    co_filename: unicode(c_default="self->co_filename") = unchanged
    co_name: unicode(c_default="self->co_name") = unchanged
    co_qualname: unicode(c_default="self->co_qualname") = unchanged
    co_linetable: object(subclass_of="&PyBytes_Type", c_default="self->co_linetable") = unchanged
    co_exceptiontable: object(subclass_of="&PyBytes_Type", c_default="self->co_exceptiontable") = unchanged

Return a copy of the code object with new values for the specified fields.
[clinic start generated code]*/



/*[clinic input]
code._varname_from_oparg

    oparg: int

(internal-only) Return the local variable name for the given oparg.

WARNING: this method is for internal use only and may change or go away.
[clinic start generated code]*/



/* XXX code objects need to participate in GC? */

;


;


/******************
 * other API
 ******************/







#define MAX_CODE_UNITS_PER_LOC_ENTRY 8


