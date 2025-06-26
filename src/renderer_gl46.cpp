//
// Created by mgrus on 26.06.2025.
//



#ifdef RENDERER_GL46
#include <cassert>
#include <graphics.h>
#include <iostream>
#include <map>
#include <vector>
#include <../include/GL/glew.h>
#include <../include/GL/wglew.h>

static int nextHandleId = 0;
std::map<int, GLuint> programMap;
std::map<int, GLuint> shaderMap;
std::map<int, GLuint> vertexBufferMap;
std::map<int, GLuint> indexBufferMap;
std::map<int, GLuint> vaoMap;

static GLuint createFragmentShader(const std::string& code) {
    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* fssource_char = code.c_str();
    glShaderSource(fshader, 1, &fssource_char, NULL);
    glCompileShader(fshader);

    GLint compileStatus;
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &compileStatus);
    if (GL_FALSE == compileStatus) {
        GLint logSize = 0;
        glGetShaderiv(fshader, GL_INFO_LOG_LENGTH, &logSize);
        std::vector<GLchar> errorLog(logSize);
        glGetShaderInfoLog(fshader, logSize, &logSize, &errorLog[0]);
        //   result.errorMessage = errorLog.data();
        printf("fragment shader error: %s", errorLog.data());
        glDeleteShader(fshader);

    }

    return fshader;


}

static GLuint createVertexShader(const std::string & code) {
    auto vshader = glCreateShader(GL_VERTEX_SHADER);
    const GLchar* vssource_char = code.c_str();
    glShaderSource(vshader, 1, &vssource_char, NULL);
    glCompileShader(vshader);
    GLint compileStatus;
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &compileStatus);
    if (GL_FALSE == compileStatus) {
        std::cerr << "Error while compiling the vertex shader" << std::endl;
        GLint logSize = 0;
        glGetShaderiv(vshader, GL_INFO_LOG_LENGTH, &logSize);
        std::vector<GLchar> errorLog(logSize);
        glGetShaderInfoLog(vshader, logSize, &logSize, &errorLog[0]);
        //    result.errorMessage = errorLog.data();
        char buf[512];
        std::cerr << "vshader error: " <<  errorLog.data() << std::endl;
        glDeleteShader(vshader);
        return 0;

    }

    return vshader;
}

GraphicsHandle createShader(const std::string& code, ShaderType type) {
    GLuint shader = 0;
    if (type == ShaderType::Vertex) {
        shader = createVertexShader(code);
    }
    else if (type == ShaderType::Fragment) {
        shader = createFragmentShader(code);

    }
    else if (type == ShaderType::Geometry) {}


    GraphicsHandle handle = {nextHandleId++};
    shaderMap[handle.id] = shader;
    return handle;

}

void bindShaderProgram(GraphicsHandle programHandle) {
    glUseProgram(programMap[programHandle.id]);
}
GraphicsHandle createShaderProgram(const std::string& vsCode, const std::string& fsCode) {
    auto vs = createVertexShader(vsCode);
    auto fs = createFragmentShader(fsCode);
    auto prog = glCreateProgram();

    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint linkStatus;
    glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);

    if (GL_FALSE == linkStatus) {
        std::cerr << "Error during shader linking" << std::endl;
        GLint maxLength = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(prog, maxLength, &maxLength, &infoLog[0]);
        std::cerr << infoLog.data() << std::endl;
        printf("shader linking error: %s", infoLog.data());
        glDeleteProgram(prog);
    }

    GraphicsHandle handle  = {nextHandleId++};
    programMap[handle.id] = prog;
    return handle;
}

GLenum getGlTypeForPrimitiveType(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::Float:
            return GL_FLOAT;
        case PrimitiveType::Int32:
            return GL_INT;
        case PrimitiveType::Uint32:
            return GL_UNSIGNED_INT;
        case PrimitiveType::Int16:
            return GL_SHORT;
        case PrimitiveType::Uint16:
            return GL_UNSIGNED_SHORT;
        default: return GL_FLOAT;
    }
}

void associateVertexAttribute(int attributeLocation, int numberOfComponents, PrimitiveType type, int stride, int offset,
    GraphicsHandle bufferHandle, GraphicsHandle shaderProgramHandle, GraphicsHandle vaoHandle) {
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferMap[bufferHandle.id]);
    glVertexAttribPointer(attributeLocation, numberOfComponents, getGlTypeForPrimitiveType(type),
        GL_FALSE, stride, (void*)offset);
    glEnableVertexAttribArray(attributeLocation);
}

GraphicsHandle createVertexBuffer(void* data, int size) {
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    auto err = glGetError();
    assert(err == GL_NO_ERROR);

    GraphicsHandle handle = {nextHandleId++};
    vertexBufferMap[handle.id] = vbo;
    return handle;
}

void bindVertexArray(GraphicsHandle vaoHandle) {
    glBindVertexArray(vaoMap[vaoHandle.id]);

}

GraphicsHandle createVertexArray() {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    GraphicsHandle handle = {nextHandleId++};
    vaoMap[handle.id] = vao;
    return handle;
}

void assertNoGlError() {
    auto err = glGetError();
    assert(err == GL_NO_ERROR);
}

void bindVertexBuffer(GraphicsHandle bufferHandle) {
    // For the case of opengl we actually want to bind the vertex array object.(vao)
    // We will need to do a bit of mapping here.
    auto vao = vaoMap[bufferHandle.id];
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferMap[bufferHandle.id]);
    assertNoGlError();
}


GLenum getGLPrimitiveType(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::TRIANGLE_LIST: return GL_TRIANGLES;
        default: return GL_TRIANGLES;
    }
}

void renderGeometry(PrimitiveType type) {

    glDrawArrays(getPrimitiveType(type), 0, 3);
    assertNoGlError();
}


void initGraphics(Win32Window& window, bool msaa, int msaa_samples) {

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
        PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
        32,                   // Color depth of the framebuffer.
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,                   // Number of bits for the depthbuffer
        8,                    // Number of bits for the stencilbuffer
        0,                    // Number of Aux buffers in the framebuffer.
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };



    // 1. Register a dummy window class
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_OWNDC;                  // get our own DC for the window
    wc.lpfnWndProc   = DefWindowProcA;            // a trivial wndproc
    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpszClassName = "Dummy_WGL";

    if (!RegisterClassA(&wc)) {
        // handle error
    }

    // 2. Create the hidden (dummy) window
    HWND dummyWnd = CreateWindowA(
        "Dummy_WGL",            // class name
        "Dummy OpenGL Window",  // window name
        WS_OVERLAPPEDWINDOW,    // style
        CW_USEDEFAULT, CW_USEDEFAULT, // position
        CW_USEDEFAULT, CW_USEDEFAULT, // size
        NULL,                   // parent
        NULL,                   // menu
        wc.hInstance,
        NULL
    );
    if (!dummyWnd) {
        // TODO handle error
        exit(999);
    }

    // 3. Get the DC for the dummy window
    HDC dummyDC = GetDC(dummyWnd);

    int windowsChosenFormat = ChoosePixelFormat(dummyDC, &pfd);
    SetPixelFormat(dummyDC, windowsChosenFormat, &pfd);

    HGLRC baseContext = wglCreateContext(dummyDC);
    BOOL ok = wglMakeCurrent (dummyDC, baseContext);
    if (!ok) {
        printf("error");
        exit(1);
    }

    int gl46_attribs[] =
            {
                    WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
                    WGL_CONTEXT_MINOR_VERSION_ARB, 6,
                    WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                    0,
            };


    //HGLRC wglCreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int *attribList)
    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
    auto createContextAttribsProc = (PFNWGLCREATECONTEXTATTRIBSARBPROC ) wglGetProcAddress("wglCreateContextAttribsARB");

    int pixelAttribs[] =
    {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
        WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,     32,
        WGL_DEPTH_BITS_ARB,     24,
        WGL_STENCIL_BITS_ARB,   8,
        WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
        WGL_SAMPLE_BUFFERS_ARB, msaa ? 1 : 0,
        WGL_SAMPLES_ARB,        msaa_samples,
        0
    };

    int pixelFormat;
    UINT numFormats; // will receive how many formats match

    auto hwnd = window.get_hwnd();
    // Find a pixel format that matches our criteria
    HDC hdc_ = GetDC(hwnd);
    bool success = wglChoosePixelFormatARB(hdc_, pixelAttribs, nullptr, 1,
                                           &pixelFormat, &numFormats);

    if ( ! success || numFormats ==0 ) {
        // TODO handle error, no throw in dll exports.
        //throw std::runtime_error("Failed to create OpenGL context with MSAA 4x");
    }

    HDC ourWindowHandleToDeviceContext = GetDC(hwnd);
    SetPixelFormat(ourWindowHandleToDeviceContext, pixelFormat, &pfd);

    auto coreRenderContext = createContextAttribsProc(hdc_, nullptr, gl46_attribs);
    wglDeleteContext(baseContext);
    ok = wglMakeCurrent(ourWindowHandleToDeviceContext, coreRenderContext);
    if (!ok) {
        printf("error");
        exit(1);
    }

    // Initialize GLEW
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        // GLEW initialization failed
        fprintf(stderr, "Error initializing GLEW: %s\n", glewGetErrorString(err));
        exit(1);
    }


    const GLubyte *GLVersionString = glGetString(GL_VERSION);
    char buf[200];
    sprintf_s(buf, "gl version: %s\n", GLVersionString);
    OutputDebugStringA(buf);
    printf("%s", buf);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    if (msaa > 0) {
        glEnable(GL_MULTISAMPLE);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST );
        glEnable(GL_LINE_SMOOTH);

        GLint samples = 0, sampleBuffers = 0;
        glGetIntegerv(GL_SAMPLES, &samples);
        glGetIntegerv(GL_SAMPLE_BUFFERS, &sampleBuffers);
    }

    glEnable(GL_FRAMEBUFFER_SRGB);

    RECT r;
    GetClientRect(hwnd, &r);
    glViewport(0, 0, r.right-r.left, r.bottom-r.top);

    auto enableVsync = [](bool enable) {
        ((BOOL(WINAPI*)(int))wglGetProcAddress("wglSwapIntervalEXT"))(enable);
    };
    enableVsync(false);

}

void clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void present() {
    SwapBuffers(GetDC(GetActiveWindow()));
}

#endif