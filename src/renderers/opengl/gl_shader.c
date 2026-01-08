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
    shaderManager.defaultProgram.program = glCreateProgram();
    shaderManager.defaultProgram.vertexShader = glCreateShader(GL_VERTEX_SHADER);
    shaderManager.defaultProgram.fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (GL_CompileShader(shaderManager.defaultProgram.vertexShader, defaultVertexShader) &&
        GL_CompileShader(shaderManager.defaultProgram.fragmentShader, defaultFragmentShader) &&
        GL_LinkShaderProgram(&shaderManager.defaultProgram)) {

        // Get uniform locations
        shaderManager.defaultProgram.u_modelViewProjectionMatrix = glGetUniformLocation(shaderManager.defaultProgram.program, "u_modelViewProjectionMatrix");
        shaderManager.defaultProgram.u_diffuseMap = glGetUniformLocation(shaderManager.defaultProgram.program, "u_diffuseMap");
        shaderManager.defaultProgram.u_color = glGetUniformLocation(shaderManager.defaultProgram.program, "u_color");
        shaderManager.defaultProgram.u_alphaTest = glGetUniformLocation(shaderManager.defaultProgram.program, "u_alphaTest");

        // Get attribute locations
        shaderManager.defaultProgram.a_position = glGetAttribLocation(shaderManager.defaultProgram.program, "a_position");
        shaderManager.defaultProgram.a_texCoord = glGetAttribLocation(shaderManager.defaultProgram.program, "a_texCoord");
        shaderManager.defaultProgram.a_color = glGetAttribLocation(shaderManager.defaultProgram.program, "a_color");
        shaderManager.defaultProgram.a_normal = glGetAttribLocation(shaderManager.defaultProgram.program, "a_normal");

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
        glDeleteProgram(shaderManager.defaultProgram.program);
        shaderManager.defaultProgram.program = 0;
    }

    if (shaderManager.defaultProgram.vertexShader) {
        glDeleteShader(shaderManager.defaultProgram.vertexShader);
        shaderManager.defaultProgram.vertexShader = 0;
    }

    if (shaderManager.defaultProgram.fragmentShader) {
        glDeleteShader(shaderManager.defaultProgram.fragmentShader);
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
    shaderProgram_t *program = ri.Malloc(sizeof(shaderProgram_t));
    Com_Memset(program, 0, sizeof(shaderProgram_t));

    program->vertexShader = glCreateShader(GL_VERTEX_SHADER);
    program->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    program->program = glCreateProgram();

    if (GL_CompileShader(program->vertexShader, vertexSource) &&
        GL_CompileShader(program->fragmentShader, fragmentSource) &&
        GL_LinkShaderProgram(program)) {

        program->compiled = qtrue;
        program->linked = qtrue;
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
        glDeleteProgram(program->program);
    }

    if (program->vertexShader) {
        glDeleteShader(program->vertexShader);
    }

    if (program->fragmentShader) {
        glDeleteShader(program->fragmentShader);
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

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
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

    glAttachShader(program->program, program->vertexShader);
    glAttachShader(program->program, program->fragmentShader);
    glLinkProgram(program->program);

    glGetProgramiv(program->program, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program->program, sizeof(infoLog), NULL, infoLog);
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
        glUseProgram(program->program);
    } else {
        glUseProgram(0);
    }
}

/*
===============
GL_SetShaderUniformMatrix4
===============
*/
void GL_SetShaderUniformMatrix4(shaderProgram_t *program, GLint location, const float *matrix) {
    if (program && program->linked && location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
    }
}

/*
===============
GL_SetShaderUniform1i
===============
*/
void GL_SetShaderUniform1i(shaderProgram_t *program, GLint location, GLint value) {
    if (program && program->linked && location >= 0) {
        glUniform1i(location, value);
    }
}

/*
===============
GL_SetShaderUniform4f
===============
*/
void GL_SetShaderUniform4f(shaderProgram_t *program, GLint location, float x, float y, float z, float w) {
    if (program && program->linked && location >= 0) {
        glUniform4f(location, x, y, z, w);
    }
}