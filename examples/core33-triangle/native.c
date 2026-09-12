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
      ps5_native_trace("OGL2_FAIL stage=%s log=%.*s\n", stage, length, log);
      glDeleteShader(shader);
      return 0;
   }
   ps5_native_trace("OGL2_SHADER_COMPILE_OK stage=%s\n", stage);
   return shader;
}

int main(void)
{
#if defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE)
   static const char *vs = "#version 330 core\n"
      "layout(location=0) in vec2 position;\n"
      "layout(location=1) in vec2 texcoord;\n"
      "out vec2 v_texcoord;\n"
      "void main(){\n"
      "   v_texcoord=texcoord;\n"
      "   gl_Position=vec4(position,0.0,1.0);\n"
      "}\n";
   static const char *fs = "#version 330 core\n"
      "in vec2 v_texcoord;\n"
      "layout(location=0) out vec4 color;\n"
      "uniform sampler2D u_texture;\n"
      "void main(){\n"
      "   color=texture(u_texture,v_texcoord);\n"
      "}\n";
#elif defined(PS5_NATIVE_ALPHA_BLEND)
   static const char *vs = "#version 330 core\nlayout(location=0) in vec2 position;\n"
      "void main(){gl_Position=vec4(position,0.0,1.0);}\n";
   static const char *fs = "#version 330 core\nlayout(location=0) out vec4 color;\n"
      "void main(){color=vec4(1.0,0.0,0.0,0.5);}\n";
#elif defined(PS5_NATIVE_UNIFORM_MATRIX)
   static const char *vs = "#version 330 core\nlayout(location=0) in vec2 position;\n"
      "uniform mat4 u_transform;\n"
      "void main(){gl_Position=u_transform*vec4(position,0.0,1.0);}\n";
   static const char *fs = "#version 330 core\nlayout(location=0) out vec4 color;\n"
      "void main(){color=vec4(1.0,0.0,1.0,1.0);}\n";
#else
   static const char *vs = "#version 330 core\nlayout(location=0) in vec2 position;\n"
      "void main(){gl_Position=vec4(position,0.0,1.0);}\n";
   static const char *fs = "#version 330 core\nlayout(location=0) out vec4 color;\n"
      "void main(){color=vec4(1.0,0.0,1.0,1.0);}\n";
#endif
#if defined(PS5_NATIVE_ALPHA_BLEND)
   /* Standard centered triangle [-0.5, 0.5] with indices {0, 1, 3} matching control.
    * Fragment shader outputs semi-transparent red (1.0, 0.0, 0.0, 0.5).
    * Clear color is solid blue (0.0, 0.0, 1.0, 1.0).
    * Blending is configured with GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD:
    * Result = 0.5 * Red(1,0,0) + 0.5 * Blue(0,0,1) = Purple(0.5, 0.0, 0.5).
    * Single-variable visual oracle:
    * - Blending active: purple / violet centered triangle on solid blue background.
    * - Blending disabled/ignored: solid red triangle on blue background.
    * - Shader / clear failure: black or missing triangle. */
   static const GLfloat vertices[] = {
      -0.5f, -0.5f,
       0.5f, -0.5f,
      -0.5f, -0.5f,
       0.0f,  0.5f
   };
   static const GLushort indices[] = {0, 1, 3};
   GLuint ebo = 0;
#elif defined(PS5_NATIVE_SAMPLER_STATE)
   /* Interleaved {pos.x, pos.y, uv.u, uv.v} with extended UV coordinates [0.0, 2.0].
    * Indices {0, 1, 3} form the centered triangle [-0.5, 0.5] matching control.
    * Texels are 2x2 RGBA8:
    * (0,0)=Red (255,0,0,255), (1,0)=Green (0,255,0,255),
    * (0,1)=Blue (0,0,255,255), (1,1)=Yellow (255,255,0,255).
    * Texture object retains GL_NEAREST and GL_CLAMP_TO_EDGE (control baseline).
    * Sampler object overrides with GL_LINEAR and GL_REPEAT.
    * Single-variable visual oracle:
    * - Sampler object active: repeating 2x2 pattern with smooth linear color blending
    *   (repeating red/green pattern along base, repeating vertically toward apex).
    * - Sampler object ignored (fallback to texture clamp/nearest): clamped edges
    *   beyond UV=1.0 and blocky nearest pixels.
    * - Sampler binding failure / shader failure: black / invisible.
    * - Control fallback: solid purple (refuted). */
   static const GLfloat vertices[] = {
      -0.5f, -0.5f,  0.0f, 0.0f,
       0.5f, -0.5f,  2.0f, 0.0f,
      -0.5f, -0.5f,  0.0f, 0.0f,
       0.0f,  0.5f,  1.0f, 2.0f
   };
   static const GLushort indices[] = {0, 1, 3};
   static const uint8_t texels[4][4] = {
      { 255,   0,   0, 255 }, /* (0,0): Red */
      {   0, 255,   0, 255 }, /* (1,0): Green */
      {   0,   0, 255, 255 }, /* (0,1): Blue */
      { 255, 255,   0, 255 }  /* (1,1): Yellow */
   };
   GLuint ebo = 0;
   GLuint texture = 0;
   GLuint sampler = 0;
   GLint u_tex_loc = -1;
#elif defined(PS5_NATIVE_TEXTURE_2D)
   /* Interleaved {pos.x, pos.y, uv.u, uv.v}.
    * Indices {0, 1, 3} form the centered triangle [-0.5, 0.5] matching control.
    * Texels are 2x2 RGBA8:
    * (0,0)=Red (255,0,0,255), (1,0)=Green (0,255,0,255),
    * (0,1)=Blue (0,0,255,255), (1,1)=Yellow (255,255,0,255).
    * Single-variable visual oracle:
    * - Texture sampling / UV interpolation works: multi-colored textured triangle
    *   (bottom-left Red, bottom-right Green, apex Blue/Yellow).
    * - Texture missing / uninitialized sampler: black / invisible.
    * - UV stream failure (constant zero): solid Red.
    * - Control fallback: solid purple (refuted). */
   static const GLfloat vertices[] = {
      -0.5f, -0.5f,  0.0f, 0.0f,
       0.5f, -0.5f,  1.0f, 0.0f,
      -0.5f, -0.5f,  0.0f, 0.0f,
       0.0f,  0.5f,  0.5f, 1.0f
   };
   static const GLushort indices[] = {0, 1, 3};
   static const uint8_t texels[4][4] = {
      { 255,   0,   0, 255 }, /* (0,0): Red */
      {   0, 255,   0, 255 }, /* (1,0): Green */
      {   0,   0, 255, 255 }, /* (0,1): Blue */
      { 255, 255,   0, 255 }  /* (1,1): Yellow */
   };
   GLuint ebo = 0;
   GLuint texture = 0;
   GLint u_tex_loc = -1;
#elif defined(PS5_NATIVE_UNIFORM_MATRIX)
   /* Base vertices are [-1, 1]; uniform 0.5 scale matrix maps to [-0.5, 0.5].
    * If uniform is unassigned (0.0 in GLSL), vertex positions become (0,0,0,0) (invisible).
    * If uniform transform is not applied, vertices span [-1, 1] (edge-to-edge).
    * With uniform matrix applied, rendered triangle matches control centered geometry. */
   static const GLfloat vertices[] = {-1.0f,-1.0f, 1.0f,-1.0f, -1.0f,-1.0f, 0.0f,1.0f};
   static const GLushort indices[] = {0, 1, 3};
   static const GLfloat transform_matrix[16] = {
      0.5f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.5f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
   };
   GLuint ebo = 0;
   GLint u_loc = -1;
#elif defined(PS5_NATIVE_INDEXED_TRIANGLE)
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
   ps5_native_trace("OGL2_MAIN_ENTER title=%s build=%s start_us=%llu\n", PS5_NATIVE_TITLE_ID,
          PS5_NATIVE_BUILD_ID, (unsigned long long)sceKernelGetProcessTime());
   display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   CHECK(display != EGL_NO_DISPLAY, "get-display");
   CHECK(eglInitialize(display, &major, &minor), "initialize");
   initialized = 1;
   ps5_native_trace("OGL2_EGL_INITIALIZE_OK");
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
   ps5_native_trace("OGL2_CONTEXT_CURRENT major=%d minor=%d profile=core\n", gl_major, gl_minor);
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
   ps5_native_trace("OGL2_PROGRAM_LINK_OK");
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
#if defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE)
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)0);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
   glEnableVertexAttribArray(1);
#else
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
   glEnableVertexAttribArray(0);
#endif
   glUseProgram(program);
   glViewport(0, 0, width, height);
#ifdef PS5_NATIVE_ALPHA_BLEND
   glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
#else
   glClearColor(0, 0, 0, 1);
#endif
   CHECK(vao && vbo && glGetError() == GL_NO_ERROR, "vertex-setup");
#if defined(PS5_NATIVE_INDEXED_TRIANGLE) || defined(PS5_NATIVE_UNIFORM_MATRIX) || defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_ALPHA_BLEND)
   glGenBuffers(1, &ebo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
   CHECK(ebo && glGetError() == GL_NO_ERROR, "index-setup");
   ps5_native_trace("OGL3_INDEXED_SETUP_OK type=ushort count=3 indices=0,1,3 offset=0");
#endif
#ifdef PS5_NATIVE_UNIFORM_MATRIX
   u_loc = glGetUniformLocation(program, "u_transform");
   CHECK(u_loc >= 0 && glGetError() == GL_NO_ERROR, "uniform-location");
   glUniformMatrix4fv(u_loc, 1, GL_FALSE, transform_matrix);
   CHECK(glGetError() == GL_NO_ERROR, "uniform-matrix");
   ps5_native_trace("OGL3_UNIFORM_MATRIX_SETUP_OK loc=%d\n", u_loc);
#endif
#if defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE)
   glGenTextures(1, &texture);
   CHECK(texture && glGetError() == GL_NO_ERROR, "texture-gen");
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
   CHECK(glGetError() == GL_NO_ERROR, "texture-upload");
   u_tex_loc = glGetUniformLocation(program, "u_texture");
   CHECK(u_tex_loc >= 0 && glGetError() == GL_NO_ERROR, "texture-location");
   glUniform1i(u_tex_loc, 0);
   CHECK(glGetError() == GL_NO_ERROR, "texture-uniform");
   ps5_native_trace("OGL3_TEXTURE_2D_SETUP_OK tex=%u loc=%d\n", texture, u_tex_loc);
#endif
#ifdef PS5_NATIVE_SAMPLER_STATE
   glGenSamplers(1, &sampler);
   CHECK(sampler && glGetError() == GL_NO_ERROR, "sampler-gen");
   glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
   CHECK(glGetError() == GL_NO_ERROR, "sampler-param");
   glBindSampler(0, sampler);
   CHECK(glGetError() == GL_NO_ERROR, "sampler-bind");
   {
      GLint q_min = 0, q_mag = 0, q_wrap_s = 0, q_wrap_t = 0;
      glGetSamplerParameteriv(sampler, GL_TEXTURE_MIN_FILTER, &q_min);
      glGetSamplerParameteriv(sampler, GL_TEXTURE_MAG_FILTER, &q_mag);
      glGetSamplerParameteriv(sampler, GL_TEXTURE_WRAP_S, &q_wrap_s);
      glGetSamplerParameteriv(sampler, GL_TEXTURE_WRAP_T, &q_wrap_t);
      CHECK(q_min == GL_LINEAR && q_mag == GL_LINEAR &&
            q_wrap_s == GL_REPEAT && q_wrap_t == GL_REPEAT &&
            glGetError() == GL_NO_ERROR, "sampler-query");
      ps5_native_trace("OGL3_SAMPLER_SETUP_OK sampler=%u min=0x%x mag=0x%x wrap_s=0x%x wrap_t=0x%x\n",
                       sampler, q_min, q_mag, q_wrap_s, q_wrap_t);
   }
#endif
#ifdef PS5_NATIVE_ALPHA_BLEND
   glEnable(GL_BLEND);
   CHECK(glIsEnabled(GL_BLEND), "blend-enable");
   glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
   CHECK(glGetError() == GL_NO_ERROR, "blend-setup");
   {
      GLint src_rgb = 0, dst_rgb = 0, src_a = 0, dst_a = 0, eq_rgb = 0, eq_a = 0;
      glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
      glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
      glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_a);
      glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_a);
      glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq_rgb);
      glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eq_a);
      CHECK(src_rgb == GL_SRC_ALPHA && dst_rgb == GL_ONE_MINUS_SRC_ALPHA &&
            src_a == GL_ONE && dst_a == GL_ONE_MINUS_SRC_ALPHA &&
            eq_rgb == GL_FUNC_ADD && eq_a == GL_FUNC_ADD &&
            glGetError() == GL_NO_ERROR, "blend-query");
      ps5_native_trace("OGL3_ALPHA_BLEND_SETUP_OK src_rgb=0x%x dst_rgb=0x%x src_a=0x%x dst_a=0x%x eq_rgb=0x%x eq_a=0x%x\n",
                       src_rgb, dst_rgb, src_a, dst_a, eq_rgb, eq_a);
   }
#endif
   start = sceKernelGetProcessTime();
   for (frames = 0; frames < 600; ++frames) {
      CHECK(sceKernelGetProcessTime() - start < UINT64_C(30000000), "frame-deadline");
      glClear(GL_COLOR_BUFFER_BIT);
#ifdef PS5_NATIVE_UNIFORM_MATRIX
      glUniformMatrix4fv(u_loc, 1, GL_FALSE, transform_matrix);
#endif
#if defined(PS5_NATIVE_INDEXED_TRIANGLE) || defined(PS5_NATIVE_UNIFORM_MATRIX) || defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_ALPHA_BLEND)
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL);
#else
      glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
      glFinish();
      CHECK(glGetError() == GL_NO_ERROR, "draw-finish");
      CHECK(eglSwapBuffers(display, surface), "swap");
      if (frames == 0 || (frames + 1) % 60 == 0)
         ps5_native_trace("OGL2_FRAME_COMPLETE frame=%u\n", frames + 1);
   }
   ps5_native_trace("OGL2_RUN_COMPLETE frames=%u elapsed_ms=%llu reason=frame-limit\n", frames,
          (unsigned long long)((sceKernelGetProcessTime() - start) / 1000));
   result = 0;
cleanup:
   if (result) ps5_native_trace("OGL2_FAIL operation=%s egl=0x%x frames=%u\n", operation,
                      eglGetError(), frames);
   if (current) {
#ifdef PS5_NATIVE_ALPHA_BLEND
      glDisable(GL_BLEND);
      if (glIsEnabled(GL_BLEND)) cleanup_ok = 0;
#endif
#ifdef PS5_NATIVE_SAMPLER_STATE
      glBindSampler(0, 0);
      if (sampler) glDeleteSamplers(1, &sampler);
#endif
#if defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE)
      if (texture) glDeleteTextures(1, &texture);
#endif
#if defined(PS5_NATIVE_INDEXED_TRIANGLE) || defined(PS5_NATIVE_UNIFORM_MATRIX) || defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_ALPHA_BLEND)
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
   if (!cleanup_ok) { ps5_native_trace("OGL2_FAIL operation=teardown"); result = 1; }
   else if (!result) ps5_native_trace("OGL2_EGL_TEARDOWN_OK");
   return result;
}
