// PS5 OpenGL - OpenGL implementation for PlayStation 5.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PS5_NATIVE_BOUNDED_TRIANGLE
#include "native_diagnostics.h"
#endif

extern int sceKernelUsleep(uint32_t microseconds);

#ifdef PS5_OPENGL_STANDALONE_LOG
__attribute__((constructor)) static void ps5_opengl_open_log(void) {
#else
void ps5_opengl_open_log(void) {
#endif
#ifdef PS5_NATIVE_BOUNDED_TRIANGLE
  /* Retain inherited console descriptors; do not depend on writable title storage. */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
#else
  FILE *stream = freopen("/download0/ps5-opengl.log", "w", stdout);
  /* Start a fresh receipt, then make both independent streams append-only. */
  if (stream != NULL)
    stream = freopen("/download0/ps5-opengl.log", "a", stdout);
  if (stream != NULL)
    setvbuf(stream, NULL, _IONBF, 0);
  stream = freopen("/download0/ps5-opengl.log", "a", stderr);
  if (stream != NULL)
    setvbuf(stream, NULL, _IONBF, 0);
#endif
}

__attribute__((noreturn)) void catchReturnFromMain(int status) {
#ifdef PS5_NATIVE_BOUNDED_TRIANGLE
  ps5_native_trace("[ps5-opengl-native] gate completed status=%d\n", status);
#else
  printf("[ps5-opengl-native] gate completed status=%d\n", status);
#endif
  fflush(NULL);
#ifdef PS5_NATIVE_BOUNDED_TRIANGLE
  extern int sceSystemServiceLoadExec(const char *, const char *const *);
  ps5_native_trace("OGL2_EXIT_REQUEST_BEGIN");
  int exit_result = sceSystemServiceLoadExec("exit", NULL);
  ps5_native_trace("OGL2_FAIL operation=exit-request result=0x%x status=%d\n", exit_result, status);
  fflush(NULL);
#endif
  for (;;)
    sceKernelUsleep(100000);
}

void ps5_opengl_glapi_tls_context_init(void) __asm__(
    "_ZTH23_mesa_glapi_tls_Context");

void ps5_opengl_glapi_tls_context_init(void) {}

/* DSO handle pointer for dynamic shared object destructor registration (__cxa_atexit) */
__attribute__((weak)) void *__dso_handle = 0;

/* Weak POSIX stubs so libc.a implementations override cleanly if pulled */
__attribute__((weak, noreturn)) void __assert(const char *function, const char *file,
                                        int line, const char *expression) {
  fprintf(stderr, "assertion failed: %s (%s:%d, %s)\n", expression, file, line,
          function);
  abort();
}

__attribute__((weak)) int mkstemps(char *template_name, int suffix_length) {
  (void)template_name;
  (void)suffix_length;
  errno = ENOSYS;
  return -1;
}

__attribute__((weak)) int mkstemp(char *template_name) {
  return mkstemps(template_name, 0);
}

__attribute__((weak)) void openlog(const char *identifier, int option, int facility) {
  (void)identifier;
  (void)option;
  (void)facility;
}

__attribute__((weak)) FILE *popen(const char *command, const char *mode) {
  (void)command;
  (void)mode;
  errno = ENOSYS;
  return NULL;
}

__attribute__((weak)) int pclose(FILE *stream) {
  (void)stream;
  errno = ENOSYS;
  return -1;
}

/* Fallback unwinder stubs for C++ ABI personality references without libunwind.a duplicate symbols */
struct _Unwind_Exception;
struct _Unwind_Context;

__attribute__((weak)) void _Unwind_DeleteException(struct _Unwind_Exception *exc) {
  (void)exc;
}

__attribute__((weak)) int _Unwind_RaiseException(struct _Unwind_Exception *exc) {
  (void)exc;
  abort();
}

__attribute__((weak)) uintptr_t _Unwind_GetLanguageSpecificData(struct _Unwind_Context *ctx) {
  (void)ctx;
  return 0;
}

__attribute__((weak)) uintptr_t _Unwind_GetRegionStart(struct _Unwind_Context *ctx) {
  (void)ctx;
  return 0;
}

__attribute__((weak)) uintptr_t _Unwind_GetIP(struct _Unwind_Context *ctx) {
  (void)ctx;
  return 0;
}

__attribute__((weak)) void _Unwind_SetIP(struct _Unwind_Context *ctx, uintptr_t val) {
  (void)ctx;
  (void)val;
}

__attribute__((weak)) void _Unwind_SetGR(struct _Unwind_Context *ctx, int idx, uintptr_t val) {
  (void)ctx;
  (void)idx;
  (void)val;
}

/* Missing POSIX locale stubs */
__attribute__((weak)) double strtod_l(const char *nptr, char **endptr, void *loc) {
  (void)loc;
  return strtod(nptr, endptr);
}

__attribute__((weak)) float strtof_l(const char *nptr, char **endptr, void *loc) {
  (void)loc;
  return strtof(nptr, endptr);
}

__attribute__((weak)) void *newlocale(int category_mask, const char *locale, void *base) {
  (void)category_mask;
  (void)locale;
  (void)base;
  return (void *)1;
}

__attribute__((weak)) void freelocale(void *locobj) {
  (void)locobj;
}

/* Missing POSIX reentrant quicksort, bridging to libc qsort_s */
extern int qsort_s(void *base, size_t nmemb, size_t size,
                   int (*compar)(const void *, const void *, void *),
                   void *context);

struct ps5_qsort_r_data {
  void *thunk;
  int (*compar)(void *, const void *, const void *);
};

static int ps5_qsort_adapt_cmp(const void *a, const void *b, void *context) {
  struct ps5_qsort_r_data *d = (struct ps5_qsort_r_data *)context;
  return d->compar(d->thunk, a, b);
}

__attribute__((weak)) void qsort_r(void *base, size_t nmemb, size_t size,
                                   void *thunk,
                                   int (*compar)(void *, const void *, const void *)) {
  struct ps5_qsort_r_data d = { thunk, compar };
  qsort_s(base, nmemb, size, ps5_qsort_adapt_cmp, &d);
}



