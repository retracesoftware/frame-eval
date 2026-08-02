
/* Error handling */

#include "Python.h"
#include "pycore_call.h"          // _PyObject_CallNoArgs()
#include "pycore_initconfig.h"    // _PyStatus_ERR()
#include "pycore_pyerrors.h"      // _PyErr_Format()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_structseq.h"     // _PyStructSequence_FiniBuiltin()
#include "pycore_sysmodule.h"     // _PySys_Audit()
#include "pycore_traceback.h"     // _PyTraceBack_FromFrame()

#include <ctype.h>
#ifdef MS_WINDOWS
#  include <windows.h>
#  include <winbase.h>
#  include <stdlib.h>             // _sys_nerr
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
static PyObject *
_PyErr_FormatV(PyThreadState *tstate, PyObject *exception,
               const char *format, va_list vargs);

void
_PyErr_SetRaisedException(PyThreadState *tstate, PyObject *exc)
{
    PyObject *old_exc = tstate->current_exception;
    tstate->current_exception = exc;
    Py_XDECREF(old_exc);
}

















/* Set a key error with the specified argument, wrapping it in a
 * tuple automatically so that tuple keys are not unpacked as the
 * exception arguments. */

























#ifndef Py_NORMALIZE_RECURSION_LIMIT
#define Py_NORMALIZE_RECURSION_LIMIT 32
#endif

/* Used in many places to normalize a raised exception, including in
   eval_code2(), do_raise(), and PyErr_Print()

   XXX: should PyErr_NormalizeException() also call
            PyException_SetTraceback() with the resulting value and tb?
*/






PyObject *
_PyErr_GetRaisedException(PyThreadState *tstate) {
    PyObject *exc = tstate->current_exception;
    tstate->current_exception = NULL;
    return exc;
}




































/* Like PyErr_Restore(), but if an exception is already set,
   set the context associated with it.

   The caller is responsible for ensuring that this call won't create
   any cycles in the exception context chain. */


/* Like PyErr_SetRaisedException(), but if an exception is already set,
   set the context associated with it.

   The caller is responsible for ensuring that this call won't create
   any cycles in the exception context chain. */


/* Set the currently set exception's context to the given exception.

   If the provided exc_info is NULL, then the current Python thread state's
   exc_info will be used for the context instead.

   This function can only be called when _PyErr_Occurred() is true.
   Also, this function won't create any cycles in the exception context
   chain to the extent that _PyErr_SetObject ensures this. */








/* Convenience functions to set a type error exception and return 0 */













#ifdef MS_WINDOWS
/* Windows specific error code handling */
PyObject *PyErr_SetExcFromWindowsErrWithFilenameObject(
    PyObject *exc,
    int ierr,
    PyObject *filenameObject)
{
    return PyErr_SetExcFromWindowsErrWithFilenameObjects(exc, ierr,
        filenameObject, NULL);
}

PyObject *PyErr_SetExcFromWindowsErrWithFilenameObjects(
    PyObject *exc,
    int ierr,
    PyObject *filenameObject,
    PyObject *filenameObject2)
{
    PyThreadState *tstate = _PyThreadState_GET();
    int len;
    WCHAR *s_buf = NULL; /* Free via LocalFree */
    PyObject *message;
    PyObject *args, *v;

    DWORD err = (DWORD)ierr;
    if (err==0) {
        err = GetLastError();
    }

    len = FormatMessageW(
        /* Error API error */
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,           /* no message source */
        err,
        MAKELANGID(LANG_NEUTRAL,
        SUBLANG_DEFAULT), /* Default language */
        (LPWSTR) &s_buf,
        0,              /* size not used */
        NULL);          /* no args */
    if (len==0) {
        /* Only seen this in out of mem situations */
        message = PyUnicode_FromFormat("Windows Error 0x%x", err);
        s_buf = NULL;
    } else {
        /* remove trailing cr/lf and dots */
        while (len > 0 && (s_buf[len-1] <= L' ' || s_buf[len-1] == L'.'))
            s_buf[--len] = L'\0';
        message = PyUnicode_FromWideChar(s_buf, len);
    }

    if (message == NULL)
    {
        LocalFree(s_buf);
        return NULL;
    }

    if (filenameObject == NULL) {
        assert(filenameObject2 == NULL);
        filenameObject = filenameObject2 = Py_None;
    }
    else if (filenameObject2 == NULL)
        filenameObject2 = Py_None;
    /* This is the constructor signature for OSError.
       The POSIX translation will be figured out by the constructor. */
    args = Py_BuildValue("(iOOiO)", 0, message, filenameObject, err, filenameObject2);
    Py_DECREF(message);

    if (args != NULL) {
        v = PyObject_Call(exc, args, NULL);
        Py_DECREF(args);
        if (v != NULL) {
            _PyErr_SetObject(tstate, (PyObject *) Py_TYPE(v), v);
            Py_DECREF(v);
        }
    }
    LocalFree(s_buf);
    return NULL;
}

PyObject *PyErr_SetExcFromWindowsErrWithFilename(
    PyObject *exc,
    int ierr,
    const char *filename)
{
    PyObject *name = NULL;
    if (filename) {
        if ((DWORD)ierr == 0) {
            ierr = (int)GetLastError();
        }
        name = PyUnicode_DecodeFSDefault(filename);
        if (name == NULL) {
            return NULL;
        }
    }
    PyObject *ret = PyErr_SetExcFromWindowsErrWithFilenameObjects(exc,
                                                                 ierr,
                                                                 name,
                                                                 NULL);
    Py_XDECREF(name);
    return ret;
}

PyObject *PyErr_SetExcFromWindowsErr(PyObject *exc, int ierr)
{
    return PyErr_SetExcFromWindowsErrWithFilename(exc, ierr, NULL);
}

PyObject *PyErr_SetFromWindowsErr(int ierr)
{
    return PyErr_SetExcFromWindowsErrWithFilename(PyExc_OSError,
                                                  ierr, NULL);
}

PyObject *PyErr_SetFromWindowsErrWithFilename(
    int ierr,
    const char *filename)
{
    PyObject *name = NULL;
    if (filename) {
        if ((DWORD)ierr == 0) {
            ierr = (int)GetLastError();
        }
        name = PyUnicode_DecodeFSDefault(filename);
        if (name == NULL) {
            return NULL;
        }
    }
    PyObject *result = PyErr_SetExcFromWindowsErrWithFilenameObjects(
                                                  PyExc_OSError,
                                                  ierr, name, NULL);
    Py_XDECREF(name);
    return result;
}

#endif /* MS_WINDOWS */

static PyObject *
_PyErr_SetImportErrorSubclassWithNameFrom(
    PyObject *exception, PyObject *msg,
    PyObject *name, PyObject *path, PyObject* from_name)
{
    PyThreadState *tstate = _PyThreadState_GET();
    int issubclass;
    PyObject *kwargs, *error;

    issubclass = PyObject_IsSubclass(exception, PyExc_ImportError);
    if (issubclass < 0) {
        return NULL;
    }
    else if (!issubclass) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "expected a subclass of ImportError");
        return NULL;
    }

    if (msg == NULL) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "expected a message argument");
        return NULL;
    }

    if (name == NULL) {
        name = Py_None;
    }
    if (path == NULL) {
        path = Py_None;
    }
    if (from_name == NULL) {
        from_name = Py_None;
    }


    kwargs = PyDict_New();
    if (kwargs == NULL) {
        return NULL;
    }
    if (PyDict_SetItemString(kwargs, "name", name) < 0) {
        goto done;
    }
    if (PyDict_SetItemString(kwargs, "path", path) < 0) {
        goto done;
    }
    if (PyDict_SetItemString(kwargs, "name_from", from_name) < 0) {
        goto done;
    }

    error = PyObject_VectorcallDict(exception, &msg, 1, kwargs);
    if (error != NULL) {
        _PyErr_SetObject(tstate, (PyObject *)Py_TYPE(error), error);
        Py_DECREF(error);
    }

done:
    Py_DECREF(kwargs);
    return NULL;
}




PyObject *
_PyErr_SetImportErrorWithNameFrom(PyObject *msg, PyObject *name, PyObject *path, PyObject* from_name)
{
    return _PyErr_SetImportErrorSubclassWithNameFrom(PyExc_ImportError, msg, name, path, from_name);
}





/* Remove the preprocessor macro for PyErr_BadInternalCall() so that we can
   export the entry point for existing object code: */
#undef PyErr_BadInternalCall

#define PyErr_BadInternalCall() _PyErr_BadInternalCall(__FILE__, __LINE__)














/* Adds a note to the current exception (if any) */






/* Create an exception with docstring */



PyDoc_STRVAR(UnraisableHookArgs__doc__,
"UnraisableHookArgs\n\
\n\
Type used to pass arguments to sys.unraisablehook.");

static PyTypeObject UnraisableHookArgsType;

;

;












/* Default implementation of sys.unraisablehook.

   It can be called to log the exception of a custom sys.unraisablehook.

   Do nothing if sys.stderr attribute doesn't exist or is set to None. */









/* Call sys.unraisablehook().

   This function can be used when an exception has occurred but there is no way
   for Python to handle it. For example, when a destructor raises an exception
   or during garbage collection (gc.collect()).

   If err_msg_str is non-NULL, the error message is formatted as:
   "Exception ignored %s" % err_msg_str. Otherwise, use "Exception ignored in"
   error message.

   An exception must be set when calling this function. */









/* Set file and line information for the current exception.
   If the exception is not a SyntaxError, also sets additional attributes
   to make printing of exceptions believe it is a syntax error. */









/* Attempt to load the line of text that the exception refers to.  If it
   fails, it will return NULL but will not set an exception.

   XXX The functionality of this function is quite similar to the
   functionality in tb_displayline() in traceback.c. */





/* Function from Parser/tokenizer/file_tokenizer.c */
extern char* _PyTokenizer_FindEncodingFilename(int, PyObject *);





#ifdef __cplusplus
}
#endif
