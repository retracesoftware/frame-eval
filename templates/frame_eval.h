#ifndef FRAME_EVAL_H
#define FRAME_EVAL_H

#include "Python.h"

extern PyDictKeysObject *frame_eval_empty_keys;
extern PyObject *frame_eval_monitoring_disable;
extern PyObject *frame_eval_monitoring_missing;
extern PyTypeObject *frame_eval_typealias_type;
extern PyTypeObject *frame_eval_union_type;

int frame_eval_init(void);

#endif