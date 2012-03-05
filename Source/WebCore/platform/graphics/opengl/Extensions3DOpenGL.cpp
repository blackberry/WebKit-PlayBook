/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "Extensions3DOpenGL.h"

#include "GraphicsContext3D.h"
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include "ANGLE/ShaderLang.h"
#include <OpenGL/gl.h>
#elif PLATFORM(GTK)
#include "OpenGLShims.h"
#elif PLATFORM(QT)
#include <cairo/OpenGLShims.h>
#elif PLATFORM(BLACKBERRY)
#include <EGL/egl.h>
#endif

namespace WebCore {

Extensions3DOpenGL::Extensions3DOpenGL(GraphicsContext3D* context)
    : m_initializedAvailableExtensions(false)
    , m_context(context)
#if PLATFORM(BLACKBERRY)
    , m_supportsOESvertexArrayObject(false)
    , m_supportsIMGMultisampledRenderToTexture(false)
    , m_glFramebufferTexture2DMultisampleIMG(0)
    , m_glRenderbufferStorageMultisampleIMG(0)
    , m_glBindVertexArrayOES(0)
    , m_glDeleteVertexArraysOES(0)
    , m_glGenVertexArraysOES(0)
    , m_glIsVertexArrayOES(0)
#endif
{
}

Extensions3DOpenGL::~Extensions3DOpenGL()
{
}

bool Extensions3DOpenGL::supports(const String& name)
{
    // Note on support for BGRA:
    //
    // For OpenGL ES2.0, requires checking for
    // GL_EXT_texture_format_BGRA8888 and GL_EXT_read_format_bgra.
    // For desktop GL, BGRA has been supported since OpenGL 1.2.
    //
    // However, note that the GL ES2 extension requires the
    // internalFormat to glTexImage2D() be GL_BGRA, while desktop GL
    // will not accept GL_BGRA (must be GL_RGBA), so this must be
    // checked on each platform. Desktop GL offers neither
    // GL_EXT_texture_format_BGRA8888 or GL_EXT_read_format_bgra, so
    // treat them as unsupported here.
    if (!m_initializedAvailableExtensions) {
        String extensionsString(reinterpret_cast<const char*>(::glGetString(GL_EXTENSIONS)));
        Vector<String> availableExtensions;
        extensionsString.split(" ", availableExtensions);
        for (size_t i = 0; i < availableExtensions.size(); ++i)
            m_availableExtensions.add(availableExtensions[i]);
        m_initializedAvailableExtensions = true;
    }
    
#if !PLATFORM(BLACKBERRY)
    // GL_ANGLE_framebuffer_blit and GL_ANGLE_framebuffer_multisample are "fake". They are implemented using other
    // extensions. In particular GL_EXT_framebuffer_blit and GL_EXT_framebuffer_multisample
    if (name == "GL_ANGLE_framebuffer_blit")
        return m_availableExtensions.contains("GL_EXT_framebuffer_blit");
    if (name == "GL_ANGLE_framebuffer_multisample")
        return m_availableExtensions.contains("GL_EXT_framebuffer_multisample");

    // Desktop GL always supports GL_OES_rgb8_rgba8.
    if (name == "GL_OES_rgb8_rgba8")
        return true;

    // If GL_ARB_texture_float is available then we report GL_OES_texture_float and
    // GL_OES_texture_half_float as available.
    if (name == "GL_OES_texture_float" || name == "GL_OES_texture_half_float")
        return m_availableExtensions.contains("GL_ARB_texture_float");
    
    // GL_OES_vertex_array_object
    if (name == "GL_OES_vertex_array_object")
        return m_availableExtensions.contains("GL_APPLE_vertex_array_object");

    // Desktop GL always supports the standard derivative functions
    if (name == "GL_OES_standard_derivatives")
        return true;

    return m_availableExtensions.contains(name);
#else
    if (m_availableExtensions.contains(name)) {
        if (name == "GL_OES_vertex_array_object" && !m_supportsOESvertexArrayObject) {
            m_glBindVertexArrayOES = reinterpret_cast<PFNGLBINDVERTEXARRAYOESPROC>(eglGetProcAddress("glBindVertexArrayOES"));
            m_glGenVertexArraysOES = reinterpret_cast<PFNGLGENVERTEXARRAYSOESPROC>(eglGetProcAddress("glGenVertexArraysOES"));
            m_glDeleteVertexArraysOES = reinterpret_cast<PFNGLDELETEVERTEXARRAYSOESPROC>(eglGetProcAddress("glDeleteVertexArraysOES"));
            m_glIsVertexArrayOES = reinterpret_cast<PFNGLISVERTEXARRAYOESPROC>(eglGetProcAddress("glIsVertexArrayOES"));
            m_supportsOESvertexArrayObject = true;
        } else if (name == "GL_IMG_multisampled_render_to_texture" && !m_supportsIMGMultisampledRenderToTexture) {
            m_glFramebufferTexture2DMultisampleIMG = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC>(eglGetProcAddress("glFramebufferTexture2DMultisampleIMG"));
            m_glRenderbufferStorageMultisampleIMG = reinterpret_cast<PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC>(eglGetProcAddress("glRenderbufferStorageMultisampleIMG"));
            m_supportsIMGMultisampledRenderToTexture = true;
        }
        return true;
    } else {
        return false;
    }
#endif
}

void Extensions3DOpenGL::ensureEnabled(const String& name)
{
#if PLATFORM(MAC) || PLATFORM(QT)
    if (name == "GL_OES_standard_derivatives") {
        // Enable support in ANGLE (if not enabled already)
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.OES_standard_derivatives) {
            ANGLEResources.OES_standard_derivatives = 1;
            compiler.setResources(ANGLEResources);
        }
    }
#else
    ASSERT_UNUSED(name, supports(name));
#endif
}

bool Extensions3DOpenGL::isEnabled(const String& name)
{
#if PLATFORM(MAC) || PLATFORM(QT)
    if (name == "GL_OES_standard_derivatives") {
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        return compiler.getResources().OES_standard_derivatives;
    }
#endif
    return supports(name);
}

int Extensions3DOpenGL::getGraphicsResetStatusARB()
{
    return GraphicsContext3D::NO_ERROR;
}

#if PLATFORM(BLACKBERRY)
void Extensions3DOpenGL::framebufferTexture2DMultisampleIMG(unsigned long target, unsigned long attachment, unsigned long textarget, unsigned int texture, int level, unsigned long samples)
{
    if (m_glFramebufferTexture2DMultisampleIMG)
        m_glFramebufferTexture2DMultisampleIMG(target, attachment, textarget, texture, level, samples);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DOpenGL::renderbufferStorageMultisampleIMG(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    if (m_glRenderbufferStorageMultisampleIMG)
        m_glRenderbufferStorageMultisampleIMG(target, samples, internalformat, width, height);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}
#else
void Extensions3DOpenGL::blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter)
{
    ::glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void Extensions3DOpenGL::renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    ::glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width, height);
}
#endif

Platform3DObject Extensions3DOpenGL::createVertexArrayOES()
{
    m_context->makeContextCurrent();
#if !PLATFORM(GTK) && !PLATFORM(QT) && defined(GL_APPLE_vertex_array_object) && GL_APPLE_vertex_array_object
    GLuint array = 0;
    glGenVertexArraysAPPLE(1, &array);
    return array;
#elif PLATFORM(BLACKBERRY)
    if (m_glGenVertexArraysOES) {
        GLuint array = 0;
        m_glGenVertexArraysOES(1, &array);
        return array;
    } else {
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
        return 0;
    }
#else
    return 0;
#endif
}

void Extensions3DOpenGL::deleteVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return;
    
    m_context->makeContextCurrent();
#if !PLATFORM(GTK) && !PLATFORM(QT) && defined(GL_APPLE_vertex_array_object) && GL_APPLE_vertex_array_object
    glDeleteVertexArraysAPPLE(1, &array);
#elif PLATFORM(BLACKBERRY)
    if (m_glDeleteVertexArraysOES)
        m_glDeleteVertexArraysOES(1, &array);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
#endif
}

GC3Dboolean Extensions3DOpenGL::isVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return GL_FALSE;
    
    m_context->makeContextCurrent();
#if !PLATFORM(GTK) && !PLATFORM(QT) && defined(GL_APPLE_vertex_array_object) && GL_APPLE_vertex_array_object
    return glIsVertexArrayAPPLE(array);
#elif PLATFORM(BLACKBERRY)
    if (m_glIsVertexArrayOES)
        return m_glIsVertexArrayOES(array);
    else {
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
        return false;
    }
#else
    return GL_FALSE;
#endif
}

void Extensions3DOpenGL::bindVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return;

    m_context->makeContextCurrent();
#if !PLATFORM(GTK) && !PLATFORM(QT) && defined(GL_APPLE_vertex_array_object) && GL_APPLE_vertex_array_object
    glBindVertexArrayAPPLE(array);
#elif PLATFORM(BLACKBERRY)
    if (m_glBindVertexArrayOES)
        m_glBindVertexArrayOES(array);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
#endif
}

String Extensions3DOpenGL::getTranslatedShaderSourceANGLE(Platform3DObject shader)
{
    UNUSED_PARAM(shader);
    return "";
    // FIXME: implement this function and add GL_ANGLE_translated_shader_source in supports().
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
