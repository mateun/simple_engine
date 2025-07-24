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
    GraphicsHandle shaderProgram = { -1};

    std::vector<GameObject*> children;
    std::string name;

    BoundingBox treeBoundingBox;

    bool expandedInTree = false;

    DropDownItem* dropDownMenu = nullptr;

};

struct Level {
    std::string name;
    std::vector<GameObject*> gameObjects;

};

struct Project {
    std::string name;
    std::vector<Level*> levels;
};


struct alignas(16) ObjectTransformBuffer {
    glm::mat4 world_matrix;
};

struct DropDownItem {
    BoundingBox renderBoundingBox;
    BoundingBox globalMouseBoundingBox;
    std::vector<MenuItem*> menuItems;
    bool locked = false;
};

struct EditorState {

    Win32Window* window;
    std::map<std::string, MeshGroup*> meshGroupPool;
    std::map<std::string, GraphicsHandle> texturePool;
    std::map<std::string, Camera*> tabCameraMap;

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

        GraphicsHandle meshEditorGridVertexArray;
        GraphicsHandle meshEditorGridVertexBuffer;
        GraphicsHandle meshEditorGridIndexBuffer;

        GraphicsHandle levelEditorGridVertexArray;
        GraphicsHandle levelEditorGridVertexBuffer;
        GraphicsHandle levelEditorGridIndexBuffer;


        GraphicsHandle textureHandle;
        GraphicsHandle cameraTransformBuffer;
        GraphicsHandle objectTransformBuffer;
        GraphicsHandle skinningMatricesCBuffer;
        GraphicsHandle fontHandle;
        GraphicsHandle fontHandleBig;
        GraphicsHandle frameBuffer3DPanel;
        GraphicsHandle frameBufferMainTabPanel;
        GraphicsHandle frameBufferTopMenu;
        GraphicsHandle frameBufferAssetPanel;
        GraphicsHandle frameBufferGameObjectTree;
        GraphicsHandle frameBufferStatusBar;
        GraphicsHandle frameBufferThumbnail;
        GraphicsHandle frameBufferAnimationPanel;
        GraphicsHandle frameBufferLevelEditorPanel;
        GraphicsHandle frameBufferDropDownOverlayPanel;
        GraphicsHandle shaderProgram3D;
        GraphicsHandle lineShaderProgram;

        GraphicsHandle jointDebugTexture;
        Mesh* jointDebugMesh = nullptr;

        std::vector<VertexAttributeDescription> posUVVertexAttributes;
        std::vector<VertexAttributeDescription> positionOnlyVertexAttributes;
        std::vector<VertexAttributeDescription> animatedVertexAttributes;

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
    std::vector<MenuItem*> topMenuItems;
    std::string currentMainTabTitle;
    float assetVBrowserVScrollOffset = 0;
    int assetVBrowserVScrollPosition = 0;
    float assetBrowserVScrollThumbHeight = 0;
    int hoveredAssetIndex = -1;
    int hoveredGameObjectTreeItemIndex = -1;
    std::string currentHoverTabTitle;
    GameObject * rootGameObject = {};
    std::vector<GameObject*> visibleGameObjectTreeItems;
    std::vector<GameObject*> visibleGameObjectsWithChildrenTreeItems;
    GameObject* currentHoverExpandItem;
    GameObject* currentSelectedGameObjectTreeItem = nullptr;
    MenuItem* currentHoverMenuItem = nullptr;
    MenuItem * currentHoverDropDownMenuItem = nullptr;
    int meshEditorGridLines = 23;
    int levelEditorGridLines = 200;

    Project* project = nullptr;
    bool paused = false;
    bool minimized = false;
    std::vector<DropDownItem> dropDownsActiveMenuItem;
    std::vector<DropDownItem> dropDownsActiveHovered;
    std::vector<DropDownItem*> dropDownsTopMenuActive;
    std::vector<DropDownItem*> dropDownsGameObjectsActive;
    float menuHoverMargin = 4.0f;

};

#endif //GAME_H
