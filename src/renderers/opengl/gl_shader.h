/*
===========================================================================
OpenGL Shader System - Modern GLSL Implementation

Replaces the fixed-function pipeline with programmable shaders.
===========================================================================
*/

#ifndef __GL_SHADER_H__
#define __GL_SHADER_H__

#include "tr_local.h"

// Shader types
typedef enum {
    SHADER_VERTEX,
    SHADER_FRAGMENT,
    SHADER_GEOMETRY,
    SHADER_TESS_CONTROL,
    SHADER_TESS_EVALUATION
} shaderType_t;

// Shader program structure
typedef struct shaderProgram_s {
    GLuint program;
    GLuint vertexShader;
    GLuint fragmentShader;

    // Uniform locations
    GLint u_modelViewMatrix;
    GLint u_projectionMatrix;
    GLint u_modelViewProjectionMatrix;
    GLint u_normalMatrix;

    GLint u_diffuseMap;
    GLint u_lightMap;
    GLint u_normalMap;

    GLint u_color;
    GLint u_alphaTest;

    // Vertex attribute locations
    GLint a_position;
    GLint a_texCoord;
    GLint a_color;
    GLint a_normal;
    GLint a_tangent;

    qboolean compiled;
    qboolean linked;
} shaderProgram_t;

// Shader manager
typedef struct {
    shaderProgram_t defaultProgram;
    shaderProgram_t lightmappedProgram;
    shaderProgram_t vertexLitProgram;

    qboolean initialized;
} shaderManager_t;

extern shaderManager_t shaderManager;

// Function declarations
void GL_ShaderInit(void);
void GL_ShaderShutdown(void);

shaderProgram_t *GL_CreateShaderProgram(const char *vertexSource, const char *fragmentSource);
void GL_DestroyShaderProgram(shaderProgram_t *program);

qboolean GL_CompileShader(GLuint shader, const char *source);
qboolean GL_LinkShaderProgram(shaderProgram_t *program);

void GL_UseShaderProgram(shaderProgram_t *program);
void GL_SetShaderUniformMatrix4(shaderProgram_t *program, GLint location, const float *matrix);
void GL_SetShaderUniform1i(shaderProgram_t *program, GLint location, GLint value);
void GL_SetShaderUniform4f(shaderProgram_t *program, GLint location, float x, float y, float z, float w);

#endif // __GL_SHADER_H__