/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif


PyDoc_STRVAR(_imp_lock_held__doc__,
"lock_held($module, /)\n"
"--\n"
"\n"
"Return True if the import lock is currently held, else False.\n"
"\n"
"On platforms without threads, return False.");

#define _IMP_LOCK_HELD_METHODDEF    \
    {"lock_held", (PyCFunction)_imp_lock_held, METH_NOARGS, _imp_lock_held__doc__},

static PyObject *
_imp_lock_held_impl(PyObject *module);



PyDoc_STRVAR(_imp_acquire_lock__doc__,
"acquire_lock($module, /)\n"
"--\n"
"\n"
"Acquires the interpreter\'s import lock for the current thread.\n"
"\n"
"This lock should be used by import hooks to ensure thread-safety when importing\n"
"modules. On platforms without threads, this function does nothing.");

#define _IMP_ACQUIRE_LOCK_METHODDEF    \
    {"acquire_lock", (PyCFunction)_imp_acquire_lock, METH_NOARGS, _imp_acquire_lock__doc__},

static PyObject *
_imp_acquire_lock_impl(PyObject *module);



PyDoc_STRVAR(_imp_release_lock__doc__,
"release_lock($module, /)\n"
"--\n"
"\n"
"Release the interpreter\'s import lock.\n"
"\n"
"On platforms without threads, this function does nothing.");

#define _IMP_RELEASE_LOCK_METHODDEF    \
    {"release_lock", (PyCFunction)_imp_release_lock, METH_NOARGS, _imp_release_lock__doc__},

static PyObject *
_imp_release_lock_impl(PyObject *module);



PyDoc_STRVAR(_imp__fix_co_filename__doc__,
"_fix_co_filename($module, code, path, /)\n"
"--\n"
"\n"
"Changes code.co_filename to specify the passed-in file path.\n"
"\n"
"  code\n"
"    Code object to change.\n"
"  path\n"
"    File path to use.");

#define _IMP__FIX_CO_FILENAME_METHODDEF    \
    {"_fix_co_filename", _PyCFunction_CAST(_imp__fix_co_filename), METH_FASTCALL, _imp__fix_co_filename__doc__},

static PyObject *
_imp__fix_co_filename_impl(PyObject *module, PyCodeObject *code,
                           PyObject *path);



PyDoc_STRVAR(_imp_create_builtin__doc__,
"create_builtin($module, spec, /)\n"
"--\n"
"\n"
"Create an extension module.");

#define _IMP_CREATE_BUILTIN_METHODDEF    \
    {"create_builtin", (PyCFunction)_imp_create_builtin, METH_O, _imp_create_builtin__doc__},

PyDoc_STRVAR(_imp_extension_suffixes__doc__,
"extension_suffixes($module, /)\n"
"--\n"
"\n"
"Returns the list of file suffixes used to identify extension modules.");

#define _IMP_EXTENSION_SUFFIXES_METHODDEF    \
    {"extension_suffixes", (PyCFunction)_imp_extension_suffixes, METH_NOARGS, _imp_extension_suffixes__doc__},

static PyObject *
_imp_extension_suffixes_impl(PyObject *module);



PyDoc_STRVAR(_imp_init_frozen__doc__,
"init_frozen($module, name, /)\n"
"--\n"
"\n"
"Initializes a frozen module.");

#define _IMP_INIT_FROZEN_METHODDEF    \
    {"init_frozen", (PyCFunction)_imp_init_frozen, METH_O, _imp_init_frozen__doc__},

static PyObject *
_imp_init_frozen_impl(PyObject *module, PyObject *name);



PyDoc_STRVAR(_imp_find_frozen__doc__,
"find_frozen($module, name, /, *, withdata=False)\n"
"--\n"
"\n"
"Return info about the corresponding frozen module (if there is one) or None.\n"
"\n"
"The returned info (a 2-tuple):\n"
"\n"
" * data         the raw marshalled bytes\n"
" * is_package   whether or not it is a package\n"
" * origname     the originally frozen module\'s name, or None if not\n"
"                a stdlib module (this will usually be the same as\n"
"                the module\'s current name)");

#define _IMP_FIND_FROZEN_METHODDEF    \
    {"find_frozen", _PyCFunction_CAST(_imp_find_frozen), METH_FASTCALL|METH_KEYWORDS, _imp_find_frozen__doc__},

static PyObject *
_imp_find_frozen_impl(PyObject *module, PyObject *name, int withdata);



PyDoc_STRVAR(_imp_get_frozen_object__doc__,
"get_frozen_object($module, name, data=None, /)\n"
"--\n"
"\n"
"Create a code object for a frozen module.");

#define _IMP_GET_FROZEN_OBJECT_METHODDEF    \
    {"get_frozen_object", _PyCFunction_CAST(_imp_get_frozen_object), METH_FASTCALL, _imp_get_frozen_object__doc__},

static PyObject *
_imp_get_frozen_object_impl(PyObject *module, PyObject *name,
                            PyObject *dataobj);



PyDoc_STRVAR(_imp_is_frozen_package__doc__,
"is_frozen_package($module, name, /)\n"
"--\n"
"\n"
"Returns True if the module name is of a frozen package.");

#define _IMP_IS_FROZEN_PACKAGE_METHODDEF    \
    {"is_frozen_package", (PyCFunction)_imp_is_frozen_package, METH_O, _imp_is_frozen_package__doc__},

static PyObject *
_imp_is_frozen_package_impl(PyObject *module, PyObject *name);



PyDoc_STRVAR(_imp_is_builtin__doc__,
"is_builtin($module, name, /)\n"
"--\n"
"\n"
"Returns True if the module name corresponds to a built-in module.");

#define _IMP_IS_BUILTIN_METHODDEF    \
    {"is_builtin", (PyCFunction)_imp_is_builtin, METH_O, _imp_is_builtin__doc__},

static PyObject *
_imp_is_builtin_impl(PyObject *module, PyObject *name);



PyDoc_STRVAR(_imp_is_frozen__doc__,
"is_frozen($module, name, /)\n"
"--\n"
"\n"
"Returns True if the module name corresponds to a frozen module.");

#define _IMP_IS_FROZEN_METHODDEF    \
    {"is_frozen", (PyCFunction)_imp_is_frozen, METH_O, _imp_is_frozen__doc__},

static PyObject *
_imp_is_frozen_impl(PyObject *module, PyObject *name);



PyDoc_STRVAR(_imp__frozen_module_names__doc__,
"_frozen_module_names($module, /)\n"
"--\n"
"\n"
"Returns the list of available frozen modules.");

#define _IMP__FROZEN_MODULE_NAMES_METHODDEF    \
    {"_frozen_module_names", (PyCFunction)_imp__frozen_module_names, METH_NOARGS, _imp__frozen_module_names__doc__},

static PyObject *
_imp__frozen_module_names_impl(PyObject *module);



PyDoc_STRVAR(_imp__override_frozen_modules_for_tests__doc__,
"_override_frozen_modules_for_tests($module, override, /)\n"
"--\n"
"\n"
"(internal-only) Override PyConfig.use_frozen_modules.\n"
"\n"
"(-1: \"off\", 1: \"on\", 0: no override)\n"
"See frozen_modules() in Lib/test/support/import_helper.py.");

#define _IMP__OVERRIDE_FROZEN_MODULES_FOR_TESTS_METHODDEF    \
    {"_override_frozen_modules_for_tests", (PyCFunction)_imp__override_frozen_modules_for_tests, METH_O, _imp__override_frozen_modules_for_tests__doc__},

static PyObject *
_imp__override_frozen_modules_for_tests_impl(PyObject *module, int override);



PyDoc_STRVAR(_imp__override_multi_interp_extensions_check__doc__,
"_override_multi_interp_extensions_check($module, override, /)\n"
"--\n"
"\n"
"(internal-only) Override PyInterpreterConfig.check_multi_interp_extensions.\n"
"\n"
"(-1: \"never\", 1: \"always\", 0: no override)");

#define _IMP__OVERRIDE_MULTI_INTERP_EXTENSIONS_CHECK_METHODDEF    \
    {"_override_multi_interp_extensions_check", (PyCFunction)_imp__override_multi_interp_extensions_check, METH_O, _imp__override_multi_interp_extensions_check__doc__},

static PyObject *
_imp__override_multi_interp_extensions_check_impl(PyObject *module,
                                                  int override);



#if defined(HAVE_DYNAMIC_LOADING)

PyDoc_STRVAR(_imp_create_dynamic__doc__,
"create_dynamic($module, spec, file=<unrepresentable>, /)\n"
"--\n"
"\n"
"Create an extension module.");

#define _IMP_CREATE_DYNAMIC_METHODDEF    \
    {"create_dynamic", _PyCFunction_CAST(_imp_create_dynamic), METH_FASTCALL, _imp_create_dynamic__doc__},

static PyObject *
_imp_create_dynamic_impl(PyObject *module, PyObject *spec, PyObject *file);



#endif /* defined(HAVE_DYNAMIC_LOADING) */

#if defined(HAVE_DYNAMIC_LOADING)

PyDoc_STRVAR(_imp_exec_dynamic__doc__,
"exec_dynamic($module, mod, /)\n"
"--\n"
"\n"
"Initialize an extension module.");

#define _IMP_EXEC_DYNAMIC_METHODDEF    \
    {"exec_dynamic", (PyCFunction)_imp_exec_dynamic, METH_O, _imp_exec_dynamic__doc__},

static int
_imp_exec_dynamic_impl(PyObject *module, PyObject *mod);



#endif /* defined(HAVE_DYNAMIC_LOADING) */

PyDoc_STRVAR(_imp_exec_builtin__doc__,
"exec_builtin($module, mod, /)\n"
"--\n"
"\n"
"Initialize a built-in module.");

#define _IMP_EXEC_BUILTIN_METHODDEF    \
    {"exec_builtin", (PyCFunction)_imp_exec_builtin, METH_O, _imp_exec_builtin__doc__},

static int
_imp_exec_builtin_impl(PyObject *module, PyObject *mod);



PyDoc_STRVAR(_imp_source_hash__doc__,
"source_hash($module, /, key, source)\n"
"--\n"
"\n");

#define _IMP_SOURCE_HASH_METHODDEF    \
    {"source_hash", _PyCFunction_CAST(_imp_source_hash), METH_FASTCALL|METH_KEYWORDS, _imp_source_hash__doc__},

static PyObject *
_imp_source_hash_impl(PyObject *module, long key, Py_buffer *source);



#ifndef _IMP_CREATE_DYNAMIC_METHODDEF
    #define _IMP_CREATE_DYNAMIC_METHODDEF
#endif /* !defined(_IMP_CREATE_DYNAMIC_METHODDEF) */

#ifndef _IMP_EXEC_DYNAMIC_METHODDEF
    #define _IMP_EXEC_DYNAMIC_METHODDEF
#endif /* !defined(_IMP_EXEC_DYNAMIC_METHODDEF) */
/*[clinic end generated code: output=b18d46e0036eff49 input=a9049054013a1b77]*/
