#ifndef FRAME_EVAL_H
#define FRAME_EVAL_H

#include "Python.h"

extern PyDictKeysObject *frame_eval_empty_keys;
extern PyObject *frame_eval_monitoring_disable;
extern PyObject *frame_eval_monitoring_missing;
extern PyTypeObject *frame_eval_typealias_type;
extern PyTypeObject *frame_eval_union_type;
extern getattrofunc frame_eval_slot_tp_getattr_hook;
extern getattrofunc frame_eval_slot_tp_getattro;
extern vectorcallfunc frame_eval_function_vectorcall;

PyObject *frame_eval(PyThreadState *, struct _PyInterpreterFrame *, int);
int frame_eval_init(void);

#ifndef FRAME_EVAL_HOOK_EVAL_ENTER
#define FRAME_EVAL_HOOK_EVAL_ENTER() ((void)0)
#endif
#ifndef FRAME_EVAL_HOOK_EVAL_EXIT
#define FRAME_EVAL_HOOK_EVAL_EXIT() ((void)0)
#endif
#ifndef FRAME_EVAL_HOOK_INSTRUCTION
#define FRAME_EVAL_HOOK_INSTRUCTION() ((void)0)
#endif
#ifndef FRAME_EVAL_HOOK_BACKWARD_JUMP
#define FRAME_EVAL_HOOK_BACKWARD_JUMP() ((void)0)
#endif
#ifndef FRAME_EVAL_HOOK_FRAME_PUSH
#define FRAME_EVAL_HOOK_FRAME_PUSH(new_frame) ((void)(new_frame))
#endif
#ifndef FRAME_EVAL_HOOK_FRAME_POP
#define FRAME_EVAL_HOOK_FRAME_POP() ((void)0)
#endif
#ifndef FRAME_EVAL_HOOK_NATIVE_CALL_PRE
#define FRAME_EVAL_HOOK_NATIVE_CALL_PRE() ((void)0)
#endif
#ifndef FRAME_EVAL_HOOK_NATIVE_CALL_POST
#define FRAME_EVAL_HOOK_NATIVE_CALL_POST() ((void)0)
#endif

#ifndef FRAME_EVAL_SPECIALIZATION_ALLOWED
#define FRAME_EVAL_SPECIALIZATION_ALLOWED_INTERP(interp) \
	((interp)->eval_frame == NULL || (interp)->eval_frame == frame_eval)
#define FRAME_EVAL_SPECIALIZATION_ALLOWED(tstate) \
	FRAME_EVAL_SPECIALIZATION_ALLOWED_INTERP((tstate)->interp)
#endif

#endif
