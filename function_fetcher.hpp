#pragma once

#include <dlfcn.h>
#include <assert.h>

/* functions implemented in GNU libc that we can use to get
 * symbols without directly invoking dlopen/dlsym.
 */
extern "C" void *__libc_dlopen_mode(const char *filename, int flag);
extern "C" void *__libc_dlsym(void *handle, const char *symbol);

/* calls the "real" dlsym() by calling the __libc_ functions
 * to get the dlsym symbol in libdl.so.
 */
static
void*
real_dlsym(void *handle, const char *symbol)
{
   typedef void* (*fptr_type)(void*, const char*);
   static fptr_type fptr  = nullptr;
   if (!fptr)
     {
       void *libdl;
       libdl =  __libc_dlopen_mode("libdl.so", RTLD_LOCAL | RTLD_NOW);
       if (libdl)
         {
           fptr = (fptr_type)__libc_dlsym(libdl, "dlsym");
         }

       if (!fptr)
         {
           assert(!"Warning: unable to get dlsym unnaturally");
           return nullptr;
         }
     }
   return fptr(handle, symbol);
}

namespace
{
  class FunctionFetcher
  {
  public:
    void*
    dlsym_lib_style(const char *symbol)
    {
      void *return_value;
      if (!m_handle)
        {
          return_value = real_dlsym(RTLD_NEXT, symbol);
          if (return_value)
            {
              m_handle = RTLD_NEXT;
              return return_value;
            }

          m_handle = __libc_dlopen_mode(m_name, RTLD_GLOBAL | RTLD_LAZY | RTLD_DEEPBIND);
          if (!m_handle)
            {
              m_handle = __libc_dlopen_mode(m_fallback, RTLD_GLOBAL | RTLD_LAZY | RTLD_DEEPBIND);
            }
        }

      return real_dlsym(m_handle, symbol);
    }

    static
    FunctionFetcher&
    gl(void)
    {
      static FunctionFetcher r("I965_BLACKBOX_GL_LIB", "libGL.so");
      return r;
    }

    static
    FunctionFetcher&
    gles(void)
    {
      static FunctionFetcher r("I965_BLACKBOX_GLES_LIB", "libGLESv2.so");
      return r;
    }

    static
    FunctionFetcher&
    egl(void)
    {
      static FunctionFetcher r("I965_BLACKBOX_EGL_LIB", "libEGL.so");
      return r;
    }
  
  private:
    FunctionFetcher(const char *environ_var,
                    const char *fallback_lib):
      m_handle(nullptr),
      m_fallback(fallback_lib)
    {
      m_name = std::getenv(environ_var);
      if (!m_name)
        {
          m_name = m_fallback;
        }
    }

    void *m_handle;
    const char *m_name;
    const char *m_fallback;
  };
}

static
void*
gl_dlsym(const char *symbol)
{
  typedef void* (*fptr_type)(const char*);
  static fptr_type fptr1 = nullptr;
  static fptr_type fptr2 = nullptr;
  void *return_value = nullptr;

  if (!fptr1)
    {
      fptr1 = (fptr_type)FunctionFetcher::gl().dlsym_lib_style("glXGetProcAddress");
    }

  if (!fptr2)
    {
      fptr2 = (fptr_type)FunctionFetcher::gl().dlsym_lib_style("glXGetProcAddressARB");
    }

  if (fptr1)
    {
      return_value = fptr1(symbol);
      if (return_value)
        {
          return return_value;
        }
    }

  if (fptr2)
    {
      return_value = fptr2(symbol);
      if (return_value)
        {
          return return_value;
        }
    }

  return FunctionFetcher::gl().dlsym_lib_style(symbol);
}

static
void*
gles_dlsym(const char *symbol)
{
  typedef void* (*fptr_type)(const char*);
  static fptr_type fptr = nullptr;
  
  void *return_value = nullptr;

  if (!fptr)
    {
      fptr = (fptr_type)FunctionFetcher::egl().dlsym_lib_style("eglGetProcAddress");
    }

  if (fptr)
    {
      return_value = fptr(symbol);
    }

  if (!return_value)
    {
      return_value = FunctionFetcher::gles().dlsym_lib_style(symbol);
    }

  return return_value;
}

static
void*
egl_dlsym(const char *symbol)
{
  typedef void* (*fptr_type)(const char*);
  static fptr_type fptr = nullptr;
  void *return_value = nullptr;

  if (!fptr)
    {
      fptr = (fptr_type)FunctionFetcher::egl().dlsym_lib_style("eglGetProcAddress");
    }

  if (fptr)
    {
      return_value = fptr(symbol);
    }

  if (!return_value)
    {
      return_value = FunctionFetcher::egl().dlsym_lib_style(symbol);
    }

  return return_value;
}
