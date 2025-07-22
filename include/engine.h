//
// Created by mgrus on 25.06.2025.
//

#define NOMINMAX
#include <assimp/material.h>
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <deque>
#include <functional>
#include <stb_truetype.h>
#include <vector>
#include <map>
#include <Win32Window.h>
#include <glm/glm.hpp>
#include <assimp/vector3.h>
#include <assimp/quaternion.h>
#include <assimp/anim.h>

struct Animation;
struct JointAnimationTrack;
struct Mesh;

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
    glm::mat4 poseLocalTransform;
    glm::mat4 poseGlobalTransform;
    glm::mat4 poseFinalTransform;
};

struct Skeleton {
    std::vector<Joint*> joints;
    std::vector<Animation*> animations;
};

enum class KeyFrameType {
    Translation,
    Rotation,
    Scale,
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

    // Material data
    std::string materialName;
    std::string diffuseTexturePath; // needed for external textures
    std::string normalMapPath;
    GraphicsHandle diffuseTexture = {-1};  // already the full texture created in memory if it was embedded in the gltf file.

    // Skeleton/joints data:
    Skeleton * skeleton = nullptr;
    std::vector<glm::vec4> jointIndices;
    std::vector<glm::vec4> jointWeights;



    Mesh* toMesh();
};

struct Font {
    GraphicsHandle atlasTexture;
    float maxDescent = std::numeric_limits<float>::max();
    float lineHeight = std::numeric_limits<float>::min();
    float baseLine = 0.0f;
    std::vector<stbtt_bakedchar> bakedChars;

};

enum class BufferUsage {
    Static,
    Dynamic,

};

enum class BufferType {
    Vertex,
    Index,
    Constant,
};

/**
 * As each Mesh can (and often does)
 * consist of multiple meshes, one per material,
 * we store meshes which are logically belonging together
 * (e.g. a character which consists of 3 body parts due to
 * 3 different materials).
 *
 */
struct MeshGroup {
    std::vector<Mesh*> meshes;
    std::string name;
};

struct BoundingBox {
    float left;
    float top;
    float right;
    float bottom;
};

struct TabHeader {
    std::string title;
    Mesh* titleTextMesh;
};

enum class TabType {
    Model,
    Level,
    Texture,
    Script
};

struct Level;
struct Tab {
    TabHeader tabHeader;
    TabType type;

    // For levels:
    Level* level;

    // For models
    MeshGroup* meshGroup = nullptr;

    // For textures
    GraphicsHandle texture;

    // For scripts
    // TODO this is just temporary,
    // we don't know the real structure of a "script" yet.
    std::string script;
    BoundingBox renderBoundingBox;
};

struct DropDownItem;
struct EditorState;
struct MenuItem {
    std::string name;
    Mesh* textMesh = nullptr;
    BoundingBox renderBoundingBox = {};
    std::function<void(EditorState&)> action;
    DropDownItem* dropDownMenu = nullptr;

};

struct Mesh {
    GraphicsHandle transformBuffer;
    GraphicsHandle meshVertexBuffer;
    GraphicsHandle meshIndexBuffer;
    GraphicsHandle meshVertexArray;
    GraphicsHandle thumbnail = {-1};
    GraphicsHandle diffuseTexture = {-1};
    GraphicsHandle normalTexture;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec4> jointIndices;
    std::vector<glm::vec4> jointWeights;
    uint32_t creation_index_count;
    uint32_t index_count;
    Skeleton* skeleton = nullptr;
    std::string name;
    std::string materialName;
    std::string diffuseTexturePath;
    std::string normalMapPath;

};

enum class FillMode {
    Solid,
    Wireframe,
};



struct aiVectorKey;
struct aiQuatKey;
struct JointAnimationTrack {
    std::vector<aiVectorKey> positionKeys;
    std::vector<aiQuatKey> rotationKeys;
    std::vector<aiVectorKey> scaleKeys;
};

struct Animation {
    std::string name;
    float duration = -1;
    std::map<std::string, JointAnimationTrack> jointAnimationTracks;
    double ticksPerSecond;
};

struct StartEndKeyFrame {
    std::pair<aiVectorKey, aiVectorKey> posKeys;
    std::pair<aiQuatKey, aiQuatKey> rotKeys;
    std::pair<aiVectorKey, aiVectorKey> scaleKeys;
};

struct alignas(16) Camera {
    glm::vec3 getForward();
    glm::vec3 getRight();
    glm::vec3 getUp();
    glm::mat4 updateAndGetViewMatrix();
    glm::mat4 updateAndGetPerspectiveProjectionMatrix(float fovInDegrees, int width, int height, float nearPlane, float farPlane);
    glm::mat4 getOrthoProjectionMatrix();

    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;
    glm::vec3 location;
    glm::vec3 lookAtTarget;
    glm::vec3 _initialForward;

    glm::vec3 followOffset;
    glm::vec3 followDirection;
    float nearPlane = 0.1f;
    float farPlane = 500.0f;

    // GLuint positionBuffer = 0;
    // GLuint shadowPositionBuffer = 0;
    // Shader* frustumShader = nullptr;
    // CameraType type_;

    float left_ = -1;
    float right_= -1;
    float bottom_ = -1;
    float top_ = -1;
    void * matrixBufferPtr();
};



/**
 * This class calculates the average frametime out of a number of samples
 * to avoid too much jitter in the different times, which may vary frame by frame if uncapped.
 */
class FrameTimer {

public:
    FrameTimer(size_t maxSamples);
    void addFrameTime(float dt);

    float getAverage() const;

private:
    std::deque<float> frameTimes;
    const size_t maxSamples_ = 60;
    float sum = 0.0f;
};


struct EditorState;
GraphicsHandle createFont(const std::string& fontName, int fontSize);
GraphicsHandle createTexture(int width, int height, uint8_t* pixels, uint8_t num_channels);
GraphicsHandle createTextureFromFile(const std::string& diffuseTexturePath);
GraphicsHandle createThumbnailForMesh(std::vector<MeshData*> meshDataItems, GraphicsHandle shaderProgram, GraphicsHandle objectTransformBuffer, GraphicsHandle cameraTransformBuffer, std::vector<VertexAttributeDescription> vertexAttributes, int width, int height);
GraphicsHandle createShader(const std::string& code, ShaderType type);
GraphicsHandle createShaderProgram(const std::string& vsCode, const std::string& fsCode);
GraphicsHandle createVertexBuffer(void* data, int size, uint32_t stride=0, BufferUsage usage = BufferUsage::Static);
GraphicsHandle createConstantBuffer(uint32_t size);
GraphicsHandle createIndexBuffer(void* data, int size, BufferUsage usage = BufferUsage::Static);
GraphicsHandle createVertexArray();
GraphicsHandle getTextureFromFont(GraphicsHandle fontHandle);
GraphicsHandle getTextureFromFrameBuffer(GraphicsHandle frameBufferHandle);
GraphicsHandle getDefaultTextShaderProgram();
GraphicsHandle createFrameBuffer(int width, int height, bool includeDepthBuffer = false);
StartEndKeyFrame getStartEndKeyFrameForTime(float time, Animation* animation, KeyFrameType type, std::string jointName);
glm::vec3 aiToGLM(aiVector3D);
glm::quat aiToGLM(aiQuaternion aiQuat);
Mesh* createTextMesh(GraphicsHandle fontHandle, const std::string& text);
MeshData* renderTextIntoQuadGeometry(GraphicsHandle fontHandle, const std::string& text);
std::vector<MeshData*> importMeshFromFile(const std::string& fileName);
std::wstring showFileDialog(const std::wstring& typeFilter);
BoundingBox measureText(GraphicsHandle fontHandle, const std::string& text);
GraphicsHandle beginRenderAnnotation(std::wstring name);
std::string fileNameFromPath(const std::string& filePath);
float findWidestMenuItemSize(std::vector<MenuItem*>& menuItems, GraphicsHandle fontHandle);

void initGraphics(Win32Window& window, bool msaa, int msaa_samples);

void clear(float r, float g, float b, float a);
void clearFrameBuffer(GraphicsHandle fbHandle, float r, float g, float b, float a);
void describeVertexAttributes(std::vector<VertexAttributeDescription>& attributeDescriptions,  GraphicsHandle bufferHandle, GraphicsHandle shaderProgramHandle, GraphicsHandle vertexArrayHandle);
void associateVertexBufferWithVertexArray(GraphicsHandle vertexBuffer, GraphicsHandle vertexArray);
void associateIndexBufferWithVertexArray(GraphicsHandle indexBuffer, GraphicsHandle vertexArray);
void bindVertexBuffer(GraphicsHandle bufferHandle);
void bindVertexArray(GraphicsHandle vaoHandle);
void bindShaderProgram(GraphicsHandle programHandle);
void bindTexture(GraphicsHandle textureHandle, uint8_t slot);
void bindDefaultBackbuffer(int originX, int originY, int width, int height);
void bindFrameBuffer(GraphicsHandle fbHandle, int viewportOriginX, int viewportOriginY, int viewportWidth, int viewportHeight);
void bindFrameBufferTexture(GraphicsHandle frameBufferHandle, int slot);
void uploadConstantBufferData(GraphicsHandle bufferHandle, void* data, uint32_t size_in_bytes, uint32_t bufferSlot);
void renderGeometry(PrimitiveType primitiveType);
void updateText(Mesh& textMesh, GraphicsHandle fontHandle, const std::string& text);
void updateBuffer(GraphicsHandle bufferHandle, BufferType bufferType, void* data, uint32_t size_in_bytes);
void resizeSwapChain(HWND hwnd, int width, int height);
void resizeFrameBuffer(GraphicsHandle fbHandle, int width, int height);
void copyTexture(GraphicsHandle targetTexture, GraphicsHandle sourceTexture);
void setFillMode(FillMode mode);
void setFrontCulling(bool front);
void setDepthTesting(bool on);
void enableBlending(bool enable);
void endRenderAnnotation(GraphicsHandle annotationHandle);
void setViewport(int originX, int originY, int width, int height);
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

int mouseX();
int mouseY();
bool mouseLeftDoubleClick();
bool mouseLeftClick();
bool mouseRightClick();
void mouseLeftClickConsumed();
bool isKeyDown(int key);
bool keyPressed(int key);
int resizedWidth();
int resizedHeight();

#include <Windows.h>






#endif //GRAPHICS_H
