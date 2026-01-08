/*
===========================================================================
Modern Vertex Buffer System Implementation
===========================================================================
*/

#include "tr_local.h"
#include "gl_vertex.h"
#include "gl_shader.h"

vertexManager_t vertexManager;

static GLenum currentPrimitiveMode;
static vertex_t *currentVertex;
static int vertexCount;

/*
===============
GL_VertexInit
===============
*/
void GL_VertexInit(void) {
    Com_Memset(&vertexManager, 0, sizeof(vertexManager));

    // Create immediate mode emulation buffer
    vertexManager.immediateBuffer.maxVertices = 65536; // Large enough for most immediate mode usage
    vertexManager.immediateBuffer.maxIndices = 65536;
    vertexManager.immediateBuffer.dynamic = qtrue;

    qglGenVertexArrays(1, &vertexManager.immediateBuffer.vao);
    qglGenBuffers(1, &vertexManager.immediateBuffer.vbo);
    qglGenBuffers(1, &vertexManager.immediateBuffer.ibo);

    // Allocate vertex and index arrays
    vertexManager.immediateBuffer.vertices = ri.Malloc(sizeof(vertex_t) * vertexManager.immediateBuffer.maxVertices);
    vertexManager.immediateBuffer.indices = ri.Malloc(sizeof(GLuint) * vertexManager.immediateBuffer.maxIndices);

    // Set default matrices
    Com_Memset(vertexManager.modelViewMatrix, 0, sizeof(vertexManager.modelViewMatrix));
    Com_Memset(vertexManager.projectionMatrix, 0, sizeof(vertexManager.projectionMatrix));
    Com_Memset(vertexManager.modelViewProjectionMatrix, 0, sizeof(vertexManager.modelViewProjectionMatrix));

    vertexManager.modelViewMatrix[0] = vertexManager.modelViewMatrix[5] =
    vertexManager.modelViewMatrix[10] = vertexManager.modelViewMatrix[15] = 1.0f;

    vertexManager.projectionMatrix[0] = vertexManager.projectionMatrix[5] =
    vertexManager.projectionMatrix[10] = vertexManager.projectionMatrix[15] = 1.0f;

    vertexManager.modelViewProjectionMatrix[0] = vertexManager.modelViewProjectionMatrix[5] =
    vertexManager.modelViewProjectionMatrix[10] = vertexManager.modelViewProjectionMatrix[15] = 1.0f;

    ri.Printf(PRINT_ALL, "Modern vertex buffer system initialized\n");
}

/*
===============
GL_VertexShutdown
===============
*/
void GL_VertexShutdown(void) {
    // Clean up immediate buffer
    if (vertexManager.immediateBuffer.vertices) {
        ri.Free(vertexManager.immediateBuffer.vertices);
        vertexManager.immediateBuffer.vertices = nullptr;
    }

    if (vertexManager.immediateBuffer.indices) {
        ri.Free(vertexManager.immediateBuffer.indices);
        vertexManager.immediateBuffer.indices = nullptr;
    }

    if (vertexManager.immediateBuffer.vao) {
        qglDeleteVertexArrays(1, &vertexManager.immediateBuffer.vao);
        vertexManager.immediateBuffer.vao = 0;
    }

    if (vertexManager.immediateBuffer.vbo) {
        qglDeleteBuffers(1, &vertexManager.immediateBuffer.vbo);
        vertexManager.immediateBuffer.vbo = 0;
    }

    if (vertexManager.immediateBuffer.ibo) {
        qglDeleteBuffers(1, &vertexManager.immediateBuffer.ibo);
        vertexManager.immediateBuffer.ibo = 0;
    }

    // Clean up other buffers
    // (In a full implementation, we'd track all allocated buffers)

    Com_Memset(&vertexManager, 0, sizeof(vertexManager));
}

/*
===============
GL_CreateVertexBuffer
===============
*/
vertexBuffer_t *GL_CreateVertexBuffer(int maxVertices, int maxIndices, qboolean dynamic) {
    vertexBuffer_t *buffer = ri.Malloc(sizeof(vertexBuffer_t));
    if (!buffer) {
        ri.Printf(PRINT_ERROR, "GL_CreateVertexBuffer: Failed to allocate vertex buffer structure\n");
        return nullptr;
    }
    Com_Memset(buffer, 0, sizeof(vertexBuffer_t));

    buffer->maxVertices = maxVertices;
    buffer->maxIndices = maxIndices;
    buffer->dynamic = dynamic;

    // Generate OpenGL objects
    qglGenVertexArrays(1, &buffer->vao);
    qglGenBuffers(1, &buffer->vbo);

    if (maxIndices > 0) {
        qglGenBuffers(1, &buffer->ibo);
    }

    // Allocate memory
    buffer->vertices = ri.Malloc(sizeof(vertex_t) * maxVertices);
    if (!buffer->vertices) {
        ri.Printf(PRINT_ERROR, "GL_CreateVertexBuffer: Failed to allocate vertex data (%d vertices)\n", maxVertices);
        // Clean up OpenGL objects
        qglDeleteVertexArrays(1, &buffer->vao);
        qglDeleteBuffers(1, &buffer->vbo);
        if (maxIndices > 0) {
            qglDeleteBuffers(1, &buffer->ibo);
        }
        ri.Free(buffer);
        return nullptr;
    }

    if (maxIndices > 0) {
        buffer->indices = ri.Malloc(sizeof(GLuint) * maxIndices);
        if (!buffer->indices) {
            ri.Printf(PRINT_ERROR, "GL_CreateVertexBuffer: Failed to allocate index data (%d indices)\n", maxIndices);
            // Clean up everything
            ri.Free(buffer->vertices);
            qglDeleteVertexArrays(1, &buffer->vao);
            qglDeleteBuffers(1, &buffer->vbo);
            qglDeleteBuffers(1, &buffer->ibo);
            ri.Free(buffer);
            return nullptr;
        }
    }

    return buffer;
}

/*
===============
GL_DestroyVertexBuffer
===============
*/
void GL_DestroyVertexBuffer(vertexBuffer_t *buffer) {
    if (!buffer) return;

    if (buffer->vertices) {
        ri.Free(buffer->vertices);
    }

    if (buffer->indices) {
        ri.Free(buffer->indices);
    }

    if (buffer->vao) {
        qglDeleteVertexArrays(1, &buffer->vao);
    }

    if (buffer->vbo) {
        qglDeleteBuffers(1, &buffer->vbo);
    }

    if (buffer->ibo) {
        qglDeleteBuffers(1, &buffer->ibo);
    }

    ri.Free(buffer);
}

/*
===============
GL_BindVertexBuffer
===============
*/
void GL_BindVertexBuffer(vertexBuffer_t *buffer) {
    if (!buffer) {
        qglBindVertexArray(0);
        vertexManager.currentBuffer = nullptr;
        return;
    }

    qglBindVertexArray(buffer->vao);
    vertexManager.currentBuffer = buffer;

    // Set up vertex attributes for the default shader
    qglBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);

    if (shaderManager.defaultProgram.a_position >= 0) {
        qglEnableVertexAttribArray(shaderManager.defaultProgram.a_position);
        qglVertexAttribPointer(shaderManager.defaultProgram.a_position, 3, GL_FLOAT, GL_FALSE,
                            sizeof(vertex_t), (void*)offsetof(vertex_t, position));
    }

    if (shaderManager.defaultProgram.a_texCoord >= 0) {
        qglEnableVertexAttribArray(shaderManager.defaultProgram.a_texCoord);
        qglVertexAttribPointer(shaderManager.defaultProgram.a_texCoord, 2, GL_FLOAT, GL_FALSE,
                            sizeof(vertex_t), (void*)offsetof(vertex_t, texCoord));
    }

    if (shaderManager.defaultProgram.a_color >= 0) {
        qglEnableVertexAttribArray(shaderManager.defaultProgram.a_color);
        qglVertexAttribPointer(shaderManager.defaultProgram.a_color, 4, GL_FLOAT, GL_FALSE,
                            sizeof(vertex_t), (void*)offsetof(vertex_t, color));
    }

    if (shaderManager.defaultProgram.a_normal >= 0) {
        qglEnableVertexAttribArray(shaderManager.defaultProgram.a_normal);
        qglVertexAttribPointer(shaderManager.defaultProgram.a_normal, 3, GL_FLOAT, GL_FALSE,
                            sizeof(vertex_t), (void*)offsetof(vertex_t, normal));
    }

    if (buffer->ibo) {
        qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->ibo);
    }
}

/*
===============
GL_UnbindVertexBuffer
===============
*/
void GL_UnbindVertexBuffer(void) {
    qglBindVertexArray(0);
    vertexManager.currentBuffer = NULL;
}

/*
===============
GL_BufferVertexData
===============
*/
void GL_BufferVertexData(vertexBuffer_t *buffer, const vertex_t *vertices, int numVertices) {
    if (!buffer || !vertices || numVertices <= 0) return;

    if (numVertices > buffer->maxVertices) {
        ri.Printf(PRINT_WARNING, "GL_BufferVertexData: too many vertices (%d > %d)\n", numVertices, buffer->maxVertices);
        numVertices = buffer->maxVertices;
    }

    qglBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
    qglBufferData(GL_ARRAY_BUFFER, numVertices * sizeof(vertex_t), vertices,
                buffer->dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    buffer->numVertices = numVertices;
}

/*
===============
GL_BufferIndexData
===============
*/
void GL_BufferIndexData(vertexBuffer_t *buffer, const GLuint *indices, int numIndices) {
    if (!buffer || !indices || numIndices <= 0 || !buffer->ibo) return;

    if (numIndices > buffer->maxIndices) {
        ri.Printf(PRINT_WARNING, "GL_BufferIndexData: too many indices (%d > %d)\n", numIndices, buffer->maxIndices);
        numIndices = buffer->maxIndices;
    }

    qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->ibo);
    qglBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(GLuint), indices, GL_STATIC_DRAW);

    buffer->numIndices = numIndices;
}

/*
===============
GL_DrawVertexBuffer
===============
*/
void GL_DrawVertexBuffer(vertexBuffer_t *buffer, GLenum mode, int first, int count) {
    if (!buffer || count <= 0) return;

    GL_BindVertexBuffer(buffer);

    if (buffer->ibo && buffer->numIndices > 0) {
        qglDrawElements(mode, count, GL_UNSIGNED_INT, (void*)(first * sizeof(GLuint)));
    } else {
        qglDrawArrays(mode, first, count);
    }
}

/*
===============
Immediate Mode Emulation Functions
===============
*/

void GL_Begin(GLenum mode) {
    if (vertexManager.inImmediateMode) {
        ri.Printf(PRINT_WARNING, "GL_Begin called while already in immediate mode\n");
        return;
    }

    vertexManager.inImmediateMode = qtrue;
    currentPrimitiveMode = mode;
    currentVertex = vertexManager.immediateBuffer.vertices;
    vertexCount = 0;

    // Reset vertex data
    Com_Memset(currentVertex, 0, sizeof(vertex_t));
}

void GL_End(void) {
    if (!vertexManager.inImmediateMode) {
        ri.Printf(PRINT_WARNING, "GL_End called while not in immediate mode\n");
        return;
    }

    if (vertexCount > 0) {
        // Update the buffer with our vertex data
        GL_BufferVertexData(&vertexManager.immediateBuffer, vertexManager.immediateBuffer.vertices, vertexCount);

        // Draw the buffered data
        GL_DrawVertexBuffer(&vertexManager.immediateBuffer, currentPrimitiveMode, 0, vertexCount);
    }

    vertexManager.inImmediateMode = qfalse;
}

void GL_Vertex3f(float x, float y, float z) {
    if (!vertexManager.inImmediateMode || vertexCount >= vertexManager.immediateBuffer.maxVertices) {
        return;
    }

    currentVertex->position[0] = x;
    currentVertex->position[1] = y;
    currentVertex->position[2] = z;

    currentVertex++;
    vertexCount++;
}

void GL_TexCoord2f(float s, float t) {
    if (!vertexManager.inImmediateMode || vertexCount == 0) {
        return;
    }

    (currentVertex - 1)->texCoord[0] = s;
    (currentVertex - 1)->texCoord[1] = t;
}

void GL_Color4f(float r, float g, float b, float a) {
    if (!vertexManager.inImmediateMode || vertexCount == 0) {
        return;
    }

    (currentVertex - 1)->color[0] = r;
    (currentVertex - 1)->color[1] = g;
    (currentVertex - 1)->color[2] = b;
    (currentVertex - 1)->color[3] = a;
}

void GL_Normal3f(float nx, float ny, float nz) {
    if (!vertexManager.inImmediateMode || vertexCount == 0) {
        return;
    }

    (currentVertex - 1)->normal[0] = nx;
    (currentVertex - 1)->normal[1] = ny;
    (currentVertex - 1)->normal[2] = nz;
}

/*
===============
Matrix Management
===============
*/

void GL_SetModelViewMatrix(const float *matrix) {
    Com_Memcpy(vertexManager.modelViewMatrix, matrix, sizeof(vertexManager.modelViewMatrix));
    GL_UpdateMatrices();
}

void GL_SetProjectionMatrix(const float *matrix) {
    Com_Memcpy(vertexManager.projectionMatrix, matrix, sizeof(vertexManager.projectionMatrix));
    GL_UpdateMatrices();
}

void GL_UpdateMatrices(void) {
    // Calculate model-view-projection matrix
    // In a full implementation, this would multiply modelViewMatrix * projectionMatrix
    // For now, just copy the projection matrix
    Com_Memcpy(vertexManager.modelViewProjectionMatrix, vertexManager.projectionMatrix,
               sizeof(vertexManager.modelViewProjectionMatrix));

    // Update shader uniforms if we have a current program
    if (shaderManager.defaultProgram.linked) {
        GL_UseShaderProgram(&shaderManager.defaultProgram);
        GL_SetShaderUniformMatrix4(&shaderManager.defaultProgram,
                                 shaderManager.defaultProgram.u_modelViewProjectionMatrix,
                                 vertexManager.modelViewProjectionMatrix);
    }
}