/*
===========================================================================
OpenGL Shader System Implementation
===========================================================================
*/

#include "tr_local.h"
#include "gl_shader.h"

shaderManager_t shaderManager;

// Basic vertex shader for default rendering
static const char *defaultVertexShader =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec2 a_texCoord;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "layout(location = 3) in vec3 a_normal;\n"
    "\n"
    "uniform mat4 u_modelViewProjectionMatrix;\n"
    "\n"
    "out vec2 v_texCoord;\n"
    "out vec4 v_color;\n"
    "out vec3 v_normal;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = u_modelViewProjectionMatrix * vec4(a_position, 1.0);\n"
    "    v_texCoord = a_texCoord;\n"
    "    v_color = a_color;\n"
    "    v_normal = a_normal;\n"
    "}\n";

// Basic fragment shader for default rendering
static const char *defaultFragmentShader =
    "#version 330 core\n"
    "uniform sampler2D u_diffuseMap;\n"
    "uniform vec4 u_color;\n"
    "uniform float u_alphaTest;\n"
    "\n"
    "in vec2 v_texCoord;\n"
    "in vec4 v_color;\n"
    "in vec3 v_normal;\n"
    "\n"
    "out vec4 fragColor;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 diffuseColor = texture(u_diffuseMap, v_texCoord);\n"
    "    fragColor = diffuseColor * v_color * u_color;\n"
    "\n"
    "    if (u_alphaTest > 0.0 && fragColor.a < u_alphaTest) {\n"
    "        discard;\n"
    "    }\n"
    "}\n";

/*
===============
GL_ShaderInit
===============
*/
void GL_ShaderInit(void) {
    Com_Memset(&shaderManager, 0, sizeof(shaderManager));

    // Create default shader program
    shaderManager.defaultProgram.program = qglCreateProgram();
    shaderManager.defaultProgram.vertexShader = qglCreateShader(GL_VERTEX_SHADER);
    shaderManager.defaultProgram.fragmentShader = qglCreateShader(GL_FRAGMENT_SHADER);

    if (GL_CompileShader(shaderManager.defaultProgram.vertexShader, defaultVertexShader) &&
        GL_CompileShader(shaderManager.defaultProgram.fragmentShader, defaultFragmentShader) &&
        GL_LinkShaderProgram(&shaderManager.defaultProgram)) {

        // Get uniform locations
        shaderManager.defaultProgram.u_modelViewProjectionMatrix = qglGetUniformLocation(shaderManager.defaultProgram.program, "u_modelViewProjectionMatrix");
        shaderManager.defaultProgram.u_diffuseMap = qglGetUniformLocation(shaderManager.defaultProgram.program, "u_diffuseMap");
        shaderManager.defaultProgram.u_color = qglGetUniformLocation(shaderManager.defaultProgram.program, "u_color");
        shaderManager.defaultProgram.u_alphaTest = qglGetUniformLocation(shaderManager.defaultProgram.program, "u_alphaTest");

        // Get attribute locations
        shaderManager.defaultProgram.a_position = qglGetAttribLocation(shaderManager.defaultProgram.program, "a_position");
        shaderManager.defaultProgram.a_texCoord = qglGetAttribLocation(shaderManager.defaultProgram.program, "a_texCoord");
        shaderManager.defaultProgram.a_color = qglGetAttribLocation(shaderManager.defaultProgram.program, "a_color");
        shaderManager.defaultProgram.a_normal = qglGetAttribLocation(shaderManager.defaultProgram.program, "a_normal");
        shaderManager.defaultProgram.a_texCoord = qglGetAttribLocation(shaderManager.defaultProgram.program, "a_texCoord");
        shaderManager.defaultProgram.a_color = qglGetAttribLocation(shaderManager.defaultProgram.program, "a_color");
        shaderManager.defaultProgram.a_normal = qglGetAttribLocation(shaderManager.defaultProgram.program, "a_normal");

        shaderManager.defaultProgram.compiled = qtrue;
        shaderManager.defaultProgram.linked = qtrue;

        ri.Printf(PRINT_ALL, "Default shader program compiled and linked successfully\n");
    } else {
        ri.Printf(PRINT_ERROR, "Failed to create default shader program\n");
    }

    shaderManager.initialized = qtrue;
}

/*
===============
GL_ShaderShutdown
===============
*/
void GL_ShaderShutdown(void) {
    if (shaderManager.defaultProgram.program) {
        qglDeleteProgram(shaderManager.defaultProgram.program);
        shaderManager.defaultProgram.program = 0;
    }

    if (shaderManager.defaultProgram.vertexShader) {
        qglDeleteShader(shaderManager.defaultProgram.vertexShader);
        shaderManager.defaultProgram.vertexShader = 0;
    }

    if (shaderManager.defaultProgram.fragmentShader) {
        qglDeleteShader(shaderManager.defaultProgram.fragmentShader);
        shaderManager.defaultProgram.fragmentShader = 0;
    }

    Com_Memset(&shaderManager, 0, sizeof(shaderManager));
}

/*
===============
GL_CreateShaderProgram
===============
*/
shaderProgram_t *GL_CreateShaderProgram(const char *vertexSource, const char *fragmentSource) {
    // Validate input parameters
    if (!vertexSource || !fragmentSource) {
        ri.Printf(PRINT_ERROR, "GL_CreateShaderProgram: NULL shader source provided\n");
        return NULL;
    }

    shaderProgram_t *program = ri.Malloc(sizeof(shaderProgram_t));
    if (!program) {
        ri.Printf(PRINT_ERROR, "GL_CreateShaderProgram: Failed to allocate shader program structure\n");
        return NULL;
    }
    Com_Memset(program, 0, sizeof(shaderProgram_t));

    program->vertexShader = qglCreateShader(GL_VERTEX_SHADER);
    program->fragmentShader = qglCreateShader(GL_FRAGMENT_SHADER);
    program->program = qglCreateProgram();

    if (GL_CompileShader(program->vertexShader, vertexSource) &&
        GL_CompileShader(program->fragmentShader, fragmentSource) &&
        GL_LinkShaderProgram(program)) {

        program->compiled = qtrue;
        program->linked = qtrue;
    } else {
        // Compilation/linking failed - clean up OpenGL objects
        if (program->program) {
            qglDeleteProgram(program->program);
        }
        if (program->vertexShader) {
            qglDeleteShader(program->vertexShader);
        }
        if (program->fragmentShader) {
            qglDeleteShader(program->fragmentShader);
        }
        ri.Free(program);
        return NULL;
    }

    return program;
}

/*
===============
GL_DestroyShaderProgram
===============
*/
void GL_DestroyShaderProgram(shaderProgram_t *program) {
    if (!program) return;

    if (program->program) {
        qglDeleteProgram(program->program);
    }

    if (program->vertexShader) {
        qglDeleteShader(program->vertexShader);
    }

    if (program->fragmentShader) {
        qglDeleteShader(program->fragmentShader);
    }

    ri.Free(program);
}

/*
===============
GL_CompileShader
===============
*/
qboolean GL_CompileShader(GLuint shader, const char *source) {
    GLint compiled;

    qglShaderSource(shader, 1, &source, NULL);
    qglCompileShader(shader);

    qglGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLchar infoLog[1024];
        qglGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
        ri.Printf(PRINT_ERROR, "Shader compilation failed: %s\n", infoLog);
        return qfalse;
    }

    return qtrue;
}

/*
===============
GL_LinkShaderProgram
===============
*/
qboolean GL_LinkShaderProgram(shaderProgram_t *program) {
    GLint linked;

    qglAttachShader(program->program, program->vertexShader);
    qglAttachShader(program->program, program->fragmentShader);
    qglLinkProgram(program->program);

    qglGetProgramiv(program->program, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLchar infoLog[1024];
        qglGetProgramInfoLog(program->program, sizeof(infoLog), NULL, infoLog);
        ri.Printf(PRINT_ERROR, "Shader program linking failed: %s\n", infoLog);
        return qfalse;
    }

    return qtrue;
}

/*
===============
GL_UseShaderProgram
===============
*/
void GL_UseShaderProgram(shaderProgram_t *program) {
    if (program && program->linked) {
        qglUseProgram(program->program);
    } else {
        qglUseProgram(0);
    }
}

/*
===============
GL_SetShaderUniformMatrix4
===============
*/
void GL_SetShaderUniformMatrix4(shaderProgram_t *program, GLint location, const float *matrix) {
    if (program && program->linked && location >= 0) {
        qglUniformMatrix4fv(location, 1, GL_FALSE, matrix);
    }
}

/*
===============
GL_SetShaderUniform1i
===============
*/
void GL_SetShaderUniform1i(shaderProgram_t *program, GLint location, GLint value) {
    if (program && program->linked && location >= 0) {
        qglUniform1i(location, value);
    }
}

/*
===============
GL_SetShaderUniform4f
===============
*/
void GL_SetShaderUniform4f(shaderProgram_t *program, GLint location, float x, float y, float z, float w) {
    if (program && program->linked && location >= 0) {
        qglUniform4f(location, x, y, z, w);
    }
}