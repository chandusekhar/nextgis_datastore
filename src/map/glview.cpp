/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2016 NextGIS, <info@nextgis.com>
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include "glview.h"
#include "constants.h"

#include <iostream>
#include "cpl_error.h"
#include "cpl_string.h"

/* Links:
//https://mkonrad.net/2014/12/08/android-off-screen-rendering-using-egl-pixelbuffers.html
//http://stackoverflow.com/questions/214437/opengl-fast-off-screen-rendering
//http://stackoverflow.com/questions/14785007/can-i-use-opengl-for-off-screen-rendering/14796456#14796456
//https://gist.github.com/CartBlanche/1271517
//http://stackoverflow.com/questions/21151259/replacing-glreadpixels-with-egl-khr-image-base-for-faster-pixel-copy
//https://vec.io/posts/faster-alternatives-to-glreadpixels-and-glteximage2d-in-opengl-es
//https://www.khronos.org/registry/egl/sdk/docs/man/html/eglIntro.xhtml
//https://wiki.maemo.org/SimpleGL_example
//http://stackoverflow.com/questions/12906971/difference-from-eglcreatepbuffersurface-and-eglcreatepixmapsurface-with-opengl-e
//http://stackoverflow.com/questions/25504188/is-it-possible-to-use-pixmaps-on-android-via-java-api-for-gles

//https://solarianprogrammer.com/2013/05/13/opengl-101-drawing-primitives/
//http://www.glprogramming.com/red/chapter02.html
//https://www3.ntu.edu.sg/home/ehchua/programming/opengl/CG_Introduction.html
//https://www3.ntu.edu.sg/home/ehchua/programming/android/Android_3D.html
//https://www.opengl.org/sdk/docs/man2/xhtml/gluUnProject.xml
//https://www.opengl.org/sdk/docs/man2/xhtml/gluProject.xml

//https://github.com/libmx3/mx3/blob/master/src/event_loop.cpp

//https://www.mapbox.com/blog/drawing-antialiased-lines/
//https://github.com/afiskon/cpp-opengl-vbo-vao-shaders/blob/master/main.cpp
*/

using namespace ngs;

//------------------------------------------------------------------------------
// GlView
//------------------------------------------------------------------------------

GlView::GlView() : m_eglCtx(EGL_NO_CONTEXT),
    m_eglSurface(EGL_NO_SURFACE), m_displayWidth(100),
    m_displayHeight(100), m_extensionLoad(false), m_programLoad(false),
    m_programId(0)
{
}

GlView::~GlView()
{
    if(m_programId)
        glDeleteProgram(m_programId);

    eglDestroyContext( m_glDisplay->eglDisplay (), m_eglCtx );
    eglDestroySurface( m_glDisplay->eglDisplay (), m_eglSurface );

    m_eglSurface = EGL_NO_SURFACE;
    m_eglCtx = EGL_NO_CONTEXT;
}

bool GlView::init()
{
    // get or create Gl display
    m_glDisplay = getGlDisplay ();
    if(!m_glDisplay) {
        CPLError(CE_Failure, CPLE_OpenFailed, "GL display is not initialized.");
        return false;
    }

    // EGL context attributes
    const EGLint ctxAttr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,              // very important!
        EGL_NONE
    };

    m_eglCtx = eglCreateContext(m_glDisplay->eglDisplay (),
                                m_glDisplay->eglConf (), EGL_NO_CONTEXT, ctxAttr);
    if (m_eglCtx == EGL_NO_CONTEXT) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Create GL context failed.");
        return false;
    }

#ifdef _DEBUG
    checkEGLError("eglCreateContext");
#endif // _DEBUG

    m_eglSurface = EGL_NO_SURFACE;

    return true;
}

void GlView::setSize(int width, int height)
{
    if(m_displayWidth == width && m_displayHeight == height)
        return;

    m_displayWidth = width;
    m_displayHeight = height;

#ifdef _DEBUG
    cout << "Size changed" << endl;
#endif // _DEBUG

    if(!createSurface ())
        return;

#ifdef _DEBUG
    EGLint w, h;
    eglQuerySurface(m_glDisplay->eglDisplay (), m_eglSurface, EGL_WIDTH, &w);
    checkEGLError ("eglQuerySurface");
    cout << "EGL_WIDTH: " << w << endl;
    eglQuerySurface(m_glDisplay->eglDisplay (), m_eglSurface, EGL_HEIGHT, &h);
    checkEGLError ("eglQuerySurface");
    cout << "EGL_HEIGHT: " << h << endl;
#endif // _DEBUG

    loadExtensions();
    loadProgram ();
    ngsCheckGLEerror(glClearColor(m_bkColor.r, m_bkColor.g, m_bkColor.b,
                                   m_bkColor.a));

    ngsCheckGLEerror(glEnable(GL_DEPTH_TEST));
    ngsCheckGLEerror(glViewport ( 0, 0, m_displayWidth, m_displayHeight ));
}

bool GlView::isOk() const
{
    return EGL_NO_SURFACE != m_eglSurface;
}

void GlView::setBackgroundColor(const ngsRGBA &color)
{
    m_bkColor.r = float(color.R) / 255;
    m_bkColor.g = float(color.G) / 255;
    m_bkColor.b = float(color.B) / 255;
    m_bkColor.a = float(color.A) / 255;
    ngsCheckGLEerror(glClearColor(m_bkColor.r, m_bkColor.g, m_bkColor.b,
                                  m_bkColor.a));
}

void GlView::fillBuffer(void *buffer) const
{
    if(nullptr == buffer)
        return;
    ngsCheckEGLEerror(eglSwapBuffers(m_glDisplay->eglDisplay (), m_eglSurface));
    ngsCheckGLEerror(glReadPixels(0, 0, m_displayWidth, m_displayHeight,
                                   GL_RGBA, GL_UNSIGNED_BYTE, buffer));
}

void GlView::clearBackground()
{    
    ngsCheckGLEerror(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void GlView::prepare(const Matrix4 &mat)
{
    clearBackground();
    ngsCheckGLEerror(glUseProgram(m_programId));

#ifdef _DEBUG
    GLint numActiveUniforms = 0;
    glGetProgramiv(m_programId, GL_ACTIVE_UNIFORMS, &numActiveUniforms);
    cout << "Number active uniforms: " << numActiveUniforms << endl;
#endif //_DEBUG
    GLint location = glGetUniformLocation(m_programId, "mvMatrix");

    array<GLfloat, 16> mat4f = mat.dataF ();
    ngsCheckGLEerror(glUniformMatrix4fv(location, 1, GL_FALSE, mat4f.data()));
}

// WARNING: this is for test
void GlView::draw() const
{
    int uColorLocation = glGetUniformLocation(m_programId, "u_Color");

    GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                          };

    glUniform4f(uColorLocation, 1.0f, 0.0f, 0.0f, 1.0f);

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                           -2236992.0f, 3972353.0f, 0.5f,
                            5187591.0f, 4509961.0f, 0.5f
                          };

    glUniform4f(uColorLocation, 0.0f, 0.0f, 1.0f, 1.0f);

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices2 ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));
}

void GlView::drawPolygons(const vector<GLfloat> &vertices,
                          const vector<GLushort> &indices) const
{
    if(vertices.empty() || indices.empty ())
        return;
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));
    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                            vertices.data ()));

    // IBO
    /*GLuint IBO;
    glGenBuffers(1, &IBO );
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size () * sizeof(GLushort), indices.data (), GL_STATIC_DRAW);*/

    //glDrawArrays(GL_TRIANGLES, 0, 3); // this works
    //glDrawElements(GL_TRIANGLES, indices.size (), GL_UNSIGNED_INT, 0); // this doesnt


    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, indices.size (),
                                    GL_UNSIGNED_SHORT, indices.data ()));
}

void GlView::loadProgram()
{
    if(!m_programLoad) {
        // TODO: need special class to load/undload and manage shaders
        m_programId = prepareProgram();
        if(!m_programId) {
            CPLError(CE_Failure, CPLE_OpenFailed, "Prepare program (shaders) failed.");
            return;
        }
        m_programLoad = true;
    }
}


bool GlView::checkShaderCompileStatus(GLuint obj) const {
    GLint status;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

bool GlView::checkProgramLinkStatus(GLuint obj) const {
    GLint status;
    glGetProgramiv(obj, GL_LINK_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

GLuint GlView::prepareProgram() {

    // WARNING: this is only for testing!
    const GLchar * const vertexShaderSourcePtr =
            "attribute vec4 vPosition;    \n"
            "uniform mat4 mvMatrix;       \n"
            "void main()                  \n"
            "{                            \n"
            "   gl_Position = mvMatrix * vPosition;  \n"
            "}                            \n";

    GLuint vertexShaderId = loadShader(GL_VERTEX_SHADER, vertexShaderSourcePtr);

    if( !vertexShaderId )
        return 0;

    const GLchar * const fragmentShaderSourcePtr =
            "precision mediump float;                     \n"
            "uniform vec4 u_Color;                        \n"
            "void main()                                  \n"
            "{                                            \n"
            "  gl_FragColor = u_Color;                    \n"
            "}                                            \n";

    GLuint fragmentShaderId = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSourcePtr);
    if( !fragmentShaderId )
        return 0;

    GLuint programId = glCreateProgram();
    if ( !programId )
       return 0;

    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glBindAttribLocation ( programId, 0, "vPosition" );
    glLinkProgram(programId);

    if( !checkProgramLinkStatus(programId) )
        return 0;

    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    return programId;
}

GLuint GlView::loadShader ( GLenum type, const char *shaderSrc )
{
   GLuint shader;
   GLint compiled;

   // Create the shader object
   shader = glCreateShader ( type );

   if ( !shader )
    return 0;

   // Load the shader source
   glShaderSource ( shader, 1, &shaderSrc, nullptr );

   // Compile the shader
   glCompileShader ( shader );

   // Check the compile status
   glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

   if (!checkShaderCompileStatus(shader) ) {
      glDeleteShader ( shader );
      return 0;
   }

   return shader;

}

void GlView::loadExtensions()
{
/*  if(!m_extensionLoad){
        const GLubyte *str = glGetString(GL_EXTENSIONS);
        if (str != nullptr) {
            const char* pszList = reinterpret_cast<const char*>(str);
#ifdef _DEBUG
            cout << "GL extensions: " << pszList << endl;
#endif // _DEBUG
            char **papszTokens=CSLTokenizeString2(pszList," ",
                                    CSLT_STRIPLEADSPACES|CSLT_STRIPENDSPACES);
            for (int i = 0; i < CSLCount(papszTokens); ++i) {
                if(EQUAL(papszTokens[i], "GL_ARB_vertex_array_object")){
                    bindVertexArrayFn = reinterpret_cast<int (*)(GLuint)>(
                                eglGetProcAddress("glBindVertexArray"));
                    deleteVertexArraysFn = reinterpret_cast<int (*)(GLsizei, const GLuint*)>(
                                eglGetProcAddress("glDeleteVertexArrays"));
                    genVertexArraysFn = reinterpret_cast<int (*)(GLsizei, GLuint*)>(
                                eglGetProcAddress("glGenVertexArrays"));
                }
                else if(EQUAL(papszTokens[i], "GL_OES_vertex_array_object")){
                    bindVertexArrayFn = reinterpret_cast<int (*)(GLuint)>(
                                eglGetProcAddress("glBindVertexArrayOES"));
                    deleteVertexArraysFn = reinterpret_cast<int (*)(GLsizei, const GLuint*)>(
                                eglGetProcAddress("glDeleteVertexArraysOES"));
                    genVertexArraysFn = reinterpret_cast<int (*)(GLsizei, GLuint*)>(
                                eglGetProcAddress("glGenVertexArraysOES"));
                }
                else if(EQUAL(papszTokens[i], "GL_APPLE_vertex_array_object")){
                    bindVertexArrayFn = reinterpret_cast<int (*)(GLuint)>(
                                eglGetProcAddress("glBindVertexArrayAPPLE"));
                    deleteVertexArraysFn = reinterpret_cast<int (*)(GLsizei, const GLuint*)>(
                                eglGetProcAddress("glDeleteVertexArraysAPPLE"));
                    genVertexArraysFn = reinterpret_cast<int (*)(GLsizei, GLuint*)>(
                                eglGetProcAddress("glGenVertexArraysAPPLE"));
                }

            }
            CSLDestroy(papszTokens);

            m_extensionLoad = true;
        }
    }*/

}

//------------------------------------------------------------------------------
// GlOffScreenView
//------------------------------------------------------------------------------

GlOffScreenView::GlOffScreenView() : GlView(), m_defaultFramebuffer(0)
{
    memset(m_renderbuffers, 0, sizeof(m_renderbuffers));
}

GlOffScreenView::~GlOffScreenView()
{

}

bool GlOffScreenView::createSurface()
{
    if(m_eglSurface != EGL_NO_SURFACE)
        eglDestroySurface( m_glDisplay->eglDisplay (), m_eglSurface );

    destroyFBO ();

    // create a pixelbuffer surface
    // surface attributes
    // the surface size is set to the input frame size
    const EGLint surfaceAttr[] = {
         EGL_WIDTH, m_displayWidth,
         EGL_HEIGHT, m_displayHeight,
         EGL_LARGEST_PBUFFER, EGL_TRUE,
         EGL_NONE
    };

    // NOTE: need to create both pbuffer and FBO to draw into offscreen buffer
    // see: http://stackoverflow.com/q/28817777/2901140

    m_eglSurface = eglCreatePbufferSurface( m_glDisplay->eglDisplay (),
                                            m_glDisplay->eglConf (), surfaceAttr);

#ifdef _DEBUG
    checkEGLError("eglCreatePbufferSurface");
#endif // _DEBUG

    if(EGL_NO_SURFACE != m_eglSurface){

        if(!eglMakeCurrent(m_glDisplay->eglDisplay (), m_eglSurface, m_eglSurface,
                           m_eglCtx)) {
            CPLError(CE_Failure, CPLE_OpenFailed, "eglMakeCurrent failed.");
            return false;
        }

        if(!createFBO (m_displayWidth, m_displayHeight)){
            CPLError(CE_Failure, CPLE_OpenFailed, "createFBO failed.");
            return false;
        }
    }
    else {
        CPLError(CE_Failure, CPLE_OpenFailed, "eglCreatePbufferSurface failed.");
        return false;
    }

    return true;
}

void GlOffScreenView::destroyFBO()
{
    ngsCheckGLEerror(glDeleteFramebuffers(1, &m_defaultFramebuffer));
    ngsCheckGLEerror(glDeleteRenderbuffers(ARRAY_SIZE(m_renderbuffers),
                                           m_renderbuffers));
}

bool GlOffScreenView::createFBO(int width, int height)
{
    //Create the FrameBuffer and binds it
    ngsCheckGLEerror(glGenFramebuffers(1, &m_defaultFramebuffer));
    ngsCheckGLEerror(glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFramebuffer));

    ngsCheckGLEerror(glGenRenderbuffers(ARRAY_SIZE(m_renderbuffers),
                                        m_renderbuffers));

    //Create the RenderBuffer for offscreen rendering // Color
    ngsCheckGLEerror(glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffers[0]));
    ngsCheckGLEerror(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, width,
                                           height));

    //Create the RenderBuffer for offscreen rendering // Depth
    ngsCheckGLEerror(glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffers[1]));
    ngsCheckGLEerror(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                           width, height));

    //Create the RenderBuffer for offscreen rendering // Stencil
    /*glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffers[2]);
    checkGLError("glBindRenderbuffer stencil");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
    checkGLError("glRenderbufferStorage stencil");*/

    // bind renderbuffers to framebuffer object
    ngsCheckGLEerror(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffers[1]));
    ngsCheckGLEerror(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_renderbuffers[0]));
    /*glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffers[2]);
    checkGLError("glFramebufferRenderbuffer stencil");*/

    //Test for FrameBuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    checkGLError("glCheckFramebufferStatus");
    const char* msg = nullptr;
    bool res = false;
    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE:
            msg = "FBO complete  GL_FRAMEBUFFER_COMPLETE";
            res = true;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            msg = "FBO GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            msg = "FBO FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            msg = "FBO FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            msg = "FBO GL_FRAMEBUFFER_UNSUPPORTED";
            break;
        default:
            msg = "Failed to make complete framebuffer object";
        }
#ifdef _DEBUG
    cout << msg << " " << status << endl;
#endif //_DEBUG

    return res;
}

//------------------------------------------------------------------------------
// GlFuctions
//------------------------------------------------------------------------------

GlFuctions::GlFuctions() : m_programId(0), m_extensionLoad(false),
    m_programLoad(false), m_pBkChanged(true)
{
}

bool GlFuctions::init()
{
    if(!loadExtensions ())
        return false;
    if(loadProgram ())
        return false;

    return true;
}

bool GlFuctions::isOk() const
{
    return m_programLoad && m_extensionLoad;
}

void GlFuctions::setBackgroundColor(const ngsRGBA &color)
{
    m_bkColor.r = float(color.R) / 255;
    m_bkColor.g = float(color.G) / 255;
    m_bkColor.b = float(color.B) / 255;
    m_bkColor.a = float(color.A) / 255;

    m_pBkChanged = true;
}

// NOTE: Should be run on current context
void GlFuctions::clearBackground()
{
    if(m_pBkChanged) {
        ngsCheckGLEerror(glClearColor(m_bkColor.r, m_bkColor.g, m_bkColor.b,
                                  m_bkColor.a));
        m_pBkChanged = false;
    }
    ngsCheckGLEerror(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void GlFuctions::prepare(const Matrix4 &mat)
{
#ifdef _DEBUG
    GLint numActiveUniforms = 0;
    glGetProgramiv(m_programId, GL_ACTIVE_UNIFORMS, &numActiveUniforms);
    cout << "Number active uniforms: " << numActiveUniforms << endl;
#endif //_DEBUG
    GLint location = glGetUniformLocation(m_programId, "mvMatrix");

    array<GLfloat, 16> mat4f = mat.dataF ();

    ngsCheckGLEerror(glUniformMatrix4fv(location, 1, GL_FALSE, mat4f.data()));

}

void GlFuctions::testDrawPreserved() const
{
    static bool isBuffersFilled = false;
    static GLuint    buffers[2];
    static GLint uColorLocation = -1;
    if(!isBuffersFilled) {
        isBuffersFilled = true;
        uColorLocation = glGetUniformLocation(m_programId, "u_Color");
        ngsCheckGLEerror(glGenBuffers(2, buffers)); // TODO: glDeleteBuffers

        ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));

        GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                               -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                                4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                              };

        ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices), vVertices, GL_STATIC_DRAW));

        ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));

        GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                               -2236992.0f, 3972353.0f, 0.5f,
                                5187591.0f, 4509961.0f, 0.5f
                              };

        ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices2), vVertices2, GL_STATIC_DRAW));
    }

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));

    ngsCheckGLEerror(glUniform4f(uColorLocation, 1.0f, 0.0f, 0.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));

    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));

    ngsCheckGLEerror(glUniform4f(uColorLocation, 0.0f, 0.0f, 1.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));

    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

}

void GlFuctions::testDraw() const
{
    int uColorLocation = glGetUniformLocation(m_programId, "u_Color");

    GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                          };

    ngsCheckGLEerror(glUniform4f(uColorLocation, 1.0f, 0.0f, 0.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                           -2236992.0f, 3972353.0f, 0.5f,
                            5187591.0f, 4509961.0f, 0.5f
                          };

    ngsCheckGLEerror(glUniform4f(uColorLocation, 0.0f, 0.0f, 1.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices2 ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));
}

void GlFuctions::drawPolygons(const vector<GLfloat> &vertices,
                              const vector<GLushort> &indices) const
{
    if(vertices.empty() || indices.empty ())
        return;
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));
    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                            vertices.data ()));
    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, indices.size (),
                                    GL_UNSIGNED_SHORT, indices.data ()));
}

bool GlFuctions::loadProgram()
{
    if(!m_programLoad) {
        m_programId = prepareProgram();
        if(!m_programId) {
            CPLError(CE_Failure, CPLE_OpenFailed, "Prepare program (shaders) failed.");
            return false;
        }
        m_programLoad = true;
        ngsCheckGLEerror(glUseProgram(m_programId));
    }

    return true;
}

GLuint GlFuctions::prepareProgram()
{
    // WARNING: this is only for testing!
    const GLchar * const vertexShaderSourcePtr =
            "attribute vec4 vPosition;    \n"
            "uniform mat4 mvMatrix;       \n"
            "void main()                  \n"
            "{                            \n"
            "   gl_Position = mvMatrix * vPosition;  \n"
            "}                            \n";

    GLuint vertexShaderId = loadShader(GL_VERTEX_SHADER, vertexShaderSourcePtr);

    if( !vertexShaderId )
        return 0;

    const GLchar * const fragmentShaderSourcePtr =
            "precision mediump float;                     \n"
            "uniform vec4 u_Color;                        \n"
            "void main()                                  \n"
            "{                                            \n"
            "  gl_FragColor = u_Color;                    \n"
            "}                                            \n";

    GLuint fragmentShaderId = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSourcePtr);
    if( !fragmentShaderId )
        return 0;

    GLuint programId = glCreateProgram();
    if ( !programId )
       return 0;

    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glBindAttribLocation ( programId, 0, "vPosition" );
    glLinkProgram(programId);

    if( !checkProgramLinkStatus(programId) )
        return 0;

    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    return programId;
}

bool GlFuctions::checkProgramLinkStatus(GLuint obj) const
{
    GLint status;
    glGetProgramiv(obj, GL_LINK_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

bool GlFuctions::checkShaderCompileStatus(GLuint obj) const
{
    GLint status;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        reportGlStatus(obj);
        return false;
    }
    return true;
}

GLuint GlFuctions::loadShader(GLenum type, const char *shaderSrc)
{
    GLuint shader;
    GLint compiled;

    // Create the shader object
    shader = glCreateShader ( type );

    if ( !shader )
     return 0;

    // Load the shader source
    glShaderSource ( shader, 1, &shaderSrc, nullptr );

    // Compile the shader
    glCompileShader ( shader );

    // Check the compile status
    glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

    if (!checkShaderCompileStatus(shader) ) {
       glDeleteShader ( shader );
       return 0;
    }

    return shader;
}

bool GlFuctions::loadExtensions()
{
    m_extensionLoad = true;
    return true;
}
