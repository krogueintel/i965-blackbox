#pragma once

// GL typedef's
typedef void GLvoid;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef float GLclampf;
typedef double GLclampd;
typedef ptrdiff_t GLsizeiptr;
typedef GLsizeiptr GLsizeiptrARB;
typedef ptrdiff_t GLintptr;
typedef GLintptr GLintptrARB;
typedef char GLchar;
typedef GLchar GLcharARB;
typedef short GLshort;
typedef signed char GLbyte;
typedef unsigned short GLushort;
typedef unsigned short GLhalf;
typedef struct __GLsync *GLsync;
typedef uint64_t GLuint64;
typedef int64_t GLint64;
typedef int64_t GLint64EXT;
typedef uint64_t GLuint64EXT;
typedef int32_t GLfixed;
typedef unsigned long GLhandleARB;
typedef uint16_t GLhalfNV;
typedef void (*GLDEBUGPROC)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void (*GLDEBUGPROCAMD)(GLuint id,GLenum category,GLenum severity,GLsizei length,const GLchar *message,void *userParam);
typedef void (*GLDEBUGPROCARB)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void (*GLDEBUGPROCKHR)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void* GLeglImageOES;
typedef int GLclampx;
typedef GLintptr GLvdpauSurfaceNV;
typedef void (*GLVULKANPROCNV)(void);
typedef void *GLeglClientBufferEXT;

// types needed for X
typedef unsigned long XID;
typedef XID GLXDrawable;
