#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include "i965_batchbuffer_logger_app.h"
#include "i965_batchbuffer_logger_output.h"

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

/* Interception Notes:
 *
 * 1. For apps linked against libGL: enough to define for each GL,
 *    GLX and EGL function, define extern "C" that calls the real
 *    function via function pointer fetched in (3).
 *
 * 2. for apps that use glXGetProcAddress(), glXGetProcAddressARB()
 *    and eglGetProcAddress(), define those symbols to return our
 *    functions of (1).
 *
 * 3. Fetching the real functions:
 *     - use "real" dlopen/dlsym to grab eglGetProcAddress(), glXGetProcAddressARB()
 *       and glXGetProcAddress(); we get the the real dlopen/dlsym by calling
 *       h = __libc_dlopen_mode("libdl.so.2", RTLD_LOCAL | RTLD_NOW) to get a handle
 *       to the libdl library and then call real_dlsym = __libc_dlsym(h, "dlsym")
 *       to get the real dlsym symbol and use that.
 *
 *     - intercept dlopen() to see if the application opens libGL, libGLESv2 or
 *       libEGL dynamically. If so, use that handle with real_dlsym() to get
 *       the function pointer eglGetProcAddress(),  glXGetProcAddressARB() and
 *       glXGetProcAddress(); if the application asks for the symbol without
 *       using dlopen for the function, then try RTLD_NEXT.
 *
 * 4. When an app uses dlsym() to get eglGetProcAddress(), glXGetProcAddressARB()
 *    or glXGetProcAddress() we can have it return our special implementation that
 *    returns our wrapped functions (that special implementation is machine generated
 *    with a long string of if/else if with strcmp() to give the correct function.
 *    We will set the function pointer to the real function pointer on calling
 *    the proc getter.
 *
 * 5. If a GL function is called without getting fetched, we will call the last
 *    getter used by the application (one of eglGetProcAddress, glXGetProcAddress
 *    or glXGetProcAddressARB, dlsym). If no getter was used, then we use dlsym
 *    with RTLD_NEXT to get the symbol.
 */

// file size of 16MB
#define FILE_SIZE (16 * 1024 * 1024)

// so many frames per dumping
#define NUM_FRAMES 100

class Block {
public:
   void
   set(const void *name, uint32_t name_length,
       const void *value, uint32_t value_length)
   {
      m_name.resize(name_length);
      if (name_length > 0) {
         std::memcpy(&m_name[0], name, name_length);
      }

      m_value.resize(value_length);
      if (value_length > 0) {
         std::memcpy(&m_value[0], value, value_length);
      }
   }

   const void*
   name(void) const
   {
      return m_name.empty() ?
         nullptr :
         &m_name[0];
   }

   uint32_t
   name_length(void) const
   {
      return m_name.size();
   }

   const void*
   value(void) const
   {
      return m_value.empty() ?
         nullptr :
         &m_value[0];
   }

   uint32_t
   value_length(void) const
   {
      return m_value.size();
   }

private:
   std::vector<uint8_t> m_name;
   std::vector<uint8_t> m_value;
};

class Session {
public:
   explicit
   Session();

   ~Session();

   static
   void
   write_fcn(void *pthis,
             enum i965_batchbuffer_logger_message_type_t tp,
             const void *name, uint32_t name_length,
             const void *value, uint32_t value_length);

   static
   void
   close_fcn(void *pthis);
private:
   void
   start_new_file(void);

   void
   close_file(void);

   void
   write_to_file(enum i965_batchbuffer_logger_message_type_t tp,
                 const void *name, uint32_t name_length,
                 const void *value, uint32_t value_length);

   unsigned int m_count;
   /* Because we split a single session across many files,
    * we need to reset the block to zero on ending a file
    * and restore the block structure at the start of a
    * new file; m_block_stack holds the block structure.
    */
   std::vector<Block> m_block_stack;
   std::string m_prefix;
   std::FILE *m_file;
};

//////////////////////////////////////////
// Session methods
Session::
Session(void):
   m_count(0),
   m_file(nullptr)
{
   static unsigned int count(0);
   const char *filename_prefix;
   std::ostringstream str;

   filename_prefix = getenv("BATCHBUFFER_LOG_PREFIX");
   if (filename_prefix == NULL) {
      filename_prefix = "batchbuffer_log";
   }

   str << filename_prefix << "-" << ++count;
   m_prefix = str.str();

   start_new_file();
}

Session::
~Session()
{
   close_file();
}

void
Session::
close_file(void)
{
   if (m_file) {
      for (auto iter = m_block_stack.rbegin();
           iter != m_block_stack.rend(); ++iter) {
         write_to_file(I965_BATCHBUFFER_LOGGER_MESSAGE_BLOCK_END,
                       nullptr, 0, nullptr, 0);
      }
      std::fclose(m_file);
      m_file = nullptr;
   }
}

void
Session::
start_new_file(void)
{
   close_file();

   std::ostringstream str;
   str << m_prefix << "." << ++m_count;
   m_file = std::fopen(str.str().c_str(), "w");
   for (auto iter = m_block_stack.begin();
           iter != m_block_stack.end(); ++iter) {
         write_to_file(I965_BATCHBUFFER_LOGGER_MESSAGE_BLOCK_BEGIN,
                       iter->name(), iter->name_length(),
                       iter->value(), iter->value_length());
      }
}

void
Session::
write_to_file(enum i965_batchbuffer_logger_message_type_t tp,
              const void *name, uint32_t name_length,
              const void *value, uint32_t value_length)
{
   if (!m_file) {
      return;
   }

   struct i965_batchbuffer_logger_header hdr;
   hdr.type = tp;
   hdr.name_length = name_length;
   hdr.value_length = value_length;
   std::fwrite(&hdr, sizeof(hdr), 1, m_file);
   if (name_length > 0) {
      std::fwrite(name, sizeof(char), name_length, m_file);
   }
   if (value_length > 0) {
      std::fwrite(value, sizeof(char), value_length, m_file);
   }
}

void
Session::
close_fcn(void *pthis)
{
   Session *p;
   p = static_cast<Session*>(pthis);
   delete p;
}

static
bool
is_begin_execbuffer2(const void *name, uint32_t name_length)
{
   static const char *src = "drmIoctl(execbuffer2)";
   static const uint32_t src_length = std::strlen(src);

   const char *n;
   n = static_cast<const char*>(name);

   if (name_length != src_length) {
      return false;
   }

   for(int i = 0, endi = std::strlen(src); i < endi; ++i) {
      if (n[i] != src[i]) {
         return false;
      }
   }

   return true;
}

void
Session::
write_fcn(void *pthis,
          enum i965_batchbuffer_logger_message_type_t tp,
          const void *name, uint32_t name_length,
          const void *value, uint32_t value_length)
{
   Session *p;
   p = static_cast<Session*>(pthis);

   if (tp == I965_BATCHBUFFER_LOGGER_MESSAGE_BLOCK_BEGIN
       && is_begin_execbuffer2(name, name_length)
       && std::ftell(p->m_file) > FILE_SIZE) {
      p->start_new_file();
   }

   switch (tp) {
   case I965_BATCHBUFFER_LOGGER_MESSAGE_BLOCK_BEGIN:
      p->m_block_stack.push_back(Block());
      p->m_block_stack.back().set(name, name_length, value, value_length);
      break;

   case I965_BATCHBUFFER_LOGGER_MESSAGE_BLOCK_END:
      p->m_block_stack.pop_back();
      break;
   }
   p->write_to_file(tp, name, name_length, value, value_length);
}

/////////////////////////////////////////////
// and so it begins.
static struct i965_batchbuffer_logger_app *logger_app = NULL;
static struct i965_batchbuffer_logger_session logger_session = { .opaque = NULL };
static unsigned int frame_count = 0;

extern "C" struct i965_batchbuffer_logger_app*
i965_batchbuffer_logger_app_acquire(void);

typedef unsigned long XID;
typedef XID GLXDrawable;


static void *libGLhandle = nullptr;
static void *libEGLhandle = nullptr;

extern "C" void *__libc_dlopen_mode(const char *filename, int flag);
extern "C" void *__libc_dlsym(void *handle, const char *symbol);

static
void*
real_dlsym(void *handle, const char *symbol)
{
   typedef void* (*fptr_type)(void*, const char*);
   static fptr_type fptr  = nullptr;
   if (!fptr) {
      void *libdl;
      libdl =  __libc_dlopen_mode("libdl.so", RTLD_LOCAL | RTLD_NOW);
      if (libdl) {
         fptr = (fptr_type)__libc_dlsym(libdl, "dlsym");
      }

      if (!fptr) {
         std::fprintf(stderr, "Warning: unable to get dlsym unnaturally");
         return nullptr;
      }
   }

   return fptr(handle, symbol);
}

static
void*
gl_dlsym_lib_style(const char *symbol)
{
   void *return_value;

   if (!libGLhandle) {
      return_value = real_dlsym(RTLD_NEXT, symbol);
      if (symbol) {
         return return_value;
      }
      libGLhandle = __libc_dlopen_mode("libGL.so", RTLD_GLOBAL | RTLD_LAZY | RTLD_DEEPBIND);
   }

   return real_dlsym(libGLhandle, symbol);
}

static
void*
gl_dlsym(const char *symbol)
{
  typedef void* (*fptr_type)(const char*);
  static fptr_type fptr = nullptr;
  void *return_value = nullptr;

  if (!fptr) {
    fptr = (fptr_type)gl_dlsym_lib_style("glXGetProcAddress");
  }

  if (!fptr) {
    fptr = (fptr_type)gl_dlsym_lib_style("glXGetProcAddressARB");
  }

  if (fptr) {
    return_value = fptr(symbol);
  }

  if (!return_value) {
    return_value = gl_dlsym_lib_style(symbol);
  }

  return return_value;
}

static
void*
egl_dlsym_lib_style(const char *symbol)
{
   void *return_value;

   if (!libEGLhandle) {
      return_value = real_dlsym(RTLD_NEXT, symbol);
      if (symbol) {
         return return_value;
      }
      libEGLhandle = __libc_dlopen_mode("libEGL.so", RTLD_GLOBAL | RTLD_LAZY | RTLD_DEEPBIND);
   }

   return real_dlsym(libGLhandle, symbol);
}

static
void*
egl_dlsym(const char *symbol)
{
  typedef void* (*fptr_type)(const char*);
  static fptr_type fptr = nullptr;
  void *return_value = nullptr;

  if (!fptr) {
    fptr = (fptr_type)egl_dlsym_lib_style("eglGetProcAddress");
  }

  if (fptr) {
    return_value = fptr(symbol);
  }

  if (!return_value) {
    return_value = gl_dlsym_lib_style(symbol);
  }

  return return_value;
}


#define FUNCTION_ENTRY(name, type_arg_list, arg_list)              \
  extern "C" void name type_arg_list                               \
  {                                                                \
    typedef void (*fptr_type) type_arg_list;                       \
    static fptr_type fptr = nullptr;                               \
    if (fptr == nullptr) {                                         \
      fptr = (fptr_type)gl_dlsym(#name);                           \
    }                                                              \
    if (logger_app) {                                              \
      logger_app->pre_call(logger_app, frame_count, #name, #name); \
    }                                                              \
    fptr arg_list;                                                 \
    if (logger_app) {                                              \
      logger_app->post_call(logger_app, frame_count);              \
    }                                                              \
    ++frame_count;                                                 \
  }

#define FUNCTION_ENTRY_RET(type, name, type_arg_list, arg_list)    \
  extern "C" type name type_arg_list                               \
  {                                                                \
    typedef type (*fptr_type) type_arg_list;                       \
    static fptr_type fptr = nullptr;                               \
    type return_value;                                             \
    if (fptr == nullptr) {                                         \
      fptr = (fptr_type)gl_dlsym(#name);                           \
    }                                                              \
    if (logger_app) {                                              \
      logger_app->pre_call(logger_app, frame_count, #name, #name); \
    }                                                              \
    return_value = fptr arg_list;                                  \
    if (logger_app) {                                              \
      logger_app->post_call(logger_app, frame_count);              \
    }                                                              \
    ++frame_count;                                                 \
    return return_value;                                           \
  }

#include "build/function_macros.inc"
  

#undef FUNCTION_ENTRY
#undef FUNCTION_ENTRY_RET

struct function_list
{
  const char *m_name;
  void *m_function;
};


struct function_list every_function[] = {
#define FUNCTION_ENTRY(name, type_arg_list, arg_list) { #name, (void*)name },
#define FUNCTION_ENTRY_RET(type, name, type_arg_list, arg_list) { #name, (void*)name },
#include "build/function_macros.inc"
};

static
void*
gl_function(const char *name)
{
  const unsigned int sz = sizeof(every_function) / sizeof(every_function[0]);
  for (unsigned int i = 0; i < sz; ++i) {
    if (std::strcmp(name, every_function[i].m_name) == 0) {
      return every_function[i].m_function;
    }
  }
  return nullptr;
}

extern "C"
void
glXSwapBuffers(void *dpy, GLXDrawable drawable)
{
   typedef void (*fptr_type)(void*, GLXDrawable);
   static fptr_type fptr = nullptr;
   if (fptr == nullptr) {
      fptr = (fptr_type)gl_dlsym("glXSwapBuffers");
   }

   if (logger_app) {
      logger_app->pre_call(logger_app, frame_count, "glXSwapBuffers", "glXSwapBuffers");
   }

   fptr(dpy, drawable);

   if (logger_app) {
      logger_app->post_call(logger_app, frame_count);
      if (frame_count > NUM_FRAMES) {
         struct i965_batchbuffer_logger_session_params params;

         logger_app->end_session(logger_app, logger_session);
         params.client_data = new Session();
         params.write = &Session::write_fcn;
         params.close = &Session::close_fcn;
         logger_session = logger_app->begin_session(logger_app, &params);
      }
   }

   ++frame_count;
}

extern "C"
unsigned int
eglSwapBuffers(void *dpy, void *surface)
{
   typedef unsigned int (*fptr_type)(void*, void*);
   static fptr_type fptr = nullptr;
   unsigned int R;

   if (fptr == nullptr) {
      fptr = (fptr_type)egl_dlsym("eglSwapBuffers");
   }

   if (logger_app) {
      logger_app->pre_call(logger_app, frame_count, "eglSwapBuffers", "eglSwapBuffers");
   }

   R = fptr(dpy, surface);

   if (logger_app) {
      logger_app->post_call(logger_app, frame_count);
      if (frame_count > NUM_FRAMES) {
         struct i965_batchbuffer_logger_session_params params;

         logger_app->end_session(logger_app, logger_session);
         params.client_data = new Session();
         params.write = &Session::write_fcn;
         params.close = &Session::close_fcn;
         logger_session = logger_app->begin_session(logger_app, &params);
      }
   }

   ++frame_count;
   return R;
}


extern "C"
void*
glXGetProcAddressARB(const char *name)
{
   typedef void* (*fptr_type)(const char*);
   static fptr_type fptr = nullptr;
   if (!fptr) {
      fptr = (fptr_type)gl_dlsym_lib_style("glXGetProcAddressARB");
   }

   if (std::strcmp(name, "glXSwapBuffers") == 0) {
      return (void*)glXSwapBuffers;
   }

   void *q;
   q = gl_function(name);
   if (q)
     {
       return q;
     }

   return fptr(name);
}

extern "C"
void*
glXGetProcAddress(const char *name)
{
   typedef void* (*fptr_type)(const char*);
   static fptr_type fptr = nullptr;
   if (!fptr) {
      fptr = (fptr_type)gl_dlsym_lib_style("glXGetProcAddress");
   }

   if (std::strcmp(name, "glXSwapBuffers") == 0) {
      return (void*)glXSwapBuffers;
   }

   void *q;
   q = gl_function(name);
   if (q)
     {
       return q;
     }

   return fptr(name);
}

extern "C"
void*
eglGetProcAddress(const char *name)
{
   typedef void* (*fptr_type)(const char*);
   static fptr_type fptr = nullptr;
   if (!fptr) {
      fptr = (fptr_type)egl_dlsym_lib_style("eglGetProcAddress");
   }

   if (std::strcmp(name, "eglSwapBuffers") == 0) {
      return (void*)eglSwapBuffers;
   }

   void *q;
   q = gl_function(name);
   if (q)
     {
       return q;
     }

   return fptr(name);
}

/* TODO:
 *   1. for each GL function, define two variants:
 *      those that use gl_dlsym() to get the real
 *      function and those that use egl_dlsym().
 *      The former are returned by glXGetProcAddress
 *      or dlsym when passed libGLhandle, the latter
 *      for those returned by eglGetProcAddress and
 *      or dlsym when passed libEGLhandle.
 */
extern "C"
void*
dlsym(void *handle, const char *symbol)
{
   if (std::strcmp(symbol, "glXGetProcAddress") == 0 ||
       std::strcmp(symbol, "glXGetProcAddressARB") == 0) {
      libGLhandle = handle;
      return (void*)glXGetProcAddressARB;
   }

   if (std::strcmp(symbol, "eglGetProcAddress") == 0) {
      libEGLhandle = handle;
      return (void*)eglGetProcAddress;
   }

   return real_dlsym(handle, symbol);
}

__attribute__((constructor))
static
void
start_session(void)
{
   const char *filename_prefix;

   filename_prefix = getenv("BATCHBUFFER_LOG_PREFIX");
   if (filename_prefix == NULL) {
      filename_prefix = "batchbuffer_log";
   }

   struct i965_batchbuffer_logger_session_params params;
   params.client_data = new Session();
   params.write = &Session::write_fcn;
   params.close = &Session::close_fcn;

   logger_app = i965_batchbuffer_logger_app_acquire();
   logger_session = logger_app->begin_session(logger_app, &params);
}

__attribute__((destructor))
static
void
end_session(void)
{
   if (logger_app) {
      logger_app->end_session(logger_app, logger_session);
      logger_app->release_app(logger_app);
      logger_app = nullptr;
   }
}
