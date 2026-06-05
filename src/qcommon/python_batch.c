/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Infernux-style batch data bridge (SoA entity columns, arXiv:2604.10263 §VI-A).
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "python_batch.h"

#ifdef USE_PYTHON

#define PYBATCH_MAX_ENTITIES 16384u
#define PYBATCH_FIELD_POS     0
#define PYBATCH_FIELD_VEL     1

typedef struct {
	uint32_t generation;
	float position[3];
	float velocity[3];
	qboolean alive;
} pyBatchEntity_t;

static pyBatchEntity_t pyBatchEntities[PYBATCH_MAX_ENTITIES];
static uint32_t pyBatchAliveCount;

void PyBatch_Init( void ) {
	Com_Memset( pyBatchEntities, 0, sizeof( pyBatchEntities ) );
	pyBatchAliveCount = 0;
}

void PyBatch_Shutdown( void ) {
	PyBatch_Init();
}

static int PyBatch_FieldIndex( const char *field ) {
	if ( !field ) {
		return -1;
	}
	if ( !Q_stricmp( field, "position" ) || !Q_stricmp( field, "pos" ) ) {
		return PYBATCH_FIELD_POS;
	}
	if ( !Q_stricmp( field, "velocity" ) || !Q_stricmp( field, "vel" ) ) {
		return PYBATCH_FIELD_VEL;
	}
	return -1;
}

static int PyBatch_ComponentsForField( int field ) {
	return ( field == PYBATCH_FIELD_POS || field == PYBATCH_FIELD_VEL ) ? 3 : 0;
}

PyObject *PyBatch_Info( PyObject *self, PyObject *unused )
{
	PyObject *dict;

	(void)self;
	(void)unused;
	dict = PyDict_New();
	if ( !dict ) {
		return NULL;
	}
	PyDict_SetItemString( dict, "max_entities", PyLong_FromUnsignedLong( PYBATCH_MAX_ENTITIES ) );
	PyDict_SetItemString( dict, "alive", PyLong_FromUnsignedLong( pyBatchAliveCount ) );
	PyDict_SetItemString( dict, "fields", PyUnicode_FromString( "position, velocity" ) );
	return dict;
}

PyObject *PyBatch_SpawnDemoGrid( PyObject *self, PyObject *args )
{
	int gridSide;
	int i;
	int j;
	uint32_t idx;
	float spacing;

	(void)self;
	if ( !PyArg_ParseTuple( args, "i|f", &gridSide, &spacing ) ) {
		return NULL;
	}
	if ( gridSide < 1 ) {
		gridSide = 1;
	}
	if ( gridSide > 128 ) {
		gridSide = 128;
	}
	if ( spacing <= 0.0f ) {
		spacing = 2.0f;
	}

	PyBatch_Init();
	idx = 0;
	for ( i = 0; i < gridSide && idx < PYBATCH_MAX_ENTITIES; i++ ) {
		for ( j = 0; j < gridSide && idx < PYBATCH_MAX_ENTITIES; j++ ) {
			pyBatchEntities[idx].generation = 1;
			pyBatchEntities[idx].alive = qtrue;
			pyBatchEntities[idx].position[0] = (float)( i - gridSide / 2 ) * spacing;
			pyBatchEntities[idx].position[1] = 0.0f;
			pyBatchEntities[idx].position[2] = (float)( j - gridSide / 2 ) * spacing;
			pyBatchEntities[idx].velocity[0] = 0.0f;
			pyBatchEntities[idx].velocity[1] = 0.0f;
			pyBatchEntities[idx].velocity[2] = 0.0f;
			idx++;
		}
	}
	pyBatchAliveCount = idx;
	return PyLong_FromUnsignedLong( idx );
}

PyObject *PyBatch_Read( PyObject *self, PyObject *args )
{
	PyObject *handleList;
	PyObject *fieldObj;
	const char *fieldName;
	int field;
	int components;
	Py_ssize_t count;
	Py_ssize_t i;
	PyObject *out;

	(void)self;
	if ( !PyArg_ParseTuple( args, "OO", &handleList, &fieldObj ) ) {
		return NULL;
	}
	if ( !PyList_Check( handleList ) ) {
		PyErr_SetString( PyExc_TypeError, "batch_read expects a list of integer handles" );
		return NULL;
	}
	fieldName = PyUnicode_AsUTF8( fieldObj );
	field = PyBatch_FieldIndex( fieldName );
	components = PyBatch_ComponentsForField( field );
	if ( components <= 0 ) {
		PyErr_SetString( PyExc_ValueError, "unsupported batch field (use position or velocity)" );
		return NULL;
	}

	count = PyList_GET_SIZE( handleList );
	out = PyList_New( count * components );
	if ( !out ) {
		return NULL;
	}

	for ( i = 0; i < count; i++ ) {
		PyObject *item = PyList_GET_ITEM( handleList, i );
		long handle = PyLong_AsLong( item );
		uint32_t slot;
		const float *src;

		if ( PyErr_Occurred() ) {
			Py_DECREF( out );
			return NULL;
		}
		if ( handle < 0 || (uint32_t)handle >= PYBATCH_MAX_ENTITIES ) {
			Py_DECREF( out );
			PyErr_SetString( PyExc_IndexError, "invalid entity handle in batch_read" );
			return NULL;
		}
		slot = (uint32_t)handle;
		if ( !pyBatchEntities[slot].alive ) {
			Py_DECREF( out );
			PyErr_SetString( PyExc_IndexError, "dead entity handle in batch_read" );
			return NULL;
		}
		src = ( field == PYBATCH_FIELD_POS ) ? pyBatchEntities[slot].position : pyBatchEntities[slot].velocity;
		PyList_SET_ITEM( out, i * components + 0, PyFloat_FromDouble( src[0] ) );
		PyList_SET_ITEM( out, i * components + 1, PyFloat_FromDouble( src[1] ) );
		PyList_SET_ITEM( out, i * components + 2, PyFloat_FromDouble( src[2] ) );
	}

	return out;
}

PyObject *PyBatch_Write( PyObject *self, PyObject *args )
{
	PyObject *handleList;
	PyObject *fieldObj;
	PyObject *values;
	const char *fieldName;
	int field;
	int components;
	Py_ssize_t count;
	Py_ssize_t i;

	(void)self;
	if ( !PyArg_ParseTuple( args, "OOO", &handleList, &fieldObj, &values ) ) {
		return NULL;
	}
	if ( !PyList_Check( handleList ) || !PyList_Check( values ) ) {
		PyErr_SetString( PyExc_TypeError, "batch_write expects list handles and flat float values" );
		return NULL;
	}
	fieldName = PyUnicode_AsUTF8( fieldObj );
	field = PyBatch_FieldIndex( fieldName );
	components = PyBatch_ComponentsForField( field );
	if ( components <= 0 ) {
		PyErr_SetString( PyExc_ValueError, "unsupported batch field (use position or velocity)" );
		return NULL;
	}

	count = PyList_GET_SIZE( handleList );
	if ( PyList_GET_SIZE( values ) != count * components ) {
		PyErr_SetString( PyExc_ValueError, "batch_write value length must be N * components" );
		return NULL;
	}

	for ( i = 0; i < count; i++ ) {
		long handle = PyLong_AsLong( PyList_GET_ITEM( handleList, i ) );
		float *dst;
		Py_ssize_t base = i * components;

		if ( handle < 0 || (uint32_t)handle >= PYBATCH_MAX_ENTITIES || !pyBatchEntities[handle].alive ) {
			PyErr_SetString( PyExc_IndexError, "invalid or dead entity handle in batch_write" );
			return NULL;
		}
		dst = ( field == PYBATCH_FIELD_POS ) ? pyBatchEntities[handle].position : pyBatchEntities[handle].velocity;
		dst[0] = (float)PyFloat_AsDouble( PyList_GET_ITEM( values, base + 0 ) );
		dst[1] = (float)PyFloat_AsDouble( PyList_GET_ITEM( values, base + 1 ) );
		dst[2] = (float)PyFloat_AsDouble( PyList_GET_ITEM( values, base + 2 ) );
	}

	Py_RETURN_NONE;
}

#endif /* USE_PYTHON */
