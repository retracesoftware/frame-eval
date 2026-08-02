/* Module definition and import implementation */

#include "Python.h"

#include "pycore_hashtable.h"     // _Py_hashtable_new_full()
#include "pycore_import.h"        // _PyImport_BootstrapImp()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_interp.h"        // struct _import_runtime_state
#include "pycore_namespace.h"     // _PyNamespace_Type
#include "pycore_object.h"        // _Py_SetImmortal()
#include "pycore_pyerrors.h"      // _PyErr_SetString()
#include "pycore_pyhash.h"        // _Py_KeyedHash()
#include "pycore_pylifecycle.h"
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_sysmodule.h"     // _PySys_Audit()
#include "marshal.h"              // PyMarshal_ReadObjectFromString()
#include "importdl.h"             // _PyImport_DynLoadFiletab
#include "pydtrace.h"             // PyDTrace_IMPORT_FIND_LOAD_START_ENABLED()
#include <stdbool.h>              // bool

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif


/*[clinic input]
module _imp
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=9c332475d8686284]*/

#include "clinic/import.c.h"


/*******************************/
/* process-global import state */
/*******************************/

/* This table is defined in config.c: */
extern struct _inittab _PyImport_Inittab[];

// This is not used after Py_Initialize() is called.
// (See _PyRuntimeState.imports.inittab.)
;
// When we dynamically allocate a larger table for PyImport_ExtendInittab(),
// we track the pointer here so we can deallocate it during finalization.
;


/*******************************/
/* runtime-global import state */
/*******************************/

#define INITTAB _PyRuntime.imports.inittab
#define LAST_MODULE_INDEX _PyRuntime.imports.last_module_index
#define EXTENSIONS _PyRuntime.imports.extensions

#define PKGCONTEXT (_PyRuntime.imports.pkgcontext)


/*******************************/
/* interpreter import state */
/*******************************/

#define MODULES(interp) \
    (interp)->imports.modules
#define MODULES_BY_INDEX(interp) \
    (interp)->imports.modules_by_index
#define IMPORTLIB(interp) \
    (interp)->imports.importlib
#define OVERRIDE_MULTI_INTERP_EXTENSIONS_CHECK(interp) \
    (interp)->imports.override_multi_interp_extensions_check
#define OVERRIDE_FROZEN_MODULES(interp) \
    (interp)->imports.override_frozen_modules
#ifdef HAVE_DLOPEN
#  define DLOPENFLAGS(interp) \
        (interp)->imports.dlopenflags
#endif
#define IMPORT_FUNC(interp) \
    (interp)->imports.import_func

#define IMPORT_LOCK(interp) \
    (interp)->imports.lock.mutex
#define IMPORT_LOCK_THREAD(interp) \
    (interp)->imports.lock.thread
#define IMPORT_LOCK_LEVEL(interp) \
    (interp)->imports.lock.level

#define FIND_AND_LOAD(interp) \
    (interp)->imports.find_and_load


/*******************/
/* the import lock */
/*******************/

/* Locking primitives to prevent parallel imports of the same module
   in different threads to return with a partially loaded module.
   These calls are serialized by the global interpreter lock. */





#ifdef HAVE_FORK
/* This function is called from PyOS_AfterFork_Child() to ensure that newly
   created child processes do not share locks with the parent.
   We now acquire the import lock around fork() calls but on some platforms
   (Solaris 9 and earlier? see isue7242) that still left us with problems. */

#endif


/***************/
/* sys.modules */
/***************/









// This is only kept around for extensions that use _Py_IDENTIFIER.










static void remove_importlib_frames(PyThreadState *tstate);



/* Get the module object corresponding to a module name.
   First check the modules dictionary if there's one there,
   if not, create a new one and insert it in the modules dictionary. */









/* Remove name from sys.modules, if it's there.
 * Can be called with an exception raised.
 * If fail to remove name a new exception will be chained with the old
 * exception, otherwise the old exception is preserved.
 */



/************************************/
/* per-interpreter modules-by-index */
/************************************/














/* _PyState_AddModule() has been completely removed from the C-API
   (and was removed from the limited API in 3.6).  However, we're
   playing it safe and keeping it around for any stable ABI extensions
   built against 3.2-3.5. */







// Used by finalize_modules()



/*********************/
/* extension modules */
/*********************/

/*
    It may help to have a big picture view of what happens
    when an extension is loaded.  This includes when it is imported
    for the first time.

    Here's a summary, using importlib._boostrap._load() as a starting point.

    1.  importlib._bootstrap._load()
    2.    _load():  acquire import lock
    3.    _load() -> importlib._bootstrap._load_unlocked()
    4.      _load_unlocked() -> importlib._bootstrap.module_from_spec()
    5.        module_from_spec() -> ExtensionFileLoader.create_module()
    6.          create_module() -> _imp.create_dynamic()
                    (see below)
    7.        module_from_spec() -> importlib._bootstrap._init_module_attrs()
    8.      _load_unlocked():  sys.modules[name] = module
    9.      _load_unlocked() -> ExtensionFileLoader.exec_module()
    10.       exec_module() -> _imp.exec_dynamic()
                  (see below)
    11.   _load():  release import lock


    ...for single-phase init modules, where m_size == -1:

    (6). first time  (not found in _PyRuntime.imports.extensions):
       1.  _imp_create_dynamic_impl() -> import_find_extension()
       2.  _imp_create_dynamic_impl() -> _PyImport_LoadDynamicModuleWithSpec()
       3.    _PyImport_LoadDynamicModuleWithSpec():  load <module init func>
       4.    _PyImport_LoadDynamicModuleWithSpec():  call <module init func>
       5.      <module init func> -> PyModule_Create() -> PyModule_Create2() -> PyModule_CreateInitialized()
       6.        PyModule_CreateInitialized() -> PyModule_New()
       7.        PyModule_CreateInitialized():  allocate mod->md_state
       8.        PyModule_CreateInitialized() -> PyModule_AddFunctions()
       9.        PyModule_CreateInitialized() -> PyModule_SetDocString()
       10.       PyModule_CreateInitialized():  set mod->md_def
       11.     <module init func>:  initialize the module
       12.   _PyImport_LoadDynamicModuleWithSpec() -> _PyImport_CheckSubinterpIncompatibleExtensionAllowed()
       13.   _PyImport_LoadDynamicModuleWithSpec():  set def->m_base.m_init
       14.   _PyImport_LoadDynamicModuleWithSpec():  set __file__
       15.   _PyImport_LoadDynamicModuleWithSpec() -> _PyImport_FixupExtensionObject()
       16.     _PyImport_FixupExtensionObject():  add it to interp->imports.modules_by_index
       17.     _PyImport_FixupExtensionObject():  copy __dict__ into def->m_base.m_copy
       18.     _PyImport_FixupExtensionObject():  add it to _PyRuntime.imports.extensions

    (6). subsequent times  (found in _PyRuntime.imports.extensions):
       1. _imp_create_dynamic_impl() -> import_find_extension()
       2.   import_find_extension() -> import_add_module()
       3.     if name in sys.modules:  use that module
       4.     else:
                1. import_add_module() -> PyModule_NewObject()
                2. import_add_module():  set it on sys.modules
       5.   import_find_extension():  copy the "m_copy" dict into __dict__
       6. _imp_create_dynamic_impl() -> _PyImport_CheckSubinterpIncompatibleExtensionAllowed()

    (10). (every time):
       1. noop


    ...for single-phase init modules, where m_size >= 0:

    (6). not main interpreter and never loaded there - every time  (not found in _PyRuntime.imports.extensions):
       1-16. (same as for m_size == -1)

    (6). main interpreter - first time  (not found in _PyRuntime.imports.extensions):
       1-16. (same as for m_size == -1)
       17.     _PyImport_FixupExtensionObject():  add it to _PyRuntime.imports.extensions

    (6). previously loaded in main interpreter  (found in _PyRuntime.imports.extensions):
       1. _imp_create_dynamic_impl() -> import_find_extension()
       2.   import_find_extension():  call def->m_base.m_init
       3.   import_find_extension():  add the module to sys.modules

    (10). every time:
       1. noop


    ...for multi-phase init modules:

    (6). every time:
       1.  _imp_create_dynamic_impl() -> import_find_extension()  (not found)
       2.  _imp_create_dynamic_impl() -> _PyImport_LoadDynamicModuleWithSpec()
       3.    _PyImport_LoadDynamicModuleWithSpec():  load module init func
       4.    _PyImport_LoadDynamicModuleWithSpec():  call module init func
       5.    _PyImport_LoadDynamicModuleWithSpec() -> PyModule_FromDefAndSpec()
       6.      PyModule_FromDefAndSpec(): gather/check moduledef slots
       7.      if there's a Py_mod_create slot:
                 1. PyModule_FromDefAndSpec():  call its function
       8.      else:
                 1. PyModule_FromDefAndSpec() -> PyModule_NewObject()
       9:      PyModule_FromDefAndSpec():  set mod->md_def
       10.     PyModule_FromDefAndSpec() -> _add_methods_to_object()
       11.     PyModule_FromDefAndSpec() -> PyModule_SetDocString()

    (10). every time:
       1. _imp_exec_dynamic_impl() -> exec_builtin_or_dynamic()
       2.   if mod->md_state == NULL (including if m_size == 0):
            1. exec_builtin_or_dynamic() -> PyModule_ExecDef()
            2.   PyModule_ExecDef():  allocate mod->md_state
            3.   if there's a Py_mod_exec slot:
                 1. PyModule_ExecDef():  call its function
 */


/* Make sure name is fully qualified.

   This is a bit of a hack: when the shared library is loaded,
   the module name is "package.module", but the module calls
   PyModule_Create*() with just "module" for the name.  The shared
   library loader squirrels away the true name of the module in
   _PyRuntime.imports.pkgcontext, and PyModule_Create*() will
   substitute this (if the name actually matches).
*/

#ifdef HAVE_THREAD_LOCAL
;
# undef PKGCONTEXT
# define PKGCONTEXT pkgcontext
#endif





#ifdef HAVE_DLOPEN



#endif  // HAVE_DLOPEN


/* Common implementation for _imp.exec_dynamic and _imp.exec_builtin */



static int clear_singlephase_extension(PyInterpreterState *interp,
                                       PyObject *name, PyObject *filename);

// Currently, this is only used for testing.
// (See _testinternalcapi.clear_extension().)



/*******************/

#if defined(__EMSCRIPTEN__) && defined(PY_CALL_TRAMPOLINE)
#include <emscripten.h>
EM_JS(PyObject*, _PyImport_InitFunc_TrampolineCall, (PyModInitFunction func), {
    return wasmTable.get(func)();
});
#endif // __EMSCRIPTEN__ && PY_CALL_TRAMPOLINE


/*****************************/
/* single-phase init modules */
/*****************************/

/*
We support a number of kinds of single-phase init builtin/extension modules:

* "basic"
    * no module state (PyModuleDef.m_size == -1)
    * does not support repeated init (we use PyModuleDef.m_base.m_copy)
    * may have process-global state
    * the module's def is cached in _PyRuntime.imports.extensions,
      by (name, filename)
* "reinit"
    * no module state (PyModuleDef.m_size == 0)
    * supports repeated init (m_copy is never used)
    * should not have any process-global state
    * its def is never cached in _PyRuntime.imports.extensions
      (except, currently, under the main interpreter, for some reason)
* "with state"  (almost the same as reinit)
    * has module state (PyModuleDef.m_size > 0)
    * supports repeated init (m_copy is never used)
    * should not have any process-global state
    * its def is never cached in _PyRuntime.imports.extensions
      (except, currently, under the main interpreter, for some reason)

There are also variants within those classes:

* two or more modules share a PyModuleDef
    * a module's init func uses another module's PyModuleDef
    * a module's init func calls another's module's init func
    * a module's init "func" is actually a variable statically initialized
      to another module's init func
* two or modules share "methods"
    * a module's init func copies another module's PyModuleDef
      (with a different name)
* (basic-only) two or modules share process-global state

In the first case, where modules share a PyModuleDef, the following
notable weirdness happens:

* the module's __name__ matches the def, not the requested name
* the last module (with the same def) to be imported for the first time wins
    * returned by PyState_Find_Module() (via interp->modules_by_index)
    * (non-basic-only) its init func is used when re-loading any of them
      (via the def's m_init)
    * (basic-only) the copy of its __dict__ is used when re-loading any of them
      (via the def's m_copy)

However, the following happens as expected:

* a new module object (with its own __dict__) is created for each request
* the module's __spec__ has the requested name
* the loaded module is cached in sys.modules under the requested name
* the m_index field of the shared def is not changed,
  so at least PyState_FindModule() will always look in the same place

For "basic" modules there are other quirks:

* (whether sharing a def or not) when loaded the first time,
  m_copy is set before _init_module_attrs() is called
  in importlib._bootstrap.module_from_spec(),
  so when the module is re-loaded, the previous value
  for __wpec__ (and others) is reset, possibly unexpectedly.

Generally, when multiple interpreters are involved, some of the above
gets even messier.
*/





/* Magic for extension modules (built-in as well as dynamically
   loaded).  To prevent initializing an extension module more than
   once, we keep a static dictionary 'extensions' keyed by the tuple
   (module name, module name)  (for built-in modules) or by
   (filename, module name) (for dynamically loaded modules), containing these
   modules.  A copy of the module's dictionary is stored by calling
   _PyImport_FixupExtensionObject() immediately after the module initialization
   function succeeds.  A copy can be retrieved from there by calling
   import_find_extension().

   Modules which do support multiple initialization set their m_size
   field to a non-negative number (indicating the size of the
   module-specific state). They are still recorded in the extensions
   dictionary, to avoid loading shared libraries twice.
*/









#define HTSEP ':'









#undef HTSEP




















/*******************/
/* builtin modules */
/*******************/



/* Helper to test for built-in module */






/*****************************/
/* the builtin modules table */
/*****************************/

/* API for embedding applications that want to add their own entries
   to the table of built-in modules.  This should normally be called
   *before* Py_Initialize().  When the table resize fails, -1 is
   returned and the existing table is unchanged.

   After a similar function by Just van Rossum. */



/* Shorthand to add a single entry given a name and a function */




/* the internal table */








/********************/
/* the magic number */
/********************/

/* Helper for pythonrun.c -- return magic number and tag. */




extern const char * _PySys_ImplCacheTag;




/*********************************/
/* a Python module's code object */
/*********************************/

/* Execute a code object in a module and return the module object
 * WITH INCREMENTED REFERENCE COUNT.  If an error occurs, name is
 * removed from sys.modules, to avoid leaving damaged module objects
 * in sys.modules.  The caller may wish to restore the original
 * module object (if any) in this case; PyImport_ReloadModule is an
 * example.
 *
 * Note that PyImport_ExecCodeModuleWithPathnames() is the preferred, richer
 * interface.  The other two exist primarily for backward compatibility.
 */


















/******************/
/* frozen modules */
/******************/

/* Return true if the name is an alias.  In that case, "alias" is set
   to the original module name.  If it is an alias but the original
   module isn't known then "alias" is set to NULL while true is returned. */






typedef enum {
    FROZEN_OKAY,
    FROZEN_BAD_NAME,    // The given module name wasn't valid.
    FROZEN_NOT_FOUND,   // It wasn't in PyImport_FrozenModules.
    FROZEN_DISABLED,    // -X frozen_modules=off (and not essential)
    FROZEN_EXCLUDED,    /* The PyImport_FrozenModules entry has NULL "code"
                           (module is present but marked as unimportable, stops search). */
    FROZEN_INVALID,     /* The PyImport_FrozenModules entry is bogus
                           (eg. does not contain executable code). */
} frozen_status;





struct frozen_info {
    PyObject *nameobj;
    const char *data;
    PyObject *(*get_code)(void);
    Py_ssize_t size;
    bool is_package;
    bool is_alias;
    const char *origname;
};






/* Initialize a frozen module.
   Return 1 for success, 0 if the module is not found, and -1 with
   an exception set if the initialization failed.
   This function is also used from frozenmain.c */






/*************/
/* importlib */
/*************/

/* Import the _imp extension by calling manually _imp.create_builtin() and
   _imp.exec_builtin() since importlib is not initialized yet. Initializing
   importlib requires the _imp module: this function fix the bootstrap issue.
 */


/* Global initializations.  Can be undone by Py_FinalizeEx().  Don't
   call this twice without an intervening Py_FinalizeEx() call.  When
   initializations fail, a fatal error is issued and the function does
   not return.  On return, the first thread and interpreter state have
   been created.

   Locking: you must hold the interpreter lock while calling this.
   (If the lock has not yet been initialized, that's equivalent to
   having the lock, but you cannot use multiple threads.)

*/














/*******************/

/* Return a finder object for a sys.path/pkg.__path__ item 'p',
   possibly by fetching it from the path_importer_cache dict. If it
   wasn't yet cached, traverse path_hooks until a hook is found
   that can handle the path item. Return None if no hook could;
   this tells our caller that the path based finder could not find
   a finder for this path item. Cache the result in
   path_importer_cache. */






/*********************/
/* importing modules */
/*********************/



int
_PyImport_IsDefaultImportFunc(PyInterpreterState *interp, PyObject *func)
{
    return func == IMPORT_FUNC(interp);
}


/* Import a module, either built-in, frozen, or external, and return
   its module object WITH INCREMENTED REFERENCE COUNT */




/* Import a module without blocking
 *
 * At first it tries to fetch the module from sys.modules. If the module was
 * never loaded before it loads it with PyImport_ImportModule() unless another
 * thread holds the import lock. In the latter case the function raises an
 * ImportError instead of blocking.
 *
 * Returns the module object with incremented ref count.
 */



/* Remove importlib frames from the traceback,
 * except in Verbose mode. */












/* Re-import a module of any kind and return its module object, WITH
   INCREMENTED REFERENCE COUNT */




/* Higher-level import emulator which emulates the "import" statement
   more accurately -- it invokes the __import__() function from the
   builtins of the current globals.  This means that the import is
   done using whatever import hooks are installed in the current
   environment.
   A dummy list ["__doc__"] is passed as the 4th argument so that
   e.g. PyImport_Import(PyUnicode_FromString("win32com.client.gencache"))
   will return <module "gencache"> instead of <module "win32com">. */




/*********************/
/* runtime lifecycle */
/*********************/








/*************************/
/* interpreter lifecycle */
/*************************/



/* In some corner cases it is important to be sure that the import
   machinery has been initialized (or not cleaned up yet).  For
   example, see issue #4236 and PyModule_Create2(). */



/* Clear the direct per-interpreter import state, if not cleared already. */




// XXX Add something like _PyImport_Disable() for use early in interp fini?


/* "external" imports */








/******************/
/* module helpers */
/******************/






/**************/
/* the module */
/**************/

/*[clinic input]
_imp.lock_held

Return True if the import lock is currently held, else False.

On platforms without threads, return False.
[clinic start generated code]*/



/*[clinic input]
_imp.acquire_lock

Acquires the interpreter's import lock for the current thread.

This lock should be used by import hooks to ensure thread-safety when importing
modules. On platforms without threads, this function does nothing.
[clinic start generated code]*/



/*[clinic input]
_imp.release_lock

Release the interpreter's import lock.

On platforms without threads, this function does nothing.
[clinic start generated code]*/




/*[clinic input]
_imp._fix_co_filename

    code: object(type="PyCodeObject *", subclass_of="&PyCode_Type")
        Code object to change.

    path: unicode
        File path to use.
    /

Changes code.co_filename to specify the passed-in file path.
[clinic start generated code]*/




/*[clinic input]
_imp.create_builtin

    spec: object
    /

Create an extension module.
[clinic start generated code]*/




/*[clinic input]
_imp.extension_suffixes

Returns the list of file suffixes used to identify extension modules.
[clinic start generated code]*/



/*[clinic input]
_imp.init_frozen

    name: unicode
    /

Initializes a frozen module.
[clinic start generated code]*/



/*[clinic input]
_imp.find_frozen

    name: unicode
    /
    *
    withdata: bool = False

Return info about the corresponding frozen module (if there is one) or None.

The returned info (a 2-tuple):

 * data         the raw marshalled bytes
 * is_package   whether or not it is a package
 * origname     the originally frozen module's name, or None if not
                a stdlib module (this will usually be the same as
                the module's current name)
[clinic start generated code]*/



/*[clinic input]
_imp.get_frozen_object

    name: unicode
    data as dataobj: object = None
    /

Create a code object for a frozen module.
[clinic start generated code]*/



/*[clinic input]
_imp.is_frozen_package

    name: unicode
    /

Returns True if the module name is of a frozen package.
[clinic start generated code]*/



/*[clinic input]
_imp.is_builtin

    name: unicode
    /

Returns True if the module name corresponds to a built-in module.
[clinic start generated code]*/



/*[clinic input]
_imp.is_frozen

    name: unicode
    /

Returns True if the module name corresponds to a frozen module.
[clinic start generated code]*/



/*[clinic input]
_imp._frozen_module_names

Returns the list of available frozen modules.
[clinic start generated code]*/



/*[clinic input]
_imp._override_frozen_modules_for_tests

    override: int
    /

(internal-only) Override PyConfig.use_frozen_modules.

(-1: "off", 1: "on", 0: no override)
See frozen_modules() in Lib/test/support/import_helper.py.
[clinic start generated code]*/



/*[clinic input]
_imp._override_multi_interp_extensions_check

    override: int
    /

(internal-only) Override PyInterpreterConfig.check_multi_interp_extensions.

(-1: "never", 1: "always", 0: no override)
[clinic start generated code]*/



#ifdef HAVE_DYNAMIC_LOADING

/*[clinic input]
_imp.create_dynamic

    spec: object
    file: object = NULL
    /

Create an extension module.
[clinic start generated code]*/



/*[clinic input]
_imp.exec_dynamic -> int

    mod: object
    /

Initialize an extension module.
[clinic start generated code]*/




#endif /* HAVE_DYNAMIC_LOADING */

/*[clinic input]
_imp.exec_builtin -> int

    mod: object
    /

Initialize a built-in module.
[clinic start generated code]*/



/*[clinic input]
_imp.source_hash

    key: long
    source: Py_buffer
[clinic start generated code]*/




PyDoc_STRVAR(doc_imp,
"(Extremely) low-level import machinery bits as used by importlib.");

;





;

;




#ifdef __cplusplus
}
#endif
