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
#if defined(PS5_NATIVE_DEPTH_TEXTURE)
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
      "   float d=texture(u_texture,v_texcoord).r;\n"
      "   color=vec4(0.0,d,0.0,1.0);\n"
      "}\n";
#elif defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_DYNAMIC_TEXTURE) || defined(PS5_NATIVE_FBO) || defined(PS5_NATIVE_RESOURCE_CYCLES)
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
   static const char *vs = "#version 330 core\n"
      "layout(location=0) in vec2 position;\n"
      "layout(location=1) in vec4 in_color;\n"
      "out vec4 v_color;\n"
      "void main(){\n"
      "   gl_Position=vec4(position,0.0,1.0);\n"
      "   v_color=in_color;\n"
      "}\n";
   static const char *fs = "#version 330 core\n"
      "in vec4 v_color;\n"
      "layout(location=0) out vec4 color;\n"
      "void main(){\n"
      "   color=v_color;\n"
      "}\n";
#elif defined(PS5_NATIVE_DEPTH_CULL)
   static const char *vs = "#version 330 core\n"
      "layout(location=0) in vec3 position;\n"
      "layout(location=1) in vec4 in_color;\n"
      "out vec4 v_color;\n"
      "void main(){\n"
      "   gl_Position=vec4(position,1.0);\n"
      "   v_color=in_color;\n"
      "}\n";
   static const char *fs = "#version 330 core\n"
      "in vec4 v_color;\n"
      "layout(location=0) out vec4 color;\n"
      "void main(){\n"
      "   color=v_color;\n"
      "}\n";
#elif defined(PS5_NATIVE_SCISSOR)
   static const char *vs = "#version 330 core\nlayout(location=0) in vec2 position;\n"
      "void main(){gl_Position=vec4(position,0.0,1.0);}\n";
   static const char *fs = "#version 330 core\nlayout(location=0) out vec4 color;\n"
      "void main(){color=vec4(0.0,1.0,0.0,1.0);}\n";
#elif defined(PS5_NATIVE_DYNAMIC_BUFFER)
   static const char *vs = "#version 330 core\nlayout(location=0) in vec2 position;\n"
      "void main(){gl_Position=vec4(position,0.0,1.0);}\n";
   static const char *fs = "#version 330 core\nlayout(location=0) out vec4 color;\n"
      "void main(){color=vec4(0.0,1.0,0.0,1.0);}\n";
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
   /* Vertex structure: {pos.x, pos.y, color.r, color.g, color.b, color.a}
    * Object 1 (vertices 0..3): Background vertical stripe x in [-0.2, 0.2], y in [-0.8, 0.8].
    *   Color: Green (0.0, 1.0, 0.0, 1.0) - channel 1 is immune to display R/B swap.
    * Object 2 (vertices 4..6): Foreground translucent triangle x in [-0.6, 0.6], y in [-0.4, 0.6].
    *   Color: GL Blue (0.0, 0.0, 1.0, 0.5) -> renders as TV Red with 50% alpha.
    * Clear color: GL Red (1.0, 0.0, 0.0, 1.0) -> renders as TV Blue.
    * Blending: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA.
    * Visual oracle:
    * - Translucent blending active:
    *     Background: Solid Blue.
    *     Stripe: Solid Green outside the triangle.
    *     Triangle wings: Purple / Violet (50% TV Red + 50% TV Blue).
    *     Triangle center: Olive / Amber (50% TV Red + 50% TV Green).
    *     Crucially: The green stripe is visibly seen shining right through the center
    *     of the triangle, rather than being occluded.
    * - Blending disabled / opaque:
    *     The triangle renders solid Red, completely blocking the stripe and cutting it in two.
    */
   static const GLfloat vertices[] = {
      /* Stripe (quad: 4 vertices) */
      -0.2f, -0.8f,  0.0f, 1.0f, 0.0f, 1.0f,
       0.2f, -0.8f,  0.0f, 1.0f, 0.0f, 1.0f,
       0.2f,  0.8f,  0.0f, 1.0f, 0.0f, 1.0f,
      -0.2f,  0.8f,  0.0f, 1.0f, 0.0f, 1.0f,
      /* Translucent triangle (3 vertices) */
      -0.6f, -0.4f,  0.0f, 0.0f, 1.0f, 0.5f,
       0.6f, -0.4f,  0.0f, 0.0f, 1.0f, 0.5f,
       0.0f,  0.6f,  0.0f, 0.0f, 1.0f, 0.5f
   };
   static const GLushort indices[] = {
      0, 1, 2,  0, 2, 3,
      4, 5, 6
   };
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
#elif defined(PS5_NATIVE_DYNAMIC_TEXTURE)
   /* Interleaved {pos.x, pos.y, uv.u, uv.v}.
    * Indices {0, 1, 3} form the centered triangle [-0.5, 0.5] matching control.
    * Base texture is 2x2 RGBA8:
    * Texel (0,0) and (1,0) (bottom row): solid bright Green {0, 255, 0, 255}.
    * Texel (0,1) and (1,1) (top row): initial Black {0, 0, 0, 255}.
    * Single-variable visual oracle:
    * - Dynamic subdata streaming works:
    *   Phase 1 (frames 0-299): partial 2x1 subregion update (top apex row) pulses in Green
    *   while the base stays solid green (5 full 2-second cycles).
    *   Phase 2 (frames 300-599): full 2x2 texture update pulses the entire triangle in Green
    *   in unison (5 full 2-second cycles).
    * - glTexSubImage2D fails / ignored: top apex stays black (partial triangle), never pulses.
    * - Background: solid Black. */
   static const GLfloat vertices[] = {
      -0.5f, -0.5f,  0.0f, 0.0f,
       0.5f, -0.5f,  1.0f, 0.0f,
      -0.5f, -0.5f,  0.0f, 0.0f,
       0.0f,  0.5f,  0.5f, 1.0f
   };
   static const GLushort indices[] = {0, 1, 3};
   static const uint8_t initial_texels[4][4] = {
      {   0, 255,   0, 255 }, /* (0,0): Green base left */
      {   0, 255,   0, 255 }, /* (1,0): Green base right */
      {   0,   0,   0, 255 }, /* (0,1): Black top left */
      {   0,   0,   0, 255 }  /* (1,1): Black top right */
   };
   GLuint ebo = 0;
   GLuint texture = 0;
   GLint u_tex_loc = -1;
#elif defined(PS5_NATIVE_FBO) || defined(PS5_NATIVE_DEPTH_TEXTURE) || defined(PS5_NATIVE_RESOURCE_CYCLES)
   /* Interleaved {pos.x, pos.y, uv.u, uv.v}.
    * Indices {0, 1, 3} form the centered triangle [-0.5, 0.5] matching control.
    * For FBO: 256x256 RGBA8 color attachment.
    * For Depth Texture: 256x256 GL_DEPTH_COMPONENT32F depth attachment.
    * Pass 1 renders offscreen into FBO.
    * Pass 2 renders onscreen to default backbuffer (black clear), texturing the triangle
    * with the offscreen FBO / depth texture.
    * Single-variable visual oracle:
    * - FBO / Depth texture rendering works:
    *   Centered green triangle smoothly pulsing in brightness (5 cycles over 20 seconds).
    * - Allocation / draw fails / ignored:
    *   Unwritten texture produces a black screen / no visible triangle.
    * - Background: solid Black. */
   static const GLfloat vertices[] = {
      -0.5f, -0.5f,  0.0f, 0.0f,
       0.5f, -0.5f,  1.0f, 0.0f,
      -0.5f, -0.5f,  0.0f, 0.0f,
       0.0f,  0.5f,  0.5f, 1.0f
   };
   static const GLushort indices[] = {0, 1, 3};
   GLuint ebo = 0;
   GLuint fbo = 0;
#if defined(PS5_NATIVE_FBO)
   GLuint fbo_texture = 0;
#elif defined(PS5_NATIVE_DEPTH_TEXTURE)
   GLuint depth_texture = 0;
#endif
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
#elif defined(PS5_NATIVE_INDEXED_TRIANGLE) || defined(PS5_NATIVE_SCISSOR) || defined(PS5_NATIVE_DYNAMIC_BUFFER)
   /* First three vertices are degenerate: ignoring the EBO cannot pass visually. */
   static const GLfloat vertices[] = {-0.5f,-0.5f, 0.5f,-0.5f, -0.5f,-0.5f, 0.0f,0.5f};
   static const GLushort indices[] = {0, 1, 3};
   GLuint ebo = 0;
#elif defined(PS5_NATIVE_DEPTH_CULL)
   /* Vertex structure: {pos.x, pos.y, pos.z, color.r, color.g, color.b, color.a} (stride = 7 floats)
    * Object 1 (vertices 0..2, indices 0,1,2, CCW):
    *   Geometry: [-0.5, 0.5], z = 0.0 (near, zw = 0.5).
    *   Color: Green (0.0, 1.0, 0.0, 1.0) - channel 1 is immune to display R/B swap.
    *   Drawn FIRST. Writes depth 0.5 to depth buffer.
    * Object 2 (vertices 3..5, indices 3,4,5, CCW):
    *   Geometry: [-0.8, 0.8], z = 0.8 (far, zw = 0.9).
    *   Color: Blue (0.0, 0.0, 1.0, 1.0) -> renders as TV Red.
    *   Drawn SECOND. Under glDepthFunc(GL_LESS), pixels overlapping Object 1
    *   fail depth test (0.9 < 0.5 is false) and are discarded. Only outer wings appear.
    * Object 3 (vertices 6..8, indices 6,7,8, CW):
    *   Geometry: [-0.5, 0.5], z = -0.5 (in front of Object 1, zw = 0.25).
    *   Color: White (1.0, 1.0, 1.0, 1.0).
    *   Drawn THIRD. Winding is CW. Under glCullFace(GL_BACK) + glFrontFace(GL_CCW),
    *   this triangle must be culled by hardware rasterizer and produce zero fragments.
    * Clear: Black (0.0, 0.0, 0.0, 1.0), ClearDepth = 1.0.
    */
   static const GLfloat vertices[] = {
      /* Object 1: Near triangle (CCW, Green, z=0.0) */
      -0.5f, -0.5f,  0.0f,  0.0f, 1.0f, 0.0f, 1.0f,
       0.5f, -0.5f,  0.0f,  0.0f, 1.0f, 0.0f, 1.0f,
       0.0f,  0.5f,  0.0f,  0.0f, 1.0f, 0.0f, 1.0f,

      /* Object 2: Far triangle (CCW, Blue/Red, z=0.8) */
      -0.8f, -0.8f,  0.8f,  0.0f, 0.0f, 1.0f, 1.0f,
       0.8f, -0.8f,  0.8f,  0.0f, 0.0f, 1.0f, 1.0f,
       0.0f,  0.8f,  0.8f,  0.0f, 0.0f, 1.0f, 1.0f,

      /* Object 3: Culled triangle (CW, White, z=-0.5) */
      -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f,
       0.0f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f,
       0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f
   };
   static const GLushort indices[] = {
      0, 1, 2,
      3, 4, 5,
      6, 7, 8
   };
   GLuint ebo = 0;
#else
   static const GLfloat vertices[] = {-0.5f,-0.5f, 0.5f,-0.5f, 0.0f,0.5f};
#endif
#if defined(PS5_NATIVE_DEPTH_CULL)
   static const EGLint configs[] = {EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
      EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
#else
   static const EGLint configs[] = {EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
      EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
#endif
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
#ifdef PS5_NATIVE_DYNAMIC_BUFFER
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), NULL, GL_STREAM_DRAW);
   {
      GLint buf_size = 0, buf_usage = 0;
      glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &buf_size);
      glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_USAGE, &buf_usage);
      CHECK(buf_size == (GLint)sizeof(vertices) && buf_usage == GL_STREAM_DRAW &&
            glGetError() == GL_NO_ERROR, "dynamic-buffer-param");
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
      CHECK(glGetError() == GL_NO_ERROR, "dynamic-buffer-subdata");
      ps5_native_trace("OGL3_DYNAMIC_BUFFER_SETUP_OK size=%d usage=0x%x\n", buf_size, buf_usage);
   }
#else
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
#endif
#if defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_DYNAMIC_TEXTURE) || defined(PS5_NATIVE_FBO) || defined(PS5_NATIVE_DEPTH_TEXTURE) || defined(PS5_NATIVE_RESOURCE_CYCLES)
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)0);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
   glEnableVertexAttribArray(1);
#elif defined(PS5_NATIVE_ALPHA_BLEND)
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const void *)0);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
   glEnableVertexAttribArray(1);
#elif defined(PS5_NATIVE_DEPTH_CULL)
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (const void *)0);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (const void *)(3 * sizeof(GLfloat)));
   glEnableVertexAttribArray(1);
#else
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
   glEnableVertexAttribArray(0);
#endif
   glUseProgram(program);
   glViewport(0, 0, width, height);
#ifdef PS5_NATIVE_ALPHA_BLEND
   glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
#else
   glClearColor(0, 0, 0, 1);
#endif
   CHECK(vao && vbo && glGetError() == GL_NO_ERROR, "vertex-setup");
#if defined(PS5_NATIVE_INDEXED_TRIANGLE) || defined(PS5_NATIVE_UNIFORM_MATRIX) || defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_ALPHA_BLEND) || defined(PS5_NATIVE_SCISSOR) || defined(PS5_NATIVE_DEPTH_CULL) || defined(PS5_NATIVE_DYNAMIC_BUFFER) || defined(PS5_NATIVE_DYNAMIC_TEXTURE) || defined(PS5_NATIVE_FBO) || defined(PS5_NATIVE_DEPTH_TEXTURE) || defined(PS5_NATIVE_RESOURCE_CYCLES)
   glGenBuffers(1, &ebo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
   CHECK(ebo && glGetError() == GL_NO_ERROR, "index-setup");
#ifdef PS5_NATIVE_ALPHA_BLEND
   ps5_native_trace("OGL3_INDEXED_SETUP_OK type=ushort count=9 indices=stripe(6)+triangle(3) offset=0");
#elif defined(PS5_NATIVE_DEPTH_CULL)
   ps5_native_trace("OGL3_INDEXED_SETUP_OK type=ushort count=9 indices=3triangles offset=0");
#else
   ps5_native_trace("OGL3_INDEXED_SETUP_OK type=ushort count=3 indices=0,1,3 offset=0");
#endif
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
#ifdef PS5_NATIVE_DYNAMIC_TEXTURE
   glGenTextures(1, &texture);
   CHECK(texture && glGetError() == GL_NO_ERROR, "texture-gen");
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   CHECK(glGetError() == GL_NO_ERROR, "texture-param");
   {
      GLint q_min = 0, q_mag = 0, q_wrap_s = 0, q_wrap_t = 0;
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &q_min);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &q_mag);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &q_wrap_s);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &q_wrap_t);
      CHECK(q_min == GL_NEAREST && q_mag == GL_NEAREST &&
            q_wrap_s == GL_CLAMP_TO_EDGE && q_wrap_t == GL_CLAMP_TO_EDGE &&
            glGetError() == GL_NO_ERROR, "dynamic-texture-param");
      static const uint8_t black_texels[4][4] = {
         { 0, 0, 0, 255 }, { 0, 0, 0, 255 },
         { 0, 0, 0, 255 }, { 0, 0, 0, 255 }
      };
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, black_texels);
      CHECK(glGetError() == GL_NO_ERROR, "texture-upload");
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, initial_texels);
      CHECK(glGetError() == GL_NO_ERROR, "dynamic-texture-subdata");
      u_tex_loc = glGetUniformLocation(program, "u_texture");
      CHECK(u_tex_loc >= 0 && glGetError() == GL_NO_ERROR, "texture-location");
      glUniform1i(u_tex_loc, 0);
      CHECK(glGetError() == GL_NO_ERROR, "texture-uniform");
      ps5_native_trace("OGL3_DYNAMIC_TEXTURE_SETUP_OK tex=%u loc=%d min=0x%x mag=0x%x wrap_s=0x%x wrap_t=0x%x\n",
                       texture, u_tex_loc, q_min, q_mag, q_wrap_s, q_wrap_t);
   }
#endif
#ifdef PS5_NATIVE_FBO
   glGenTextures(1, &fbo_texture);
   CHECK(fbo_texture && glGetError() == GL_NO_ERROR, "texture-gen");
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, fbo_texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   CHECK(glGetError() == GL_NO_ERROR, "texture-upload");

   glGenFramebuffers(1, &fbo);
   CHECK(fbo && glGetError() == GL_NO_ERROR, "fbo-gen");
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   CHECK(glGetError() == GL_NO_ERROR, "fbo-bind");
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);
   CHECK(glGetError() == GL_NO_ERROR, "fbo-attach");
   {
      GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      CHECK(fbo_status == GL_FRAMEBUFFER_COMPLETE && glGetError() == GL_NO_ERROR, "fbo-complete");

      GLint attach_type = 0;
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attach_type);
      CHECK(attach_type == GL_TEXTURE && glGetError() == GL_NO_ERROR, "fbo-attachment");

      u_tex_loc = glGetUniformLocation(program, "u_texture");
      CHECK(u_tex_loc >= 0 && glGetError() == GL_NO_ERROR, "texture-location");
      glUniform1i(u_tex_loc, 0);
      CHECK(glGetError() == GL_NO_ERROR, "texture-uniform");

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      CHECK(glGetError() == GL_NO_ERROR, "fbo-unbind");

      ps5_native_trace("OGL3_FBO_SETUP_OK fbo=%u tex=%u status=0x%x\n",
                       fbo, fbo_texture, fbo_status);
   }
#endif
#ifdef PS5_NATIVE_DEPTH_TEXTURE
   glGenTextures(1, &depth_texture);
   CHECK(depth_texture && glGetError() == GL_NO_ERROR, "depth-texture-gen");
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, depth_texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 256, 256, 0,
                GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
   CHECK(glGetError() == GL_NO_ERROR, "depth-texture-upload");

   {
      GLint internal_fmt = 0;
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_fmt);
      CHECK(internal_fmt == GL_DEPTH_COMPONENT32F && glGetError() == GL_NO_ERROR, "depth-texture-format");
   }

   glGenFramebuffers(1, &fbo);
   CHECK(fbo && glGetError() == GL_NO_ERROR, "fbo-gen");
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   CHECK(glGetError() == GL_NO_ERROR, "fbo-bind");
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
   CHECK(glGetError() == GL_NO_ERROR, "fbo-attach");

   glDrawBuffer(GL_NONE);
   glReadBuffer(GL_NONE);
   CHECK(glGetError() == GL_NO_ERROR, "fbo-buffers-none");

   {
      GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      CHECK(fbo_status == GL_FRAMEBUFFER_COMPLETE && glGetError() == GL_NO_ERROR, "fbo-complete");

      GLint attach_type = 0;
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attach_type);
      CHECK(attach_type == GL_TEXTURE && glGetError() == GL_NO_ERROR, "fbo-attachment");

      GLint draw_buf = -1, read_buf = -1;
      glGetIntegerv(GL_DRAW_BUFFER, &draw_buf);
      glGetIntegerv(GL_READ_BUFFER, &read_buf);
      CHECK(draw_buf == GL_NONE && read_buf == GL_NONE && glGetError() == GL_NO_ERROR, "fbo-buffer-query");

      u_tex_loc = glGetUniformLocation(program, "u_texture");
      CHECK(u_tex_loc >= 0 && glGetError() == GL_NO_ERROR, "texture-location");
      glUniform1i(u_tex_loc, 0);
      CHECK(glGetError() == GL_NO_ERROR, "texture-uniform");

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      CHECK(glGetError() == GL_NO_ERROR, "fbo-unbind");

      ps5_native_trace("OGL3_DEPTH_TEXTURE_SETUP_OK fbo=%u tex=%u status=0x%x\n",
                       fbo, depth_texture, fbo_status);
   }
#endif
#ifdef PS5_NATIVE_RESOURCE_CYCLES
   u_tex_loc = glGetUniformLocation(program, "u_texture");
   CHECK(u_tex_loc >= 0 && glGetError() == GL_NO_ERROR, "texture-location");
   glUniform1i(u_tex_loc, 0);
   CHECK(glGetError() == GL_NO_ERROR, "texture-uniform");
   ps5_native_trace("OGL3_RESOURCE_CYCLES_SETUP_OK cycles_per_frame=1\n");
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
#ifdef PS5_NATIVE_SCISSOR
   {
      GLint sc_x = width / 2;
      GLint sc_y = 0;
      GLsizei sc_w = width - sc_x;
      GLsizei sc_h = height;
      glEnable(GL_SCISSOR_TEST);
      CHECK(glIsEnabled(GL_SCISSOR_TEST), "scissor-enable");
      glScissor(sc_x, sc_y, sc_w, sc_h);
      CHECK(glGetError() == GL_NO_ERROR, "scissor-setup");
      GLint box[4] = {0};
      glGetIntegerv(GL_SCISSOR_BOX, box);
      CHECK(box[0] == sc_x && box[1] == sc_y && box[2] == (GLint)sc_w && box[3] == (GLint)sc_h &&
            glGetError() == GL_NO_ERROR, "scissor-query");
      ps5_native_trace("OGL3_SCISSOR_SETUP_OK x=%d y=%d w=%d h=%d\n", box[0], box[1], box[2], box[3]);
   }
#endif
#ifdef PS5_NATIVE_DEPTH_CULL
   {
      glEnable(GL_DEPTH_TEST);
      CHECK(glIsEnabled(GL_DEPTH_TEST), "depth-enable");
      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);
      glClearDepth(1.0);
      CHECK(glGetError() == GL_NO_ERROR, "depth-setup");

      glEnable(GL_CULL_FACE);
      CHECK(glIsEnabled(GL_CULL_FACE), "cull-enable");
      glCullFace(GL_BACK);
      glFrontFace(GL_CCW);
      CHECK(glGetError() == GL_NO_ERROR, "cull-setup");

      GLint depth_func = 0;
      GLboolean depth_mask = GL_FALSE;
      GLint cull_mode = 0;
      GLint front_face = 0;
      glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
      glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
      glGetIntegerv(GL_CULL_FACE_MODE, &cull_mode);
      glGetIntegerv(GL_FRONT_FACE, &front_face);
      CHECK(depth_func == GL_LESS && depth_mask == GL_TRUE &&
            cull_mode == GL_BACK && front_face == GL_CCW &&
            glGetError() == GL_NO_ERROR, "depth-cull-query");
      ps5_native_trace("OGL3_DEPTH_CULL_SETUP_OK depth_func=0x%x depth_mask=%d cull_mode=0x%x front_face=0x%x\n",
                       depth_func, (int)depth_mask, cull_mode, front_face);
   }
#endif
   start = sceKernelGetProcessTime();
   for (frames = 0; frames < 600; ++frames) {
      CHECK(sceKernelGetProcessTime() - start < UINT64_C(30000000), "frame-deadline");
#if defined(PS5_NATIVE_RESOURCE_CYCLES)
      /* --- Resource Allocation Phase --- */
      GLuint frame_vbo = 0;
      glGenBuffers(1, &frame_vbo);
      CHECK(frame_vbo && glGetError() == GL_NO_ERROR, "resource-cycles-vbo-gen");
      glBindBuffer(GL_ARRAY_BUFFER, frame_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
      CHECK(glGetError() == GL_NO_ERROR, "resource-cycles-vbo-data");

      GLuint frame_tex = 0;
      glGenTextures(1, &frame_tex);
      CHECK(frame_tex && glGetError() == GL_NO_ERROR, "resource-cycles-tex-gen");
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, frame_tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      CHECK(glGetError() == GL_NO_ERROR, "resource-cycles-tex-alloc");

      GLuint frame_fbo = 0;
      glGenFramebuffers(1, &frame_fbo);
      CHECK(frame_fbo && glGetError() == GL_NO_ERROR, "resource-cycles-fbo-gen");
      glBindFramebuffer(GL_FRAMEBUFFER, frame_fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame_tex, 0);
      CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
            glGetError() == GL_NO_ERROR, "resource-cycles-fbo-complete");

      /* --- Pass 1: Offscreen Render into Transient FBO --- */
      glViewport(0, 0, 256, 256);
      {
         int cycle_frame = frames % 120;
         float t = (cycle_frame < 60) ? ((float)cycle_frame / 60.0f) : ((float)(120 - cycle_frame) / 60.0f);
         float g = (30.0f + 225.0f * t) / 255.0f;
         glClearColor(0.0f, g, 0.0f, 1.0f);
      }
      glClear(GL_COLOR_BUFFER_BIT);
      CHECK(glGetError() == GL_NO_ERROR, "resource-cycles-fbo-draw");

      /* --- Pass 2: Onscreen Render to Default Backbuffer --- */
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, width, height);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glBindBuffer(GL_ARRAY_BUFFER, frame_vbo);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)0);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
      glEnableVertexAttribArray(1);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, frame_tex);
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL);
      glFinish();
      CHECK(glGetError() == GL_NO_ERROR, "resource-cycles-draw");

      /* --- Resource Destruction Phase --- */
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &frame_fbo);
      glBindTexture(GL_TEXTURE_2D, 0);
      glDeleteTextures(1, &frame_tex);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &frame_vbo);
      CHECK(glGetError() == GL_NO_ERROR, "resource-cycles-delete");
#elif defined(PS5_NATIVE_FBO) || defined(PS5_NATIVE_DEPTH_TEXTURE)
      /* Pass 1: Offscreen render into FBO */
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glViewport(0, 0, 256, 256);
#if defined(PS5_NATIVE_FBO)
      {
         int cycle_frame = frames % 120;
         float t = (cycle_frame < 60) ? ((float)cycle_frame / 60.0f) : ((float)(120 - cycle_frame) / 60.0f);
         float g = (30.0f + 225.0f * t) / 255.0f;
         glClearColor(0.0f, g, 0.0f, 1.0f);
      }
      glClear(GL_COLOR_BUFFER_BIT);
      CHECK(glGetError() == GL_NO_ERROR, "fbo-draw");
#elif defined(PS5_NATIVE_DEPTH_TEXTURE)
      {
         int cycle_frame = frames % 120;
         float t = (cycle_frame < 60) ? ((float)cycle_frame / 60.0f) : ((float)(120 - cycle_frame) / 60.0f);
         float d = (38.0f + 217.0f * t) / 255.0f;
         glClearDepth(d);
      }
      glClear(GL_DEPTH_BUFFER_BIT);
      CHECK(glGetError() == GL_NO_ERROR, "fbo-depth-draw");
#endif

      /* Pass 2: Switch to default framebuffer */
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, width, height);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      glActiveTexture(GL_TEXTURE0);
#if defined(PS5_NATIVE_FBO)
      glBindTexture(GL_TEXTURE_2D, fbo_texture);
#elif defined(PS5_NATIVE_DEPTH_TEXTURE)
      glBindTexture(GL_TEXTURE_2D, depth_texture);
#endif
#else
#if defined(PS5_NATIVE_SCISSOR)
      glDisable(GL_SCISSOR_TEST);
#endif
#if defined(PS5_NATIVE_DEPTH_CULL)
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#else
      glClear(GL_COLOR_BUFFER_BIT);
#endif
#if defined(PS5_NATIVE_SCISSOR)
      glEnable(GL_SCISSOR_TEST);
      glScissor(width / 2, 0, width - (width / 2), height);
#endif
#endif
#ifdef PS5_NATIVE_UNIFORM_MATRIX
      glUniformMatrix4fv(u_loc, 1, GL_FALSE, transform_matrix);
#endif
#ifdef PS5_NATIVE_DYNAMIC_BUFFER
      int cycle_frame = frames % 120;
      float t = (cycle_frame < 60) ? ((float)cycle_frame / 60.0f) : ((float)(120 - cycle_frame) / 60.0f);
      float dx = -0.35f + 0.70f * t;
      GLfloat dynamic_vertices[] = {
         -0.5f + dx, -0.5f,
          0.5f + dx, -0.5f,
         -0.5f + dx, -0.5f,
          0.0f + dx,  0.5f
      };
      if (frames < 300) {
         /* First 300 frames: test buffer orphaning before subdata (MonoGame Discard path) */
         glBufferData(GL_ARRAY_BUFFER, sizeof(dynamic_vertices), NULL, GL_STREAM_DRAW);
      }
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(dynamic_vertices), dynamic_vertices);
#endif
#ifdef PS5_NATIVE_DYNAMIC_TEXTURE
      int cycle_frame = frames % 120;
      float t = (cycle_frame < 60) ? ((float)cycle_frame / 60.0f) : ((float)(120 - cycle_frame) / 60.0f);
      uint8_t g = (uint8_t)(30 + 225.0f * t);
      if (frames < 300) {
         /* Phase 1 (frames 0-299): partial 2x1 subregion update (top apex row) */
         uint8_t top_row[2][4] = {
            { 0, g, 0, 255 },
            { 0, g, 0, 255 }
         };
         glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, top_row);
      } else {
         /* Phase 2 (frames 300-599): full 2x2 texture update */
         uint8_t full_tex[4][4] = {
            { 0, g, 0, 255 }, { 0, g, 0, 255 },
            { 0, g, 0, 255 }, { 0, g, 0, 255 }
         };
         glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, full_tex);
      }
      CHECK(glGetError() == GL_NO_ERROR, "dynamic-texture-subdata");
#endif
#if !defined(PS5_NATIVE_RESOURCE_CYCLES)
#if defined(PS5_NATIVE_ALPHA_BLEND)
      /* Draw opaque background stripe first with blending disabled */
      glDisable(GL_BLEND);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const void *)0);
      /* Draw translucent foreground triangle with blending enabled */
      glEnable(GL_BLEND);
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (const void *)(6 * sizeof(GLushort)));
#elif defined(PS5_NATIVE_DEPTH_CULL)
      /* Draw Object 1: Near triangle (CCW, Green, z=0.0) -> offset 0 */
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (const void *)0);
      /* Draw Object 2: Far triangle (CCW, Blue/Red, z=0.8) -> offset 3 * sizeof(GLushort) */
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (const void *)(3 * sizeof(GLushort)));
      /* Draw Object 3: Culled triangle (CW, White, z=-0.5) -> offset 6 * sizeof(GLushort) */
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (const void *)(6 * sizeof(GLushort)));
#elif defined(PS5_NATIVE_INDEXED_TRIANGLE) || defined(PS5_NATIVE_UNIFORM_MATRIX) || defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_SCISSOR) || defined(PS5_NATIVE_DYNAMIC_BUFFER) || defined(PS5_NATIVE_DYNAMIC_TEXTURE) || defined(PS5_NATIVE_FBO) || defined(PS5_NATIVE_DEPTH_TEXTURE)
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL);
#else
      glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
      glFinish();
      CHECK(glGetError() == GL_NO_ERROR, "draw-finish");
#endif
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
#ifdef PS5_NATIVE_SCISSOR
      glDisable(GL_SCISSOR_TEST);
      if (glIsEnabled(GL_SCISSOR_TEST)) cleanup_ok = 0;
#endif
#ifdef PS5_NATIVE_DEPTH_CULL
      glDisable(GL_DEPTH_TEST);
      if (glIsEnabled(GL_DEPTH_TEST)) cleanup_ok = 0;
      glDisable(GL_CULL_FACE);
      if (glIsEnabled(GL_CULL_FACE)) cleanup_ok = 0;
#endif
#ifdef PS5_NATIVE_SAMPLER_STATE
      glBindSampler(0, 0);
      if (sampler) glDeleteSamplers(1, &sampler);
#endif
#ifdef PS5_NATIVE_FBO
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      if (fbo) glDeleteFramebuffers(1, &fbo);
      if (fbo_texture) glDeleteTextures(1, &fbo_texture);
#endif
#ifdef PS5_NATIVE_DEPTH_TEXTURE
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      if (fbo) glDeleteFramebuffers(1, &fbo);
      if (depth_texture) glDeleteTextures(1, &depth_texture);
#endif
#if defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_DYNAMIC_TEXTURE)
      if (texture) glDeleteTextures(1, &texture);
#endif
#if defined(PS5_NATIVE_INDEXED_TRIANGLE) || defined(PS5_NATIVE_UNIFORM_MATRIX) || defined(PS5_NATIVE_TEXTURE_2D) || defined(PS5_NATIVE_SAMPLER_STATE) || defined(PS5_NATIVE_ALPHA_BLEND) || defined(PS5_NATIVE_SCISSOR) || defined(PS5_NATIVE_DEPTH_CULL) || defined(PS5_NATIVE_DYNAMIC_BUFFER) || defined(PS5_NATIVE_DYNAMIC_TEXTURE) || defined(PS5_NATIVE_FBO) || defined(PS5_NATIVE_DEPTH_TEXTURE) || defined(PS5_NATIVE_RESOURCE_CYCLES)
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
