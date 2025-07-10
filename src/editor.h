//
// Created by mgrus on 26.06.2025.
//

#ifndef GAME_H
#define GAME_H
#include <map>

#include "glm/vec3.hpp"
#include <glm/glm.hpp>


struct GameObject {

    struct TransformData {
        glm::vec3 position;
        glm::vec3 orientation;
        glm::vec3 scale;
    } transform;

    struct RenderData {
        GraphicsHandle transformBuffer;
        GraphicsHandle materialBuffer;
        GraphicsHandle meshVertexBuffer;
        GraphicsHandle meshIndexBuffer;
        GraphicsHandle meshVertexArray;
        GraphicsHandle meshTextureHandle;
        GraphicsHandle meshShaderProgram;
        MeshGroup* meshGroup;
    } renderData;

    GraphicsHandle diffuseTexture = {-1};

    std::vector<GameObject*> children;
    std::string name;

    BoundingBox treeBoundingBox;

    bool expandedInTree = false;
};

struct alignas(16) ObjectTransformBuffer {
    glm::mat4 world_matrix;
};





struct EditorState {

    std::map<std::string, MeshGroup*> meshPool;
    std::map<std::string, GraphicsHandle> texturePool;

    struct MenuTextMeshes {
        Mesh* tmFile;
        Mesh* tmGameObjects;
        Mesh* tmSettings;
        Mesh* tmModelImport;
    } menuTextMeshes;

    struct GraphicsState {
        GraphicsHandle shaderProgram;
        GraphicsHandle textShaderProgram;
        GraphicsHandle animatedShaderProgram;
        GraphicsHandle quadVertexBuffer;
        GraphicsHandle quadIndexBuffer;
        GraphicsHandle quadVertexArray;
        GraphicsHandle textureHandle;
        GraphicsHandle cameraTransformBuffer;
        GraphicsHandle objectTransformBuffer;
        GraphicsHandle skinningMatricesCBuffer;
        GraphicsHandle fontHandle;
        GraphicsHandle frameBuffer3DPanel;
        GraphicsHandle frameBufferMainTabPanel;
        GraphicsHandle frameBufferTopMenu;
        GraphicsHandle frameBufferAssetPanel;
        GraphicsHandle frameBufferSceneTree;
        GraphicsHandle frameBufferStatusBar;
        GraphicsHandle frameBufferThumbnail;
        GraphicsHandle shaderProgram3D;

        GraphicsHandle jointDebugTexture;
        Mesh* jointDebugMesh = nullptr;

    } graphics;

    std::vector<MeshGroup*> importedMeshGroups;
    std::vector<GameObject*> gameObjects;

    // Cameras
    Camera * perspectiveCamera = nullptr;
    Camera * orthoCamera = nullptr;

    // Text rendering (dummy/temp data for now)
    size_t textIndexCount;
    GraphicsHandle textVertexArray;
    GraphicsHandle textVertexBuffer;
    GraphicsHandle textIndexBuffer;
    uint16_t screen_width;
    uint16_t screen_height;
    Mesh * textMesh = nullptr;      // temporary to hold a test text mesh, for text render tests.
    FrameTimer* frameTimer = nullptr;
    std::vector<VertexAttributeDescription> vertexAttributesAnimated;
    std::vector<VertexAttributeDescription> vertexAttributes;
    std::vector<Tab*> mainTabs;
    std::string currentMainTabTitle;
    int hoveredAssetIndex = -1;
    std::string currentHoverTabTitle;
    GameObject * rootGameObject = {};
    std::vector<GameObject*> visibleGameObjectTreeItems;
    GameObject* currentHoverExpandItem;
};

#endif //GAME_H
