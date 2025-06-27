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

struct ObjectTransformBuffer {
    glm::mat4 world_matrix;
};

struct CameraBuffer {
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
    } graphics;

    std::vector<GameObject*> gameObjects;
    CameraBuffer * cameraData = nullptr;
};

#endif //GAME_H
