//
// Created by mgrus on 25.06.2025.
//

#define NOMINMAX
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stb_truetype.h>
#include <vector>
#include <Win32Window.h>
#include <glm/glm.hpp>

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

struct VertexAttributeDescription {
    std::string semanticName;       // "needed" only by DX11
    uint32_t attributeLocation;
    int numberOfComponents;     // single float, or a vec2, vec3 for positions, normals?!
    DataType type;              //
    int stride;
    uint32_t offset;

};

struct Joint {
    std::string name;
    glm::mat4 inverseBindMatrix;
    glm::mat4 localTransform;
    Joint* parent = nullptr;
};

struct Skeleton {
    std::vector<Joint*> joints;
};

// MeshData is a render backend agnostic representation of
// vertex data for a mesh.
// It includes positions, uvs, normals, indices, tangents
// and joint binding data.
struct MeshData {
    std::string meshName;
    std::vector<glm::vec3> posMasterList;
    std::vector<glm::vec3> posIndexSortedMasterList;
    std::vector<glm::vec2> uvMasterList;
    std::vector<glm::vec3> tangentMasterList;
    std::vector<glm::vec3> normalMasterList;

    std::vector<float> posFlat;
    std::vector<float> normFlat;
    std::vector<float> uvsFlat;
    std::vector<float> tangentsFlat;
    std::vector<uint32_t> indicesFlat;

    uint32_t stride;    // The size in bytes of 1 vertex

    // Optional
    Skeleton * skeleton = nullptr;
};

struct Font {
    GraphicsHandle atlasTexture;
    float maxDescent = std::numeric_limits<float>::max();
    float lineHeight = std::numeric_limits<float>::min();
    float baseLine = 0.0f;
    std::vector<stbtt_bakedchar> bakedChars;

};

GraphicsHandle createFont(const std::string& fontName, int fontSize);
GraphicsHandle createTexture(int width, int height, uint8_t* pixels, uint8_t num_channels);
GraphicsHandle createShader(const std::string& code, ShaderType type);
GraphicsHandle createShaderProgram(const std::string& vsCode, const std::string& fsCode);
GraphicsHandle createVertexBuffer(void* data, int size, uint32_t stride=0);
GraphicsHandle createConstantBuffer(uint32_t size);
GraphicsHandle createIndexBuffer(void* data, int size);
GraphicsHandle createVertexArray();
GraphicsHandle getTextureFromFont(GraphicsHandle fontHandle);
void initGraphics(Win32Window& window, bool msaa, int msaa_samples);
void clear(float r, float g, float b, float a);
void describeVertexAttributes(std::vector<VertexAttributeDescription>& attributeDescriptions,  GraphicsHandle bufferHandle, GraphicsHandle shaderProgramHandle, GraphicsHandle vertexArrayHandle);
void associateVertexBufferWithVertexArray(GraphicsHandle vertexBuffer, GraphicsHandle vertexArray);
void associateIndexBufferWithVertexArray(GraphicsHandle indexBuffer, GraphicsHandle vertexArray);
void bindVertexBuffer(GraphicsHandle bufferHandle);
void bindVertexArray(GraphicsHandle vaoHandle);
void bindShaderProgram(GraphicsHandle programHandle);
void bindTexture(GraphicsHandle textureHandle, uint8_t slot);
void uploadConstantBufferData(GraphicsHandle bufferHandle, void* data, uint32_t size_in_bytes, uint32_t bufferSlot);
void renderGeometry(PrimitiveType primitiveType);
MeshData* renderTextIntoQuadGeometry(GraphicsHandle fontHandle, const std::string& text);

void renderGeometryIndexed(PrimitiveType primitiveType, int count, int startIndex);
void present();

std::vector<MeshData*> importMeshFromFile(const std::string& fileName);


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
