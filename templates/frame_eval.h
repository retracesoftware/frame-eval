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

#ifndef FRAME_EVAL_SPECIALIZATION_ALLOWED
#define FRAME_EVAL_SPECIALIZATION_ALLOWED_INTERP(interp) \
	((interp)->eval_frame == NULL || (interp)->eval_frame == frame_eval)
#define FRAME_EVAL_SPECIALIZATION_ALLOWED(tstate) \
	FRAME_EVAL_SPECIALIZATION_ALLOWED_INTERP((tstate)->interp)
#endif

#endif
