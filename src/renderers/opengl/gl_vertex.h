/*
===========================================================================
Modern Vertex Buffer System

Replaces immediate mode rendering with VBOs and VAOs.
===========================================================================
*/

#ifndef __GL_VERTEX_H__
#define __GL_VERTEX_H__

#include "tr_local.h"

// Modern vertex format
typedef struct {
    vec3_t position;
    vec2_t texCoord;
    vec4_t color;
    vec3_t normal;
    vec3_t tangent;
} vertex_t;

// Vertex buffer structure
typedef struct vertexBuffer_s {
    GLuint vao;
    GLuint vbo;
    GLuint ibo;

    vertex_t *vertices;
    GLuint *indices;

    int numVertices;
    int numIndices;
    int maxVertices;
    int maxIndices;

    qboolean dynamic;
    qboolean mapped;
} vertexBuffer_t;

// Vertex buffer manager
typedef struct {
    vertexBuffer_t *currentBuffer;
    int numBuffers;
    int maxBuffers;

    // Immediate mode emulation buffers
    vertexBuffer_t immediateBuffer;
    qboolean inImmediateMode;

    // Cached matrices for shader uniforms
    float modelViewMatrix[16];
    float projectionMatrix[16];
    float modelViewProjectionMatrix[16];
} vertexManager_t;

extern vertexManager_t vertexManager;

// Function declarations
void GL_VertexInit(void);
void GL_VertexShutdown(void);

vertexBuffer_t *GL_CreateVertexBuffer(int maxVertices, int maxIndices, qboolean dynamic);
void GL_DestroyVertexBuffer(vertexBuffer_t *buffer);

void GL_BindVertexBuffer(vertexBuffer_t *buffer);
void GL_UnbindVertexBuffer(void);

void GL_BufferVertexData(vertexBuffer_t *buffer, const vertex_t *vertices, int numVertices);
void GL_BufferIndexData(vertexBuffer_t *buffer, const GLuint *indices, int numIndices);

void GL_DrawVertexBuffer(vertexBuffer_t *buffer, GLenum mode, int first, int count);

// Immediate mode emulation functions (for compatibility)
void GL_Begin(GLenum mode);
void GL_End(void);
void GL_Vertex3f(float x, float y, float z);
void GL_TexCoord2f(float s, float t);
void GL_Color4f(float r, float g, float b, float a);
void GL_Normal3f(float nx, float ny, float nz);

// Matrix management
void GL_SetModelViewMatrix(const float *matrix);
void GL_SetProjectionMatrix(const float *matrix);
void GL_UpdateMatrices(void);

#endif // __GL_VERTEX_H__