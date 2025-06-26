//
// Created by mgrus on 25.06.2025.
//

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <Win32Window.h>

struct GraphicsHandle {
    int id;
};

enum class ShaderType {
    Vertex,
    Fragment,
    Geometry,
    Tesselation,
};

enum class PrimitiveType {
    TRIANGLE_LIST,
    LINE_LIST,
    POINT_LIST
};

enum class DataType {
    Float,
    Int32,
    Int16,
    Int8,
    Uint32,
    Uint16,
    Uint8,
    Bool,
    FloatVec2,
    FloatVec3,
    FloatVec4,
};

GraphicsHandle createTexture(int width, int height, uint8_t* pixels);
GraphicsHandle createShader(const std::string& code, ShaderType type);
GraphicsHandle createShaderProgram(const std::string& vsCode, const std::string& fsCode);
GraphicsHandle createVertexBuffer(void* data, int size, uint32_t stride=0);
GraphicsHandle createIndexBuffer(void* data, int size);
GraphicsHandle createVertexArray();
void initGraphics(Win32Window& window, bool msaa, int msaa_samples);
void clear(float r, float g, float b, float a);
void associateVertexAttribute(uint32_t attributeLocation, int numberOfComponents, DataType type, int stride, int offset,
        GraphicsHandle bufferHandle, GraphicsHandle shaderProgramHandle, GraphicsHandle vertexArrayHandle);
void associateVertexBufferWithVertexArray(GraphicsHandle vertexBuffer, GraphicsHandle vertexArray);
void associateIndexBufferWithVertexArray(GraphicsHandle indexBuffer, GraphicsHandle vertexArray);
void bindVertexBuffer(GraphicsHandle bufferHandle);
void bindVertexArray(GraphicsHandle vaoHandle);
void bindShaderProgram(GraphicsHandle programHandle);
void renderGeometry(PrimitiveType primitiveType);
void renderGeometryIndexed(PrimitiveType primitiveType, int count, int startIndex);
void present();


// TODO is this really part of the common graphics interface?!
// Maybe yes, we could prepare a background texture to allow single pixel setting even
// in GPU scenarios. lets keep it for now.
void setPixel(Win32Window& window, int x, int y, int color);
void drawLine(Win32Window& window, int x1, int y1, int x2, int y2, int color);

// Render specific prototypes
void gl_clear(float r, float g, float b, float a);
void gl_init(HWND hwnd, bool msaa, int msaa_samples);

// This definition turns on implementation in general.
// This should be defined in one single CPP file only!

#include <Windows.h>






#endif //GRAPHICS_H
