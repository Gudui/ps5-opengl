// PS5 OpenGL - OpenGL implementation for PlayStation 5.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PS5_NATIVE_DIAGNOSTICS_H
#define PS5_NATIVE_DIAGNOSTICS_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern int sceKernelDebugOutText(int device, const char *text);

/* Diagnostics never retry, allocate, or decide application success/exit. */
static inline int pss_native_trace(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

static inline int pss_native_trace(const char *format, ...) {
  char text[1024];
  va_list args;
  va_start(args, format);
  /* Reserve a byte beyond the formatted NUL for a missing final newline. */
  int length = vsnprintf(text, sizeof(text) - 1, format, args);
  va_end(args);
  if (length < 0) {
    memcpy(text, "[pss-opengl-native] diagnostic-format-error\n",
           sizeof("[pss-opengl-native] diagnostic-format-error\n"));
  } else if ((size_t)length >= sizeof(text) - 1) {
    static const char suffix[] = " [truncated]\n";
    memcpy(text + sizeof(text) - sizeof(suffix), suffix, sizeof(suffix));
  } else if (length == 0 || text[length - 1] != '\n') {
    text[length] = '\n';
    text[length + 1] = '\0';
  }
  return sceKernelDebugOutText(0, text);
}

#endif
