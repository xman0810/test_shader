#include "gbm.h"
#include "stdio.h"
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "GL/gl.h"
#include "GL/glext.h"


struct gbm_device *dev_;
int fd_;

EGLNativeDisplayType native_display_;
EGLNativeWindowType native_window_;
EGLDisplay egl_display_;
EGLContext egl_context_;
EGLConfig egl_config_;

int init_drm() {
  fd_ = open("/dev/dri/card0", O_RDWR);
  return true;
}

int init_gl_context() {
  dev_ = gbm_create_device(fd_);
  if (!dev_) {
    printf("Failed to create GBM device\n");
    return false;
  }

  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");

  if (get_platform_display != NULL) {
    egl_display_ = get_platform_display(EGL_PLATFORM_GBM_KHR, dev_, NULL);
  }

  if (!egl_display_) {
    printf("eglGetPlatformDisplayEXT() failed with error: 0x%x\n",
           eglGetError());
  }

  if (!eglInitialize(egl_display_, NULL, NULL)) {
    printf("eglInitialize() failed with error: 0x%x\n", eglGetError());
    egl_display_ = 0;
    return false;
  }
  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                           EGL_NONE};
  if (!eglBindAPI(EGL_OPENGL_API)) {
    printf("Failed to bind either EGL_OPENGL_API APIs.\n");
    return false;
  }
  egl_context_ =
      eglCreateContext(egl_display_, NULL, EGL_NO_CONTEXT, context_attribs);
  if (!egl_context_) {
    printf("eglCreateContext() failed with error: 0x%x\n", eglGetError());
    return false;
  }

  eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (!eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      egl_context_)) {
    printf("eglMakeCurrent failed with error: 0x%x\n", eglGetError());
    return false;
  }


  return true;
}


int main() {
  init_drm();
  init_gl_context();

  GLuint fbo;
  GLuint tmp_texture;
  glGenTextures(1, &tmp_texture);
  glBindTexture(GL_TEXTURE_2D, tmp_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tmp_texture, 0);

  const GLchar* vsCode = 
	  "#version 410\n" 
	  "in double inValue; \n" 
	  "out double outValue; \n" 
	  "void main()\n" 
	  "{\n"
	  "  outValue = inValue; \n"  
	  "}\n";

  GLuint shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(shader, 1, &vsCode, 0);
  glCompileShader(shader);
  GLchar infoLog[100];
  GLint len;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
  glGetShaderInfoLog(shader, 100, &len, infoLog);
  printf("compile log info : %s\n", infoLog);

  GLuint program = glCreateProgram();
  glAttachShader(program, shader);

  const GLchar* fbVaryings[] = {"outValue"};
  glTransformFeedbackVaryings(program, 1, fbVaryings, GL_INTERLEAVED_ATTRIBS);

  glLinkProgram(program);
  glUseProgram(program);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLdouble data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

  GLint inputAttrib = glGetAttribLocation(program, "inValue");
  glEnableVertexAttribArray(inputAttrib);
  glVertexAttribPointer(inputAttrib, 1, GL_DOUBLE, GL_FALSE, 0, 0);

  GLuint tbo;
  glGenBuffers(1, &tbo);
  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, tbo);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(data), NULL, GL_STATIC_READ);

  glEnable(GL_RASTERIZER_DISCARD);

  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbo);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, 5);
  glEndTransformFeedback();

  glDisable(GL_RASTERIZER_DISCARD);
  glFlush();

  GLfloat feedback[5];
  glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(feedback), feedback);
  printf("%f, %f, %f, %f, %f\n", feedback[0], feedback[1], feedback[2], feedback[3], feedback[4]);

  gbm_device_destroy(dev_);
}
