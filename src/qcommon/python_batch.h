#ifndef PYTHON_BATCH_H
#define PYTHON_BATCH_H

#ifdef USE_PYTHON

#include "Python.h"

void PyBatch_Init( void );
void PyBatch_Shutdown( void );

PyObject *PyBatch_Read( PyObject *self, PyObject *args );
PyObject *PyBatch_Write( PyObject *self, PyObject *args );
PyObject *PyBatch_Info( PyObject *self, PyObject *unused );
PyObject *PyBatch_SpawnDemoGrid( PyObject *self, PyObject *args );

#endif /* USE_PYTHON */

#endif /* PYTHON_BATCH_H */
