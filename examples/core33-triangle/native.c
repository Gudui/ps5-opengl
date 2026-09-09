// PS5 OpenGL - OpenGL implementation for PlayStation 5.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded native-folder variant of main.c; public EGL/GL only.
#include <stdio.h>
#include <stdint.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include "native_identity.h"
#include "native_diagnostics.h"

extern uint64_t sceKernelGetProcessTime(void);

static GLuint compile_shader(GLenum type, const char *source, const char *stage)
{
   GLuint shader = glCreateShader(type);
   GLint compiled = GL_FALSE;
   if (!shader) return 0;
   glShaderSource(shader, 1, &source, NULL);
   glCompileShader(shader);
   glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
   if (!compiled) {
      char log[512];
      GLsizei length = 0;
      glGetShaderInfoLog(shader, sizeof(log), &length, log);
      pss_native_trace("OGL2_FAIL stage=%s log=%.*s\n", stage, length, log);
      glDeleteShader(shader);
      return 0;
   }
   pss_native_trace("OGL2_SHADER_COMPILE_OK stage=%s\n", stage);
   return shader;
}

int main(void)
{
   static const char *vs = "#version 330 core\nlayout(location=0) in vec2 position;\n"
      "void main(){gl_Position=vec4(position,0.0,1.0);}\n";
   static const char *fs = "#version 330 core\nlayout(location=0) out vec4 color;\n"
      "void main(){color=vec4(1.0,0.0,1.0,1.0);}\n";
#ifdef PS5_NATIVE_INDEXED_TRIANGLE
   /* First three vertices are degenerate: ignoring the EBO cannot pass visually. */
   static const GLfloat vertices[] = {-0.5f,-0.5f, 0.5f,-0.5f, -0.5f,-0.5f, 0.0f,0.5f};
   static const GLushort indices[] = {0, 1, 3};
   GLuint ebo = 0;
#else
   static const GLfloat vertices[] = {-0.5f,-0.5f, 0.5f,-0.5f, 0.0f,0.5f};
#endif
   static const EGLint configs[] = {EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
      EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
   static const EGLint contexts[] = {EGL_CONTEXT_MAJOR_VERSION_KHR,3,
      EGL_CONTEXT_MINOR_VERSION_KHR,3,EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
      EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,EGL_NONE};
   EGLDisplay display = EGL_NO_DISPLAY;
   EGLSurface surface = EGL_NO_SURFACE;
   EGLContext context = EGL_NO_CONTEXT;
   EGLConfig config = NULL;
   EGLint major = 0, minor = 0, count = 0, width = 0, height = 0;
   GLuint vertex = 0, fragment = 0, program = 0, vao = 0, vbo = 0;
   GLint linked = 0, gl_major = 0, gl_minor = 0, profile = 0;
   int initialized = 0, current = 0, result = 1, cleanup_ok = 1;
   const char *operation = "get-display";
   uint64_t start = 0;
   unsigned frames = 0;
#define CHECK(expr, name) do { operation = name; if (!(expr)) goto cleanup; } while (0)
   pss_native_trace("OGL2_MAIN_ENTER title=%s build=%s start_us=%llu\n", PS5_NATIVE_TITLE_ID,
          PS5_NATIVE_BUILD_ID, (unsigned long long)sceKernelGetProcessTime());
   display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   CHECK(display != EGL_NO_DISPLAY, "get-display");
   CHECK(eglInitialize(display, &major, &minor), "initialize");
   initialized = 1;
   pss_native_trace("OGL2_EGL_INITIALIZE_OK");
   CHECK(eglBindAPI(EGL_OPENGL_API), "bind-api");
   CHECK(eglChooseConfig(display, configs, &config, 1, &count) && count == 1, "config");
   surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)0, NULL);
   CHECK(surface != EGL_NO_SURFACE, "surface");
   context = eglCreateContext(display, config, EGL_NO_CONTEXT, contexts);
   CHECK(context != EGL_NO_CONTEXT, "context");
   CHECK(eglMakeCurrent(display, surface, surface, context), "make-current");
   current = 1;
   glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
   glGetIntegerv(GL_MINOR_VERSION, &gl_minor);
   glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
   CHECK(gl_major == 3 && gl_minor == 3 && (profile & GL_CONTEXT_CORE_PROFILE_BIT)
         && glGetError() == GL_NO_ERROR, "context-profile");
   pss_native_trace("OGL2_CONTEXT_CURRENT major=%d minor=%d profile=core\n", gl_major, gl_minor);
   CHECK(eglQuerySurface(display, surface, EGL_WIDTH, &width) &&
         eglQuerySurface(display, surface, EGL_HEIGHT, &height) && width > 0 && height > 0,
         "surface-size");
   vertex = compile_shader(GL_VERTEX_SHADER, vs, "vertex");
   CHECK(vertex, "vertex-compile");
   fragment = compile_shader(GL_FRAGMENT_SHADER, fs, "fragment");
   CHECK(fragment, "fragment-compile");
   program = glCreateProgram();
   CHECK(program, "create-program");
   glAttachShader(program, vertex);
   glAttachShader(program, fragment);
   glLinkProgram(program);
   glGetProgramiv(program, GL_LINK_STATUS, &linked);
   CHECK(linked, "program-link");
   pss_native_trace("OGL2_PROGRAM_LINK_OK");
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
   glEnableVertexAttribArray(0);
   glUseProgram(program);
   glViewport(0, 0, width, height);
   glClearColor(0, 0, 0, 1);
   CHECK(vao && vbo && glGetError() == GL_NO_ERROR, "vertex-setup");
#ifdef PS5_NATIVE_INDEXED_TRIANGLE
   glGenBuffers(1, &ebo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
   CHECK(ebo && glGetError() == GL_NO_ERROR, "index-setup");
   pss_native_trace("OGL3_INDEXED_SETUP_OK type=ushort count=3 indices=0,1,3 offset=0");
#endif
   start = sceKernelGetProcessTime();
   for (frames = 0; frames < 600; ++frames) {
      CHECK(sceKernelGetProcessTime() - start < UINT64_C(30000000), "frame-deadline");
      glClear(GL_COLOR_BUFFER_BIT);
#ifdef PS5_NATIVE_INDEXED_TRIANGLE
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL);
#else
      glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
      glFinish();
      CHECK(glGetError() == GL_NO_ERROR, "draw-finish");
      CHECK(eglSwapBuffers(display, surface), "swap");
      if (frames == 0 || (frames + 1) % 60 == 0)
         pss_native_trace("OGL2_FRAME_COMPLETE frame=%u\n", frames + 1);
   }
   pss_native_trace("OGL2_RUN_COMPLETE frames=%u elapsed_ms=%llu reason=frame-limit\n", frames,
          (unsigned long long)((sceKernelGetProcessTime() - start) / 1000));
   result = 0;
cleanup:
   if (result) pss_native_trace("OGL2_FAIL operation=%s egl=0x%x frames=%u\n", operation,
                      eglGetError(), frames);
   if (current) {
#ifdef PS5_NATIVE_INDEXED_TRIANGLE
      if (ebo) glDeleteBuffers(1, &ebo);
#endif
      if (vbo) glDeleteBuffers(1, &vbo);
      if (vao) glDeleteVertexArrays(1, &vao);
      if (program) glDeleteProgram(program);
      if (fragment) glDeleteShader(fragment);
      if (vertex) glDeleteShader(vertex);
      if (glGetError() != GL_NO_ERROR) cleanup_ok = 0;
      if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) cleanup_ok = 0;
   }
   if (context != EGL_NO_CONTEXT && !eglDestroyContext(display, context)) cleanup_ok = 0;
   if (surface != EGL_NO_SURFACE && !eglDestroySurface(display, surface)) cleanup_ok = 0;
   if (initialized && !eglTerminate(display)) cleanup_ok = 0;
   if (!cleanup_ok) { pss_native_trace("OGL2_FAIL operation=teardown"); result = 1; }
   else if (!result) pss_native_trace("OGL2_EGL_TEARDOWN_OK");
   return result;
}
