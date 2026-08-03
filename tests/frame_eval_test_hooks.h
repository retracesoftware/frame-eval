#ifndef FRAME_EVAL_TEST_HOOKS_H
#define FRAME_EVAL_TEST_HOOKS_H

#include "Python.h"

PyObject *frame_eval_test(PyThreadState *, struct _PyInterpreterFrame *, int);
PyObject *frame_eval(PyThreadState *, struct _PyInterpreterFrame *, int);

void frame_eval_test_eval_enter(void);
void frame_eval_test_eval_exit(void);
void frame_eval_test_instruction(void);
void frame_eval_test_backward_jump(void);
void frame_eval_test_frame_push(void);
void frame_eval_test_frame_pop(void);
void frame_eval_test_native_call_pre(void);
void frame_eval_test_native_call_post(void);

#define FRAME_EVAL_HOOK_EVAL_ENTER() frame_eval_test_eval_enter()
#define FRAME_EVAL_HOOK_EVAL_EXIT() frame_eval_test_eval_exit()
#define FRAME_EVAL_HOOK_INSTRUCTION() frame_eval_test_instruction()
#define FRAME_EVAL_HOOK_BACKWARD_JUMP() frame_eval_test_backward_jump()
#define FRAME_EVAL_HOOK_FRAME_PUSH(new_frame) \
    ((void)(new_frame), frame_eval_test_frame_push())
#define FRAME_EVAL_HOOK_FRAME_POP() frame_eval_test_frame_pop()
#define FRAME_EVAL_HOOK_NATIVE_CALL_PRE() frame_eval_test_native_call_pre()
#define FRAME_EVAL_HOOK_NATIVE_CALL_POST() frame_eval_test_native_call_post()

#define FRAME_EVAL_SPECIALIZATION_ALLOWED_INTERP(interp) \
    ((interp)->eval_frame == NULL || (interp)->eval_frame == frame_eval || \
     (interp)->eval_frame == frame_eval_test)
#define FRAME_EVAL_SPECIALIZATION_ALLOWED(tstate) \
    FRAME_EVAL_SPECIALIZATION_ALLOWED_INTERP((tstate)->interp)

#endif
