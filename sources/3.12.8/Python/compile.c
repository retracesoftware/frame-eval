/*
 * This file compiles an abstract syntax tree (AST) into Python bytecode.
 *
 * The primary entry point is _PyAST_Compile(), which returns a
 * PyCodeObject.  The compiler makes several passes to build the code
 * object:
 *   1. Checks for future statements.  See future.c
 *   2. Builds a symbol table.  See symtable.c.
 *   3. Generate an instruction sequence. See compiler_mod() in this file.
 *   4. Generate a control flow graph and run optimizations on it.  See flowgraph.c.
 *   5. Assemble the basic blocks into final code.  See optimize_and_assemble() in
 *      this file, and assembler.c.
 *
 * Note that compiler_mod() suggests module, but the module ast type
 * (mod_ty) has cases for expressions and interactive statements.
 *
 * CAUTION: The VISIT_* macros abort the current function when they
 * encounter a problem. So don't invoke them when there is memory
 * which needs to be released. Code blocks are OK, as the compiler
 * structure takes care of releasing those.  Use the arena to manage
 * objects.
 */

#include <stdbool.h>

#include "Python.h"
#include "pycore_ast.h"           // _PyAST_GetDocString()
#define NEED_OPCODE_TABLES
#include "pycore_opcode_utils.h"
#undef NEED_OPCODE_TABLES
#include "pycore_flowgraph.h"
#include "pycore_code.h"          // _PyCode_New()
#include "pycore_compile.h"
#include "pycore_intrinsics.h"
#include "pycore_long.h"          // _PyLong_GetZero()
#include "pycore_pymem.h"         // _PyMem_IsPtrFreed()
#include "pycore_symtable.h"      // PySTEntryObject, _PyFuture_FromAST()

#include "opcode_metadata.h"      // _PyOpcode_opcode_metadata, _PyOpcode_num_popped/pushed

#define DEFAULT_CODE_SIZE 128
#define DEFAULT_LNOTAB_SIZE 16
#define DEFAULT_CNOTAB_SIZE 32

#define COMP_GENEXP   0
#define COMP_LISTCOMP 1
#define COMP_SETCOMP  2
#define COMP_DICTCOMP 3

/* A soft limit for stack use, to avoid excessive
 * memory use for large constants, etc.
 *
 * The value 30 is plucked out of thin air.
 * Code that could use more stack than this is
 * rare, so the exact value is unimportant.
 */
#define STACK_USE_GUIDELINE 30

#undef SUCCESS
#undef ERROR
#define SUCCESS 0
#define ERROR -1

#define RETURN_IF_ERROR(X)  \
    if ((X) == -1) {        \
        return ERROR;       \
    }

/* If we exceed this limit, it should
 * be considered a compiler bug.
 * Currently it should be impossible
 * to exceed STACK_USE_GUIDELINE * 100,
 * as 100 is the maximum parse depth.
 * For performance reasons we will
 * want to reduce this to a
 * few hundred in the future.
 *
 * NOTE: Whatever MAX_ALLOWED_STACK_USE is
 * set to, it should never restrict what Python
 * we can write, just how we compile it.
 */
#define MAX_ALLOWED_STACK_USE (STACK_USE_GUIDELINE * 100)

#define IS_TOP_LEVEL_AWAIT(C) ( \
        ((C)->c_flags.cf_flags & PyCF_ALLOW_TOP_LEVEL_AWAIT) \
        && ((C)->u->u_ste->ste_type == ModuleBlock))

typedef _PyCompilerSrcLocation location;
typedef _PyCfgInstruction cfg_instr;
typedef _PyCfgBasicblock basicblock;
typedef _PyCfgBuilder cfg_builder;

#define LOCATION(LNO, END_LNO, COL, END_COL) \
    ((const _PyCompilerSrcLocation){(LNO), (END_LNO), (COL), (END_COL)})

/* Return true if loc1 starts after loc2 ends. */


#define LOC(x) SRC_LOCATION_FROM_AST(x)

typedef _PyCfgJumpTargetLabel jump_target_label;

;

#define SAME_LABEL(L1, L2) ((L1).id == (L2).id)
#define IS_LABEL(L) (!SAME_LABEL((L), (NO_LABEL)))

#define NEW_JUMP_TARGET_LABEL(C, NAME) \
    jump_target_label NAME = instr_sequence_new_label(INSTR_SEQUENCE(C)); \
    if (!IS_LABEL(NAME)) { \
        return ERROR; \
    }

#define USE_LABEL(C, LBL) \
    RETURN_IF_ERROR(instr_sequence_use_label(INSTR_SEQUENCE(C), (LBL).id))


/* fblockinfo tracks the current frame block.

A frame block is used to handle loops, try/except, and try/finally.
It's called a frame block to distinguish it from a basic block in the
compiler IR.
*/

enum fblocktype { WHILE_LOOP, FOR_LOOP, TRY_EXCEPT, FINALLY_TRY, FINALLY_END,
                  WITH, ASYNC_WITH, HANDLER_CLEANUP, POP_VALUE, EXCEPTION_HANDLER,
                  EXCEPTION_GROUP_HANDLER, ASYNC_COMPREHENSION_GENERATOR,
                  STOP_ITERATION };

struct fblockinfo {
    enum fblocktype fb_type;
    jump_target_label fb_block;
    location fb_loc;
    /* (optional) type-specific exit or cleanup block */
    jump_target_label fb_exit;
    /* (optional) additional information required for unwinding */
    void *fb_datum;
};

enum {
    COMPILER_SCOPE_MODULE,
    COMPILER_SCOPE_CLASS,
    COMPILER_SCOPE_FUNCTION,
    COMPILER_SCOPE_ASYNC_FUNCTION,
    COMPILER_SCOPE_LAMBDA,
    COMPILER_SCOPE_COMPREHENSION,
    COMPILER_SCOPE_TYPEPARAMS,
};




typedef _PyCompile_Instruction instruction;
typedef _PyCompile_InstructionSequence instr_sequence;

#define INITIAL_INSTR_SEQUENCE_SIZE 100
#define INITIAL_INSTR_SEQUENCE_LABELS_MAP_SIZE 10

/*
 * Resize the array if index is out of range.
 *
 * idx: the index we want to access
 * arr: pointer to the array
 * alloc: pointer to the capacity of the array
 * default_alloc: initial number of items
 * item_size: size of each item
 *
 */

















/* The following items change on entry and exit of code blocks.
   They must be saved and restored when returning to a block.
*/
struct compiler_unit {
    PySTEntryObject *u_ste;

    int u_scope_type;

    PyObject *u_private;        /* for private name mangling */

    instr_sequence u_instr_sequence; /* codegen output */

    int u_nfblocks;
    int u_in_inlined_comp;

    struct fblockinfo u_fblock[CO_MAXBLOCKS];

    _PyCompile_CodeUnitMetadata u_metadata;
};

/* This struct captures the global state of a compilation.

The u pointer points to the current compilation unit, while units
for enclosing blocks are stored in c_stack.     The u and c_stack are
managed by compiler_enter_scope() and compiler_exit_scope().

Note that we don't track recursion levels during compilation - the
task of detecting and rejecting excessive levels of nesting is
handled by the symbol analysis pass.

*/

struct compiler {
    PyObject *c_filename;
    struct symtable *c_st;
    PyFutureFeatures c_future;   /* module's __future__ */
    PyCompilerFlags c_flags;

    int c_optimize;              /* optimization level */
    int c_interactive;           /* true if in interactive mode */
    int c_nestlevel;
    PyObject *c_const_cache;     /* Python dict holding all constants,
                                    including names tuple */
    struct compiler_unit *u; /* compiler state for current block */
    PyObject *c_stack;           /* Python list holding compiler_unit ptrs */
    PyArena *c_arena;            /* pointer to memory allocation arena */
};

#define INSTR_SEQUENCE(C) (&((C)->u->u_instr_sequence))


typedef struct {
    // A list of strings corresponding to name captures. It is used to track:
    // - Repeated name assignments in the same pattern.
    // - Different name assignments in alternatives.
    // - The order of name assignments in alternatives.
    PyObject *stores;
    // If 0, any name captures against our subject will raise.
    int allow_irrefutable;
    // An array of blocks to jump to on failure. Jumping to fail_pop[i] will pop
    // i items off of the stack. The end result looks like this (with each block
    // falling through to the next):
    // fail_pop[4]: POP_TOP
    // fail_pop[3]: POP_TOP
    // fail_pop[2]: POP_TOP
    // fail_pop[1]: POP_TOP
    // fail_pop[0]: NOP
    jump_target_label *fail_pop;
    // The current length of fail_pop.
    Py_ssize_t fail_pop_size;
    // The number of items on top of the stack that need to *stay* on top of the
    // stack. Variable captures go beneath these. All of them will be popped on
    // failure.
    Py_ssize_t on_top;
} pattern_context;

static int codegen_addop_i(instr_sequence *seq, int opcode, Py_ssize_t oparg, location loc);

static void compiler_free(struct compiler *);
static int compiler_error(struct compiler *, location loc, const char *, ...);
static int compiler_warn(struct compiler *, location loc, const char *, ...);
static int compiler_nameop(struct compiler *, location, identifier, expr_context_ty);

static PyCodeObject *compiler_mod(struct compiler *, mod_ty);
static int compiler_visit_stmt(struct compiler *, stmt_ty);
static int compiler_visit_keyword(struct compiler *, keyword_ty);
static int compiler_visit_expr(struct compiler *, expr_ty);
static int compiler_augassign(struct compiler *, stmt_ty);
static int compiler_annassign(struct compiler *, stmt_ty);
static int compiler_subscript(struct compiler *, expr_ty);
static int compiler_slice(struct compiler *, expr_ty);

static bool are_all_items_const(asdl_expr_seq *, Py_ssize_t, Py_ssize_t);


static int compiler_with(struct compiler *, stmt_ty, int);
static int compiler_async_with(struct compiler *, stmt_ty, int);
static int compiler_async_for(struct compiler *, stmt_ty);
static int compiler_call_simple_kw_helper(struct compiler *c,
                                          location loc,
                                          asdl_keyword_seq *keywords,
                                          Py_ssize_t nkwelts);
static int compiler_call_helper(struct compiler *c, location loc,
                                int n, asdl_expr_seq *args,
                                asdl_keyword_seq *keywords);
static int compiler_try_except(struct compiler *, stmt_ty);
static int compiler_try_star_except(struct compiler *, stmt_ty);
static int compiler_set_qualname(struct compiler *);

static int compiler_sync_comprehension_generator(
                                      struct compiler *c, location loc,
                                      asdl_comprehension_seq *generators, int gen_index,
                                      int depth,
                                      expr_ty elt, expr_ty val, int type,
                                      int iter_on_stack);

static int compiler_async_comprehension_generator(
                                      struct compiler *c, location loc,
                                      asdl_comprehension_seq *generators, int gen_index,
                                      int depth,
                                      expr_ty elt, expr_ty val, int type,
                                      int iter_on_stack);

static int compiler_pattern(struct compiler *, pattern_ty, pattern_context *);
static int compiler_match(struct compiler *, stmt_ty);
static int compiler_pattern_subpattern(struct compiler *,
                                       pattern_ty, pattern_context *);

static PyCodeObject *optimize_and_assemble(struct compiler *, int addNone);

#define CAPSULE_NAME "compile.c compiler unit"












/* Return new dict containing names from src that match scope(s).

src is a symbol table dictionary.  If the scope of a name matches
either scope_type or flag is set, insert it into the new dict.  The
values are integers, starting at offset and increasing by one for
each key.
*/







/* Return the stack effect of opcode with argument oparg.

   Some opcodes have different stack effect when jump to the target and
   when not jump. The 'jump' parameter specifies the case:

   * 0 -- when not jump
   * 1 -- when jump
   * -1 -- maximal
 */










// Merge const *o* recursively and return constant key object.










/* Add an opcode with an integer argument */




#define RETURN_IF_ERROR_IN_SCOPE(C, CALL) { \
    if ((CALL) < 0) { \
        compiler_exit_scope((C)); \
        return ERROR; \
    } \
}

#define ADDOP(C, LOC, OP) \
    RETURN_IF_ERROR(codegen_addop_noarg(INSTR_SEQUENCE(C), (OP), (LOC)))

#define ADDOP_IN_SCOPE(C, LOC, OP) RETURN_IF_ERROR_IN_SCOPE((C), codegen_addop_noarg(INSTR_SEQUENCE(C), (OP), (LOC)))

#define ADDOP_LOAD_CONST(C, LOC, O) \
    RETURN_IF_ERROR(compiler_addop_load_const((C)->c_const_cache, (C)->u, (LOC), (O)))

/* Same as ADDOP_LOAD_CONST, but steals a reference. */
#define ADDOP_LOAD_CONST_NEW(C, LOC, O) { \
    PyObject *__new_const = (O); \
    if (__new_const == NULL) { \
        return ERROR; \
    } \
    if (compiler_addop_load_const((C)->c_const_cache, (C)->u, (LOC), __new_const) < 0) { \
        Py_DECREF(__new_const); \
        return ERROR; \
    } \
    Py_DECREF(__new_const); \
}

#define ADDOP_N(C, LOC, OP, O, TYPE) { \
    assert(!HAS_CONST(OP)); /* use ADDOP_LOAD_CONST_NEW */ \
    if (compiler_addop_o((C)->u, (LOC), (OP), (C)->u->u_metadata.u_ ## TYPE, (O)) < 0) { \
        Py_DECREF((O)); \
        return ERROR; \
    } \
    Py_DECREF((O)); \
}

#define ADDOP_NAME(C, LOC, OP, O, TYPE) \
    RETURN_IF_ERROR(compiler_addop_name((C)->u, (LOC), (OP), (C)->u->u_metadata.u_ ## TYPE, (O)))

#define ADDOP_I(C, LOC, OP, O) \
    RETURN_IF_ERROR(codegen_addop_i(INSTR_SEQUENCE(C), (OP), (O), (LOC)))

#define ADDOP_JUMP(C, LOC, OP, O) \
    RETURN_IF_ERROR(codegen_addop_j(INSTR_SEQUENCE(C), (LOC), (OP), (O)))

#define ADDOP_COMPARE(C, LOC, CMP) \
    RETURN_IF_ERROR(compiler_addcompare((C), (LOC), (cmpop_ty)(CMP)))

#define ADDOP_BINARY(C, LOC, BINOP) \
    RETURN_IF_ERROR(addop_binary((C), (LOC), (BINOP), false))

#define ADDOP_INPLACE(C, LOC, BINOP) \
    RETURN_IF_ERROR(addop_binary((C), (LOC), (BINOP), true))

#define ADD_YIELD_FROM(C, LOC, await) \
    RETURN_IF_ERROR(compiler_add_yield_from((C), (LOC), (await)))

#define POP_EXCEPT_AND_RERAISE(C, LOC) \
    RETURN_IF_ERROR(compiler_pop_except_and_reraise((C), (LOC)))

#define ADDOP_YIELD(C, LOC) \
    RETURN_IF_ERROR(addop_yield((C), (LOC)))

/* VISIT and VISIT_SEQ takes an ASDL type as their second argument.  They use
   the ASDL name to synthesize the name of the C type and the visit function.
*/

#define VISIT(C, TYPE, V) \
    RETURN_IF_ERROR(compiler_visit_ ## TYPE((C), (V)));

#define VISIT_IN_SCOPE(C, TYPE, V) \
    RETURN_IF_ERROR_IN_SCOPE((C), compiler_visit_ ## TYPE((C), (V)))

#define VISIT_SEQ(C, TYPE, SEQ) { \
    int _i; \
    asdl_ ## TYPE ## _seq *seq = (SEQ); /* avoid variable capture */ \
    for (_i = 0; _i < asdl_seq_LEN(seq); _i++) { \
        TYPE ## _ty elt = (TYPE ## _ty)asdl_seq_GET(seq, _i); \
        RETURN_IF_ERROR(compiler_visit_ ## TYPE((C), elt)); \
    } \
}

#define VISIT_SEQ_IN_SCOPE(C, TYPE, SEQ) { \
    int _i; \
    asdl_ ## TYPE ## _seq *seq = (SEQ); /* avoid variable capture */ \
    for (_i = 0; _i < asdl_seq_LEN(seq); _i++) { \
        TYPE ## _ty elt = (TYPE ## _ty)asdl_seq_GET(seq, _i); \
        if (compiler_visit_ ## TYPE((C), elt) < 0) { \
            compiler_exit_scope(C); \
            return ERROR; \
        } \
    } \
}






/* Search if variable annotations are present statically in a block. */



/*
 * Frame block handling functions
 */











/* Unwind a frame block.  If preserve_tos is true, the TOS before
 * popping the blocks will be restored afterwards, unless another
 * return, break or continue is found. In which case, the TOS will
 * be popped.
 */


/** Unwind block stack. If loop is not NULL, then stop when the first loop is encountered. */


/* Compile a sequence of statements, checking for a docstring
   and for annotations. */







/* The test for LOCAL must come before the test for FREE in order to
   handle classes where name is both local and free.  The local var is
   a method and the free var is a free var referenced within a method.
*/



















































/* Return false if the expression is a constant value except named singletons.
   Return true otherwise. */


static PyTypeObject * infer_type(expr_ty e);

/* Check operands of identity checks ("is" and "is not").
   Emit a warning if any operand is a constant except named singletons.
 */


;



























/* Code generated for "try: <body> finally: <finalbody>" is as follows:

        SETUP_FINALLY           L
        <code for body>
        POP_BLOCK
        <code for finalbody>
        JUMP E
    L:
        <code for finalbody>
    E:

   The special instructions use the block stack.  Each block
   stack entry contains the instruction that created it (here
   SETUP_FINALLY), the level of the value stack at the time the
   block stack entry was created, and a label (here L).

   SETUP_FINALLY:
    Pushes the current value stack level and the label
    onto the block stack.
   POP_BLOCK:
    Pops en entry from the block stack.

   The block stack is unwound when an exception is raised:
   when a SETUP_FINALLY entry is found, the raised and the caught
   exceptions are pushed onto the value stack (and the exception
   condition is cleared), and the interpreter jumps to the label
   gotten from the block stack.
*/






/*
   Code generated for "try: S except E1 as V1: S1 except E2 as V2: S2 ...":
   (The contents of the value stack is shown in [], with the top
   at the right; 'tb' is trace-back info, 'val' the exception's
   associated value, and 'exc' the exception.)

   Value stack          Label   Instruction     Argument
   []                           SETUP_FINALLY   L1
   []                           <code for S>
   []                           POP_BLOCK
   []                           JUMP            L0

   [exc]                L1:     <evaluate E1>           )
   [exc, E1]                    CHECK_EXC_MATCH         )
   [exc, bool]                  POP_JUMP_IF_FALSE L2    ) only if E1
   [exc]                        <assign to V1>  (or POP if no V1)
   []                           <code for S1>
                                JUMP            L0

   [exc]                L2:     <evaluate E2>
   .............................etc.......................

   [exc]                Ln+1:   RERAISE     # re-raise exception

   []                   L0:     <next statement>

   Of course, parts are not generated if Vi or Ei is not present.
*/


/*
   Code generated for "try: S except* E1 as V1: S1 except* E2 as V2: S2 ...":
   (The contents of the value stack is shown in [], with the top
   at the right; 'tb' is trace-back info, 'val' the exception instance,
   and 'typ' the exception's type.)

   Value stack                   Label         Instruction     Argument
   []                                         SETUP_FINALLY         L1
   []                                         <code for S>
   []                                         POP_BLOCK
   []                                         JUMP                  L0

   [exc]                            L1:       BUILD_LIST   )  list for raised/reraised excs ("result")
   [orig, res]                                COPY 2       )  make a copy of the original EG

   [orig, res, exc]                           <evaluate E1>
   [orig, res, exc, E1]                       CHECK_EG_MATCH
   [orig, res, rest/exc, match?]              COPY 1
   [orig, res, rest/exc, match?, match?]      POP_JUMP_IF_NONE      C1

   [orig, res, rest, match]                   <assign to V1>  (or POP if no V1)

   [orig, res, rest]                          SETUP_FINALLY         R1
   [orig, res, rest]                          <code for S1>
   [orig, res, rest]                          JUMP                  L2

   [orig, res, rest, i, v]          R1:       LIST_APPEND   3 ) exc raised in except* body - add to res
   [orig, res, rest, i]                       POP
   [orig, res, rest]                          JUMP                  LE2

   [orig, res, rest]                L2:       NOP  ) for lineno
   [orig, res, rest]                          JUMP                  LE2

   [orig, res, rest/exc, None]      C1:       POP

   [orig, res, rest]               LE2:       <evaluate E2>
   .............................etc.......................

   [orig, res, rest]                Ln+1:     LIST_APPEND 1  ) add unhandled exc to res (could be None)

   [orig, res]                                CALL_INTRINSIC_2 PREP_RERAISE_STAR
   [exc]                                      COPY 1
   [exc, exc]                                 POP_JUMP_IF_NOT_NONE  RER
   [exc]                                      POP_TOP
   []                                         JUMP                  L0

   [exc]                            RER:      SWAP 2
   [exc, prev_exc_info]                       POP_EXCEPT
   [exc]                                      RERAISE               0

   []                               L0:       <next statement>
*/































































// If an attribute access spans multiple lines, update the current start
// location to point to the attribute name.


// Return 1 if the method call was optimized, 0 if not, and -1 on error.








/* Used to implement f-strings. Format a single value. */




/* Used by compiler_call_helper and maybe_optimize_method_call to emit
 * KW_NAMES before CALL.
 */



/* shared code between compiler_call and compiler_class */



/* List and set comprehensions work by being inlined at the location where
  they are defined. The isolation of iteration variables is provided by
  pushing/popping clashing locals on the stack. Generator expressions work
  by creating a nested function to perform the actual iteration.
  This means that the iteration variables don't leak into the current scope.
  See https://peps.python.org/pep-0709/ for additional information.
  The defined function is called immediately following its definition, with the
  result of that call being the result of the expression.
  The LC/SC version returns the populated container, while the GE version is
  flagged in symtable.c as a generator, so it returns the generator object
  when the function is called.

  Possible cleanups:
    - iterate over the generator sequence instead of using recursion
*/








typedef struct {
    PyObject *pushed_locals;
    PyObject *temp_symbols;
    PyObject *fast_hidden;
    jump_target_label cleanup;
    jump_target_label end;
} inlined_comprehension_state;


























/*
   Implements the async with statement.

   The semantics outlined in that PEP are as follows:

   async with EXPR as VAR:
       BLOCK

   It is implemented roughly as:

   context = EXPR
   exit = context.__aexit__  # not calling it
   value = await context.__aenter__()
   try:
       VAR = value  # if VAR present in the syntax
       BLOCK
   finally:
       if an exception was raised:
           exc = copy of (exception, instance, traceback)
       else:
           exc = (None, None, None)
       if not (await exit(*exc)):
           raise
 */



/*
   Implements the with statement from PEP 343.
   with EXPR as VAR:
       BLOCK
   is implemented as:
        <code for EXPR>
        SETUP_WITH  E
        <code to store to VAR> or POP_TOP
        <code for BLOCK>
        LOAD_CONST (None, None, None)
        CALL_FUNCTION_EX 0
        JUMP  EXIT
    E:  WITH_EXCEPT_START (calls EXPR.__exit__)
        POP_JUMP_IF_TRUE T:
        RERAISE
    T:  POP_TOP (remove exception from stack)
        POP_EXCEPT
        POP_TOP
    EXIT:
 */



















/* Raises a SyntaxError and returns 0.
   If something goes wrong, a different exception may be raised.
*/



/* Emits a SyntaxWarning and returns 1 on success.
   If a SyntaxWarning raised as error, replaces it with a SyntaxError
   and returns 0.
*/




/* Returns the number of the values emitted,
 * thus are needed to build the slice, or -1 if there is an error. */



// PEP 634: Structural Pattern Matching

// To keep things simple, all compiler_pattern_* and pattern_helper_* routines
// follow the convention of consuming TOS (the subject for the given pattern)
// and calling jump_to_fail_pop on failure (no match).

// When calling into these routines, it's important that pc->on_top be kept
// updated to reflect the current number of items that we are using on the top
// of the stack: they will be popped on failure, and any name captures will be
// stored *underneath* them on success. This lets us defer all names stores
// until the *entire* pattern matches.

#define WILDCARD_CHECK(N) \
    ((N)->kind == MatchAs_kind && !(N)->v.MatchAs.name)

#define WILDCARD_STAR_CHECK(N) \
    ((N)->kind == MatchStar_kind && !(N)->v.MatchStar.name)

// Limit permitted subexpressions, even if the parser & AST validator let them through
#define MATCH_VALUE_EXPR(N) \
    ((N)->kind == Constant_kind || (N)->kind == Attribute_kind)

// Allocate or resize pc->fail_pop to allow for n items to be popped on failure.


// Use op to jump to the correct fail_pop block.


// Build all of the fail_pop blocks and reset fail_pop.




// Duplicate the effect of 3.10's ROT_* instructions using SWAPs.









// Like pattern_helper_sequence_unpack, but uses BINARY_SUBSCR instead of
// UNPACK_SEQUENCE / UNPACK_EX. This is more efficient for patterns with a
// starred wildcard like [first, *_] / [first, *_, last] / [*_, last] / etc.


// Like compiler_pattern, but turn off checks for irrefutability.



























#undef WILDCARD_CHECK
#undef WILDCARD_STAR_CHECK





// Merge *obj* with constant cache.
// Unlike merge_consts_recursive(), this function doesn't work recursively.














static int cfg_to_instr_sequence(cfg_builder *g, instr_sequence *seq);








/* Access to compiler optimizations for unit tests.
 *
 * _PyCompile_CodeGen takes and AST, applies code-gen and
 * returns the unoptimized CFG as an instruction list.
 *
 * _PyCompile_OptimizeCfg takes an instruction list, constructs
 * a CFG, optimizes it and converts back to an instruction list.
 *
 * An instruction list is a PyList where each item is either
 * a tuple describing a single instruction:
 * (opcode, oparg, lineno, end_lineno, col, end_col), or
 * a jump target label marking the beginning of a basic block.
 */













int _PyCfg_JumpLabelsToTargets(basicblock *entryblock);




/* Retained for API compatibility.
 * Optimization is now done in _PyCfg_OptimizeCodeUnit */


