#include "Python.h"
#include "pycore_call.h"          // _PyObject_VectorcallTstate()
#include "pycore_context.h"
#include "pycore_gc.h"            // _PyObject_GC_MAY_BE_TRACKED()
#include "pycore_hamt.h"
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_object.h"
#include "pycore_pyerrors.h"
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "structmember.h"         // PyMemberDef


#include "clinic/context.c.h"
/*[clinic input]
module _contextvars
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=a0955718c8b8cea6]*/


#define ENSURE_Context(o, err_ret)                                  \
    if (!PyContext_CheckExact(o)) {                                 \
        PyErr_SetString(PyExc_TypeError,                            \
                        "an instance of Context was expected");     \
        return err_ret;                                             \
    }

#define ENSURE_ContextVar(o, err_ret)                               \
    if (!PyContextVar_CheckExact(o)) {                              \
        PyErr_SetString(PyExc_TypeError,                            \
                       "an instance of ContextVar was expected");   \
        return err_ret;                                             \
    }

#define ENSURE_ContextToken(o, err_ret)                             \
    if (!PyContextToken_CheckExact(o)) {                            \
        PyErr_SetString(PyExc_TypeError,                            \
                        "an instance of Token was expected");       \
        return err_ret;                                             \
    }


/////////////////////////// Context API


static PyContext *
context_new_empty(void);

static PyContext *
context_new_from_vars(PyHamtObject *vars);

static inline PyContext *
context_get(void);

static PyContextToken *
token_new(PyContext *ctx, PyContextVar *var, PyObject *val);

static PyContextVar *
contextvar_new(PyObject *name, PyObject *def);

static int
contextvar_set(PyContextVar *var, PyObject *val);

static int
contextvar_del(PyContextVar *var);


#if PyContext_MAXFREELIST > 0

#endif





































/////////////////////////// PyContext

/*[clinic input]
class _contextvars.Context "PyContext *" "&PyContext_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=bdf87f8e0cb580e8]*/


































/*[clinic input]
_contextvars.Context.get
    key: object
    default: object = None
    /

Return the value for `key` if `key` has the value in the context object.

If `key` does not exist, return `default`. If `default` is not given,
return None.
[clinic start generated code]*/




/*[clinic input]
_contextvars.Context.items

Return all variables and their values in the context object.

The result is returned as a list of 2-tuples (variable, value).
[clinic start generated code]*/




/*[clinic input]
_contextvars.Context.keys

Return a list of all variables in the context object.
[clinic start generated code]*/




/*[clinic input]
_contextvars.Context.values

Return a list of all variables' values in the context object.
[clinic start generated code]*/




/*[clinic input]
_contextvars.Context.copy

Return a shallow copy of the context object.
[clinic start generated code]*/







;

;

;

;


/////////////////////////// ContextVar











/*[clinic input]
class _contextvars.ContextVar "PyContextVar *" "&PyContextVar_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=445da935fa8883c3]*/















/*[clinic input]
_contextvars.ContextVar.get
    default: object = NULL
    /

Return a value for the context variable for the current context.

If there is no value for the variable in the current context, the method will:
 * return the value of the default argument of the method, if provided; or
 * return the default value for the context variable, if it was created
   with one; or
 * raise a LookupError.
[clinic start generated code]*/



/*[clinic input]
_contextvars.ContextVar.set
    value: object
    /

Call to set a new value for the context variable in the current context.

The required value argument is the new value for the context variable.

Returns a Token object that can be used to restore the variable to its previous
value via the `ContextVar.reset()` method.
[clinic start generated code]*/



/*[clinic input]
_contextvars.ContextVar.reset
    token: object
    /

Reset the context variable.

The variable is reset to the value it had before the `ContextVar.set()` that
created the token was used.
[clinic start generated code]*/




;

;

;


/////////////////////////// Token

static PyObject * get_token_missing(void);


/*[clinic input]
class _contextvars.Token "PyContextToken *" "&PyContextToken_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=338a5e2db13d3f5b]*/
















;

;

;




/////////////////////////// Token.MISSING







;





///////////////////////////


void
_PyContext_ClearFreeList(PyInterpreterState *interp)
{
#if PyContext_MAXFREELIST > 0
    struct _Py_context_state *state = &interp->context;
    for (; state->numfree; state->numfree--) {
        PyContext *ctx = state->freelist;
        state->freelist = (PyContext *)ctx->ctx_weakreflist;
        ctx->ctx_weakreflist = NULL;
        PyObject_GC_Del(ctx);
    }
#endif
}






