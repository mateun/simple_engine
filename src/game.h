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
    };

    struct RenderData {
        GraphicsHandle transformBuffer;
        GraphicsHandle materialBuffer;
        GraphicsHandle meshVertexBuffer;
        GraphicsHandle meshIndexBuffer;
        GraphicsHandle meshVertexArray;
        GraphicsHandle meshTextureHandle;
        GraphicsHandle meshShaderProgram;
        std::vector<Mesh*> meshes;
    } renderData;


};

struct alignas(16) ObjectTransformBuffer {
    glm::mat4 world_matrix;
};





struct GameState {

    std::map<std::string, Mesh*> meshPool;
    std::map<std::string, GraphicsHandle> texturePool;

    struct GraphicsState {
        GraphicsHandle shaderProgram;
        GraphicsHandle textShaderProgram;
        GraphicsHandle animatedShaderProgram;
        GraphicsHandle quadVertexBuffer;
        GraphicsHandle quadIndexBuffer;
        GraphicsHandle vertexArray;
        GraphicsHandle textureHandle;
        GraphicsHandle cameraTransformBuffer;
        GraphicsHandle objectTransformBuffer;
        GraphicsHandle skinningMatricesCBuffer;
        GraphicsHandle fontHandle;
        GraphicsHandle frameBuffer3DPanel;
        GraphicsHandle frameBufferTopMenu;
        GraphicsHandle frameBufferAssetPanel;
        GraphicsHandle frameBufferSceneTree;

        GraphicsHandle jointDebugTexture;
        Mesh* jointDebugMesh = nullptr;

    } graphics;

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
};

#endif //GAME_H
