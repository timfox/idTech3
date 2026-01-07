/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef __QGL_MODERN_H__
#define __QGL_MODERN_H__

// Modern OpenGL 3.3 core profile functions
#define QGL_Core_PROCS_MODERN \
	/* Modern OpenGL 3.3 core functions - only those NOT in system headers */ \
	GLE( void, glActiveTexture, GLenum texture ) \
	GLE( void, glAttachShader, GLuint program, GLuint shader ) \
	GLE( void, glBindBuffer, GLenum target, GLuint buffer ) \
	GLE( void, glBindVertexArray, GLuint array ) \
	GLE( void, glBufferData, GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage ) \
	GLE( void, glBufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data ) \
	GLE( void, glCompileShader, GLuint shader ) \
	GLE( GLuint, glCreateProgram, void ) \
	GLE( GLuint, glCreateShader, GLenum type ) \
	GLE( void, glDeleteBuffers, GLsizei n, const GLuint *buffers ) \
	GLE( void, glDeleteProgram, GLuint program ) \
	GLE( void, glDeleteShader, GLuint shader ) \
	GLE( void, glDeleteVertexArrays, GLsizei n, const GLuint *arrays ) \
	GLE( void, glDisableVertexAttribArray, GLuint index ) \
	GLE( void, glDrawBuffer, GLenum mode ) \
	GLE( void, glEnableVertexAttribArray, GLuint index ) \
	GLE( void, glDisableClientState, GLenum array ) \
	GLE( void, glEnableClientState, GLenum array ) \
	GLE( void, glGenBuffers, GLsizei n, GLuint *buffers ) \
	GLE( void, glGenVertexArrays, GLsizei n, GLuint *arrays ) \
	GLE( void, glGetProgramInfoLog, GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog ) \
	GLE( void, glGetProgramiv, GLuint program, GLenum pname, GLint *params ) \
	GLE( void, glGetShaderInfoLog, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog ) \
	GLE( void, glGetShaderiv, GLuint shader, GLenum pname, GLint *params ) \
	GLE( GLint, glGetUniformLocation, GLuint program, const GLchar *name ) \
	GLE( void, glLinkProgram, GLuint program ) \
	GLE( void, glShaderSource, GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length ) \
	GLE( void, glStencilFunc, GLenum func, GLint ref, GLuint mask ) \
	GLE( void, glStencilOp, GLenum fail, GLenum zfail, GLenum zpass ) \
	GLE( void, glUniform1f, GLint location, GLfloat v0 ) \
	GLE( void, glUniform1i, GLint location, GLint v0 ) \
	GLE( void, glUniform2f, GLint location, GLfloat v0, GLfloat v1 ) \
	GLE( void, glUniform3f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2 ) \
	GLE( void, glUniform4f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3 ) \
	GLE( void, glUniformMatrix3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value ) \
	GLE( void, glUniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value ) \
	GLE( void, glUseProgram, GLuint program ) \
	GLE( void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer ) \
	/* Legacy functions - kept for compatibility but will be handled specially */ \
	GLE( void, glAlphaFunc, GLenum func, GLclampf ref ) \
	GLE( void, glClearDepth, GLclampd depth ) \
	GLE( void, glColor4f, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha ) \
	GLE( void, glColorMask, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha ) \
	GLE( void, glColorPointer, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer ) \
	GLE( void, glDisableClientState, GLenum array ) \
	GLE( void, glEnableClientState, GLenum array ) \
	GLE( void, glLineWidth, GLfloat width ) \
	GLE( void, glLoadIdentity, void ) \
	GLE( void, glLoadMatrixf, const GLfloat *m ) \
	GLE( void, glMatrixMode, GLenum mode ) \
	GLE( void, glMultMatrixf, const GLfloat *m ) \
	GLE( void, glNormal3f, GLfloat nx, GLfloat ny, GLfloat nz ) \
	GLE( void, glNormalPointer, GLenum type, GLsizei stride, const GLvoid *pointer ) \
	GLE( void, glPointSize, GLfloat size ) \
	GLE( void, glPolygonOffset, GLfloat factor, GLfloat units ) \
	GLE( void, glPopMatrix, void ) \
	GLE( void, glPushMatrix, void ) \
	GLE( void, glShadeModel, GLenum mode ) \
	GLE( void, glTexCoord2f, GLfloat s, GLfloat t ) \
	GLE( void, glTexCoord2fv, const GLfloat *v ) \
	GLE( void, glTexCoordPointer, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer ) \
	GLE( void, glTexEnvi, GLenum target, GLenum pname, GLint param ) \
	GLE( void, glVertex3f, GLfloat x, GLfloat y, GLfloat z ) \
	GLE( void, glVertex3fv, const GLfloat *v ) \
	GLE( void, glVertexPointer, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )

#endif // __QGL_MODERN_H__