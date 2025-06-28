//
// Created by mgrus on 26.06.2025.
//

#ifndef GAME_H
#define GAME_H
#include <map>

#include "glm/vec3.hpp"
#include <glm/glm.hpp>

struct Mesh {
    GraphicsHandle transformBuffer;
    GraphicsHandle meshVertexBuffer;
    GraphicsHandle meshIndexBuffer;
    GraphicsHandle meshVertexArray;
    uint32_t index_count;
};

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
        Mesh* mesh = nullptr;
    } renderData;


};

struct alignas(16) ObjectTransformBuffer {
    glm::mat4 world_matrix;
};

struct alignas(16) CameraBuffer {
    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;

};



struct GameState {

    std::map<std::string, Mesh*> meshPool;

    struct GraphicsState {
        GraphicsHandle shaderProgram;
        GraphicsHandle quadVertexBuffer;
        GraphicsHandle quadIndexBuffer;
        GraphicsHandle vertexArray;
        GraphicsHandle textureHandle;
        GraphicsHandle cameraTransformBuffer;
        GraphicsHandle objectTransformBuffer;
        GraphicsHandle fontHandle;
    } graphics;

    std::vector<GameObject*> gameObjects;
    CameraBuffer * cameraData = nullptr;

    // Text rendering (dummy/temp data for now)
    size_t textIndexCount;
    GraphicsHandle textVertexArray;
    GraphicsHandle textVertexBuffer;
    GraphicsHandle textIndexBuffer;
};

#endif //GAME_H
