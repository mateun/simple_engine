#include <iostream>
#include <Win32Window.h>
#include <engine.h>
#define GRAPHICS_IMPLEMENTATION
#include <engine.h>
#include "game.h"
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <ranges>
#include <stb_image.h>
#include <assimp/anim.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "glm/gtx/quaternion.hpp"

static float frame_time = 0.0f;

void clear_screen(int width, int height, Win32Window& window, uint32_t *pixels32) {
    auto start_clear = window.performance_count();
    memset(pixels32, 0, width * height * 4);
    auto end_clear = window.performance_count();
    auto clear_time = window.measure_time_in_seconds(start_clear, end_clear);
#ifdef _PERF_MEASURE
    std::cout << "clear time: " << (clear_time * 1000.0f) << " ms" << std::endl;
#endif

}

#ifdef RENDERER_SW
void do_sw_frame(Win32Window& window) {
    clear_screen(window.getWidth(), window.getHeight(), window, (uint32_t*)window.getBackbufferPixels());

    static float offset = 0;
    offset += 8.0f * frame_time;
    for (int i = 0; i < 100; i++) {
        setPixel(window, 10 + i + offset, 10 , 0xFFFFF00);
    }

    drawLine(window, 100, 100, 118, 80, 0xFFFF0000);
    drawLine(window, 100, 100, 118, 120, 0xFFFF0000);
    drawLine(window, 118, 120, 128, 80, 0xFFFF0000);
    drawLine(window, 100, 100, 128, 80, 0xFFFF0000);
    window.present();
}

#endif

void update_camera(GameState& gameState) {
    float movementSpeed = 10.0f;
    float camspeed = movementSpeed * frame_time;
    float rotspeed = movementSpeed * frame_time;
    float dir = 0;
    float hdir = 0;
    float yaw = 0;
    float pitch = 0;

    // This allows moving fwd/bwd in the current plane.
    // So even if you look down pitched, you would still move level.
    float panFwd = 0;
    if (isKeyDown('E')
        //|| getControllerAxis(ControllerAxis::RSTICK_X, 0) > 0.4
        ) {
        yaw = -1;
    }

    if (isKeyDown('W')) {
        dir = 1;
    }
    if (isKeyDown('S')) {
        dir = -1;
    }
    if (isKeyDown('A')) {
        hdir = -1;
    }
    if (isKeyDown('D')) {
        hdir = 1;
    }

    auto fwd = gameState.perspectiveCamera->getForward();
    auto right = gameState.perspectiveCamera->getRight();
    auto up = gameState.perspectiveCamera->getUp();

    auto locCandidate = gameState.perspectiveCamera->location + glm::vec3{camspeed * fwd.x, 0, camspeed * fwd.z} * dir;
    // Do any checks if needed.
    gameState.perspectiveCamera->location = locCandidate;

    gameState.perspectiveCamera->updateAndGetViewMatrix();


}

void renderTopMenu(int originX, int originY, int width, int height, GameState& gameState) {
    {
        // Render topmenu
        setFrontCulling(true);
        bindVertexArray(gameState.graphics.quadVertexArray);
        bindShaderProgram(gameState.graphics.shaderProgram);
        bindFrameBuffer(gameState.graphics.frameBufferTopMenu, 0, 0, width, height);
        clearFrameBuffer(gameState.graphics.frameBufferTopMenu, 0.1, .1, 0.1, 1);


        // Draw to default backbuffer
        bindDefaultBackbuffer(0, 0, gameState.screen_width, gameState.screen_height);
        bindFrameBufferTexture(gameState.graphics.frameBufferTopMenu, 0);
        uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(width, height, 1));
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(originX, originY, 0.9)) * rotationMatrix * scaleMatrix;
        //setFrontCulling(false);
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
    }

    // Render frametime:
    // Currently directly into the main framebuffer - TODO change?!
        bindShaderProgram(gameState.graphics.textShaderProgram);
        uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);

        auto textMesh = gameState.textMesh;
        updateText(*textMesh, gameState.graphics.fontHandle, "FTus: " + std::to_string(gameState.frameTimer->getAverage() * 1000 * 1000));
        bindVertexArray(textMesh->meshVertexArray);
        bindTexture(getTextureFromFont(gameState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 64, 1)) * rotationMatrix * scaleMatrix;
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
    // End text rendering


}

void renderSceneTree(int originX, int originY, int width, int height, GameState& gameState) {
        setFrontCulling(true);
        bindVertexArray(gameState.graphics.quadVertexArray);
        bindShaderProgram(gameState.graphics.shaderProgram);
        bindFrameBuffer(gameState.graphics.frameBufferSceneTree, 0, 0, width, height);
        clearFrameBuffer(gameState.graphics.frameBufferSceneTree, .1, .1, 0.1, 1);
        bindTexture(gameState.graphics.textureHandle, 0);
        Camera sceneTreeCamera;
        sceneTreeCamera.view_matrix = glm::mat4(1);
        sceneTreeCamera.projection_matrix = glm::orthoLH_ZO<float>(0.0f, (float) width, (float) height, 0.0f, 0.0, 30);
        uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, &sceneTreeCamera, sizeof(Camera), 1);

        // Now render the actual scene tree objects (gameObjects),
        // for now just dummy rects.
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(128, 16, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(120, 50, 2)) * rotationMatrix * scaleMatrix;
        // worldMatrix = (glm::translate(glm::mat4(1), glm::vec3(0, 0, 2.5)));
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);


        // Text rendering
        // bindShaderProgram(gameState.graphics.textShaderProgram);
        // uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.orthoCamera, sizeof(Camera), 1);
        // auto textMesh = gameState.textMesh;
        // updateText(*textMesh, gameState.graphics.fontHandle, "FTus: " + std::to_string(gameState.frameTimer->getAverage() * 1000 * 1000));
        // bindVertexArray(textMesh->meshVertexArray);
        // bindTexture(getTextureFromFont(gameState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
        // rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        // scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        // worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, gameState.screen_height-64, 1)) * rotationMatrix * scaleMatrix;
        // renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
        // End text rendering

        // Render the framebuffer as a quad somewhere on the screen:


        bindDefaultBackbuffer(0, 0, gameState.screen_width, gameState.screen_height);
        bindFrameBufferTexture(gameState.graphics.frameBufferSceneTree, 0);
        uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);
        scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(width, height, 1));
        rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(originX, originY, 2)) * rotationMatrix * scaleMatrix;
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
}

void renderPanels(GameState& gameState) {

    // Bind the basic quad vertex buffer and shader, so we can do 2d rendering:


    // Use our common ortho camera here, which is currently not scrollable.
    // But maybe an interesting idea to have a "virtual editor desktop", where the user could scroll around.
    // As we draw everything ourselves anyways, the editor itself could be like a game world with panel-continents on it.


    // Here we should know where every panel is, so we can give it its position as arguments.
    // This is bettter than letting the panel know where it is supposed to be. It normally can not know it.
    renderTopMenu(400, 16, gameState.screen_width, 32, gameState);
    renderSceneTree(200/2, 32+ (600-64)/2, 200, 600-64, gameState);
    renderTopMenu(400, gameState.screen_height - 16, gameState.screen_width, 32, gameState);
    // render3DView();
    // renderAssetPanel();

}

void do_frame(const Win32Window & window, GameState& gameState) {

    //setViewport(100, 100, 700, 500);
    update_camera(gameState);

    setDepthTesting(true);
    clear(0, 0.0, 0, 1);

    renderPanels(gameState);

    // Render 3D scene, first into framebuffer, then as quad on to the final backbuffer.
    setFrontCulling(false);


    bindShaderProgram(gameState.graphics.shaderProgram3D);
    bindFrameBuffer(gameState.graphics.frameBuffer3DPanel, 0, 0, 600, 400);
    clearFrameBuffer(gameState.graphics.frameBuffer3DPanel, .0, .0, 0.0, 1);
    uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.perspectiveCamera->matrixBufferPtr(), sizeof(glm::mat4) * 2, 1);

    // Animation timing
    static float animationTime = 0.0f;
    animationTime += frame_time;

    // Render each game object:
    for (auto go : gameState.gameObjects) {
        // Render each mesh of this game object:
        for (auto mesh : go->renderData.meshes) {
            bindVertexArray(mesh->meshVertexArray);

            // Texture: first we take one of the gameObject, if it is set.
            // If not present, we take the one from the mesh.
            // So, the gameObject can override the "bakedIn" original texture
            // if needed.
            // Otherwise, fall back to dummy texture for now.
            if (go->diffuseTexture.id != -1) {
                bindTexture(go->diffuseTexture, 0);
            } else {
                if (mesh->diffuseTexture.id != -1) {
                    bindTexture(mesh->diffuseTexture, 0);
                } else {
                    bindTexture(gameState.texturePool["debug_texture"], 0);
                }

            }

            // Skeleton/joint posing:
            if (mesh->skeleton != nullptr) {
                if (mesh->skeleton->animations.size() > 0) {
                    // We need a different shader here, which knows our attributes for blend-weights, blend-indices and the
                    // matrix palette cbuffer.
                    bindShaderProgram(gameState.graphics.animatedShaderProgram);

                    // Run the first animation here
                    auto animation = mesh->skeleton->animations[0];
                    if (animationTime >= animation->duration) {
                        std::cout << "Animation looping" << std::to_string(animationTime) << std::endl;
                        animationTime = 0.0f;
                    }
                    for (auto joint : mesh->skeleton->joints) {
                        auto translationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Translation, joint->name);
                        auto rotationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Rotation, joint->name);
                        auto scalingKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Scale, joint->name);

                        // Translation interpolation:
                        float t0 = translationKeys.posKeys.first.mTime / animation->ticksPerSecond;;
                        float t1 = translationKeys.posKeys.second.mTime / animation->ticksPerSecond;
                        glm::vec3 p0 = aiToGLM(translationKeys.posKeys.first.mValue);
                        glm::vec3 p1 = aiToGLM(translationKeys.posKeys.second.mValue);
                        float t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                        glm::vec3 p = glm::mix(p0, p1, t);

                        // Rotation interpolation
                        t0 = rotationKeys.rotKeys.first.mTime / animation->ticksPerSecond;
                        t1 = rotationKeys.rotKeys.second.mTime / animation->ticksPerSecond;
                        glm::quat q0 = aiToGLM(rotationKeys.rotKeys.first.mValue);
                        glm::quat q1 = aiToGLM(rotationKeys.rotKeys.second.mValue);
                        t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                        // if (joint->name == "spine") {
                        //     printf("time: %.2f | t0: %.2f, t1: %.2f, t: %.2f\n", animationTime, t0, t1, t);
                        // }
                        glm::quat q = glm::slerp(q0, q1, t);

                        // Scaling interpolation
                        t0 = scalingKeys.scaleKeys.first.mTime / animation->ticksPerSecond;
                        t1 = scalingKeys.scaleKeys.second.mTime / animation->ticksPerSecond;
                        glm::vec3 s0 = aiToGLM(scalingKeys.scaleKeys.first.mValue);
                        glm::vec3 s1 = aiToGLM(scalingKeys.scaleKeys.second.mValue);
                        t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                        glm::vec3 s = glm::mix(s0, s1, t);

                        joint->poseLocalTransform = translate(glm::mat4(1.0f), p) * glm::toMat4(q) *
                            glm::scale(glm::mat4(1.0f), s);

                    }

                    // Now calculate the global pose for each joint:
                    for (auto j  : mesh->skeleton->joints) {
                        if (j->parent) {
                            j->poseGlobalTransform = j->parent->poseGlobalTransform * j->poseLocalTransform;
                        } else {
                            j->poseGlobalTransform = j->poseLocalTransform; // only the root bone has no parent.
                        }
                        j->poseFinalTransform = j->poseGlobalTransform * j->inverseBindMatrix;

                    }

                    // build frame matrix palette.
                    // One transform per joint, we upload this to the gpu for skinning inside the vertex shader:
                    std::vector<glm::mat4> framePalette;
                    for (auto& j : mesh->skeleton->joints) {
                        framePalette.push_back(j->poseFinalTransform);
                    }

                    // Upload to gpu:
                    uploadConstantBufferData( gameState.graphics.skinningMatricesCBuffer, framePalette.data(), sizeof(glm::mat4) * framePalette.size(), 2);


                }
            }

            static float roto = 0.0f;
            roto += 0.0f * frame_time;
            auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), go->transform.position) * rotationMatrix * scaleMatrix;
            uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, mesh->index_count, 0);
        }

    }

    // Render the 3D panel framebuffer as quad
    {


        bindDefaultBackbuffer(0, 0, gameState.screen_width, gameState.screen_height);
        bindFrameBufferTexture(gameState.graphics.frameBuffer3DPanel, 0);
        uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);

        bindVertexArray(gameState.graphics.quadVertexArray);
        bindShaderProgram(gameState.graphics.shaderProgram);

        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(600, 400, 1));
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(200 + 600/2, 32 + 500/2, 0.9)) * rotationMatrix * scaleMatrix;
        //setFrontCulling(false);
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
    }

    // End 3d scene






    present();
}

static std::string vshader_hlsl = R"(
    struct VOutput
    {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;
    };

    // Per object transformation (movement, rotation, scale)
    cbuffer ObjectTransformBuffer : register(b0) {
        row_major matrix world_matrix;
    };

    // PerFrame
    cbuffer CameraBuffer : register(b1) {
        row_major float4x4 view_matrix;
        row_major float4x4 projection_matrix;

    };

    VOutput main(float4 pos : POSITION, float2 uv : TEXCOORD0) {
        VOutput output;

        output.pos = mul(pos, world_matrix);
        output.pos = mul(output.pos, view_matrix);
        output.pos = mul(output.pos, projection_matrix);
        output.uv = uv;

        return output;
     }
)";



 static std::string vshader_hlsl_animated = R"(
    struct VOutput
    {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;

    };

    // Per object transformation (movement, rotation, scale)
    cbuffer ObjectTransformBuffer : register(b0) {
        row_major matrix world_matrix;
    };

    // PerFrame
    cbuffer CameraBuffer : register(b1) {
        row_major float4x4 view_matrix;
        row_major float4x4 projection_matrix;

    };

    cbuffer SkinningMatrices : register(b2) {
        row_major float4x4 jointTransforms[50];
    };

    VOutput main(float4 pos : POSITION, float2 uv : TEXCOORD0,
                float4 blend_weights : BLENDWEIGHT,
                float4 blend_indices : BLENDINDICES) {

        VOutput output;


        float4 skinned = mul(pos, jointTransforms[blend_indices.x]) * blend_weights.x;
        skinned += mul(pos, jointTransforms[blend_indices.y]) * blend_weights.y;
        skinned += mul(pos, jointTransforms[blend_indices.z]) * blend_weights.z;
        skinned += mul(pos, jointTransforms[blend_indices.w]) * blend_weights.w;

        output.pos = mul(skinned, world_matrix);
        output.pos = mul(output.pos, view_matrix);
        output.pos = mul(output.pos, projection_matrix);
        output.uv = uv;

        return output;
     }
)";

static std::string fs_text_hlsl = R"(
    struct VOutput
    {
        float4 pos: SV_POSITION;
        float2 uv: TEXCOORD0;
    } input;

    Texture2D imageTexture;
    SamplerState samplerState;

    float4 main(VOutput input) : SV_TARGET
    {
        float r = imageTexture.Sample(samplerState, input.uv).r;
        return float4(1, 1 , 1, r);
    };
)";

static std::string fshader_3d_hlsl=  R"(

    struct VOutput
     {
     	float4 pos : SV_POSITION;
     	float2 uv : TEXCOORD0;
     };

    Texture2D imageTexture;
    SamplerState samplerState;

    float4 main(VOutput pixelShaderInput) : SV_TARGET
    {
        return imageTexture.Sample(samplerState, pixelShaderInput.uv);
        //return float4(1.0, 1.0, 0.0, 1.0); // yellow for dx11
        //return float4(pixelShaderInput.uv.x, pixelShaderInput.uv.y, 0, 1);
        //return float4(1.0, 0.5, 0.0, 1.0);
    }
)";

static std::string fshader_hlsl = R"(

    struct VOutput
     {
     	float4 pos : SV_POSITION;
     	float2 uv : TEXCOORD0;
     };

    Texture2D imageTexture;
    SamplerState samplerState;

    float4 main(VOutput pixelShaderInput) : SV_TARGET
    {
        return imageTexture.Sample(samplerState, pixelShaderInput.uv);
        //return float4(1.0, 1.0, 0.0, 1.0); // yellow for dx11
        //return float4(pixelShaderInput.uv.x, pixelShaderInput.uv.y, 0, 1);
        //return float4(1.0, 0.5, 0.0, 1.0);
    }
)";


static std::string vshader_glsl = R"(

    #version 460

    layout(location = 0) in vec3 pos;
    layout(location = 1) in vec2 uv;
    //layout(location = 2) in vec3 normal;

    layout(std140, binding = 0) uniform ObjectTransformBuffer {
        mat4 worldMatrix;
    } objectTransform;

    layout(std140, binding = 1) uniform CameraBuffer {
        mat4 viewMatrix;
        mat4 projectionMatrix;
    } camera;

    out vec2 fs_uv;

    void main() {
        gl_Position = camera.projectionMatrix * camera.viewMatrix * objectTransform.worldMatrix * vec4(pos, 1);
        fs_uv = uv;

    };

)";



static std::string fshader_glsl = R"(
    #version 460

    layout(binding = 0) uniform sampler2D diffuseTexture;


    out vec4 color;

    in vec2 fs_uv;

    void main() {
        //color = vec4(1, 0, 0, 1);
        color = texture(diffuseTexture, fs_uv);
    }

)";


uint8_t* load_image(const std::string& fileName, int* width, int* height) {
    int imageChannels;

    auto pixels = stbi_load(
            fileName.c_str(), width, height,
            &imageChannels,
            4);

    return pixels;

}

struct VOutput
{
    glm::vec4 pos;
    glm::vec2 tex;
};

void initEditor(GameState& gameState) {
#ifdef RENDERER_GL46
    gameState.graphics.shaderProgram = createShaderProgram(vshader_glsl, fshader_glsl);
#endif
#ifdef RENDERER_DX11
    gameState.graphics.shaderProgram = createShaderProgram(vshader_hlsl, fshader_hlsl);
    gameState.graphics.shaderProgram3D = createShaderProgram(vshader_hlsl, fshader_3d_hlsl);
    gameState.graphics.animatedShaderProgram = createShaderProgram(vshader_hlsl_animated, fshader_hlsl);
    gameState.graphics.textShaderProgram = createShaderProgram(vshader_hlsl, fs_text_hlsl);
#endif

    std::vector<float> tri_vertices = {
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
         -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
        0.5, 0.5, 0.0f, 1.0f, 1.0f

    };


    std::vector<uint32_t> tri_indices = {
        0,2, 1,
        1, 2, 3
    };

    // Our "current" default layout: pos|uv
    std::vector<VertexAttributeDescription> vertexAttributes =  {
        {"POSITION", 0, 3, DataType::Float, sizeof(float) * 5,
            0 },

        {"TEXCOORD", 0, 2, DataType::Float, sizeof(float) * 5,
            sizeof(float) * 3 }
    };

    std::vector<VertexAttributeDescription> vertexAttributesAnimated =  {
                {"POSITION", 0, 3, DataType::Float, sizeof(float) * 13,
                0 },

                {"TEXCOORD", 0, 2, DataType::Float, sizeof(float) * 13,
                sizeof(float) * 3 },

                {"BLENDWEIGHT", 0, 4, DataType::Float, sizeof(float) * 13,
                    sizeof(float) * 5 },
                {"BLENDINDICES", 0, 4, DataType::Float, sizeof(float) * 13,
                    sizeof(float) * 9 },
    };

    // Prepare text render meshes:
    gameState.graphics.fontHandle = createFont("assets/consola.ttf", 16);
    gameState.textMesh = createTextMesh(gameState.graphics.fontHandle, "Test text rendering");


    // Prepare an offscreen framebuffer for the 3d scene:
    gameState.graphics.frameBuffer3DPanel = createFrameBuffer(600, 400, true);
    gameState.graphics.frameBufferTopMenu = createFrameBuffer(800, 64, true);
    gameState.graphics.frameBufferSceneTree = createFrameBuffer(200, 600-64, true);


    // Mesh import and vertex buffer creation etc.
    auto importedMeshes = importMeshFromFile("assets/test2.glb");
    for (auto im : importedMeshes) {
        auto mesh = im->toMesh();
        // Now we decide which attributes to link with:
        // For skeletal meshes with animations we use the gpu vertex skinned version, otherwise the "default one":
        if (mesh->skeleton != nullptr && mesh->skeleton->animations.size() > 0) {
            describeVertexAttributes(vertexAttributesAnimated, mesh->meshVertexBuffer, gameState.graphics.animatedShaderProgram, mesh->meshVertexArray);
        } else {
            describeVertexAttributes(vertexAttributes, mesh->meshVertexBuffer, gameState.graphics.shaderProgram, mesh->meshVertexArray);
        }
        gameState.meshPool[im->meshName] = mesh;
    }

    // {
    //     auto importedTestMesh = importMeshFromFile("assets/test4.glb")[0]->toMesh();
    //     gameState.meshPool["testmesh4"] = importedTestMesh;
    //     describeVertexAttributes(vertexAttributes, importedTestMesh->meshVertexBuffer, gameState.graphics.shaderProgram, importedTestMesh->meshVertexArray);
    // }

    // Joint debug mesh import
    {
        gameState.graphics.jointDebugMesh =  importMeshFromFile("assets/joint_debug_mesh.glb")[0]->toMesh();
        describeVertexAttributes(vertexAttributes, gameState.graphics.jointDebugMesh->meshVertexBuffer, gameState.graphics.shaderProgram, gameState.graphics.jointDebugMesh->meshVertexArray);

    }

    auto heroGo = new GameObject();
    for (auto mesh : std::ranges::views::values(gameState.meshPool)) {
        heroGo->renderData.meshes.push_back(mesh);
    }
    heroGo->transform.position = glm::vec3(0, 0, 0);

    gameState.gameObjects.push_back(heroGo);

    gameState.graphics.quadVertexArray = createVertexArray();
    bindVertexArray(gameState.graphics.quadVertexArray);
    gameState.graphics.skinningMatricesCBuffer = createConstantBuffer(sizeof(glm::mat4) * 50);  // TODO have one per skeleton and adjust to the number of actual joints
    gameState.graphics.objectTransformBuffer = createConstantBuffer(sizeof(glm::mat4));
    gameState.graphics.cameraTransformBuffer = createConstantBuffer(sizeof(glm::mat4) * 2);
    gameState.perspectiveCamera = new Camera();
    gameState.perspectiveCamera->location = {0, 2, -2};
    gameState.perspectiveCamera->lookAtTarget = {0, 0, 3};
    gameState.perspectiveCamera->view_matrix =gameState.perspectiveCamera->updateAndGetViewMatrix();
    gameState.perspectiveCamera->projection_matrix = gameState.perspectiveCamera->updateAndGetPerspectiveProjectionMatrix(65,
        gameState.screen_width, gameState.screen_height, 0.1, 100);
        glm::perspectiveFovLH_ZO<float>(glm::radians(65.0f), gameState.screen_width,
            gameState.screen_height, 0.1, 100);

    gameState.orthoCamera = new Camera();
    gameState.orthoCamera->view_matrix = glm::mat4(1);
    gameState.orthoCamera->projection_matrix = (glm::orthoLH_ZO<float>(0, gameState.screen_width,  gameState.screen_height, 0.0f, 0.0, 50));

    gameState.graphics.quadVertexBuffer = createVertexBuffer(tri_vertices.data(), tri_vertices.size() * sizeof(float), sizeof(float) * 5);
    associateVertexBufferWithVertexArray(gameState.graphics.quadVertexBuffer, gameState.graphics.quadVertexArray);
    gameState.graphics.quadIndexBuffer = createIndexBuffer(tri_indices.data(), tri_indices.size() * sizeof(uint32_t));
    associateIndexBufferWithVertexArray(gameState.graphics.quadIndexBuffer, gameState.graphics.quadVertexArray);
    describeVertexAttributes(vertexAttributes, gameState.graphics.quadVertexBuffer, gameState.graphics.shaderProgram, gameState.graphics.quadVertexArray);


    int image_width, image_height;
    auto pixels = load_image("assets/debug_texture.jpg", &image_width, &image_height);
    gameState.graphics.textureHandle = createTexture(image_width, image_height, pixels, 4);
    gameState.texturePool["debug_texture"] = gameState.graphics.textureHandle;

    auto pixelsdj = load_image("assets/debug_joint_texture.png", &image_width, &image_height);
    gameState.graphics.jointDebugTexture = createTexture(image_width, image_height, pixelsdj, 4);

    pixelsdj = load_image("assets/debug_texture_2.png", &image_width, &image_height);
    gameState.texturePool["testmesh4_texture"] = createTexture(image_width, image_height, pixelsdj, 4);

    pixelsdj = load_image("assets/Ch15_1002_Diffuse.png", &image_width, &image_height);
    gameState.texturePool["ch15"] = createTexture(image_width, image_height, pixelsdj, 4);

}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev_iinst, LPSTR, int) {
    int width = 800;
    int height = 600;
    auto window = Win32Window(hInstance, SW_SHOWDEFAULT, L"my window", width, height);
    initGraphics(window, false, 0);

    // Shader initialization
    // auto vs = createShader(vshader_code, ShaderType::Vertex);
    // auto fs = createShader(fshader_code, ShaderType::Fragment);

    GameState gameState;
    gameState.screen_width = width;
    gameState.screen_height = height;
    initEditor(gameState);

    gameState.frameTimer = new FrameTimer(600);;

    bool running = true;
    while (running) {
        auto start_tok = window.performance_count();
        running = window.process_messages();

        do_frame(window, gameState);

        auto end_tok = window.performance_count();
        frame_time = window.measure_time_in_seconds(start_tok, end_tok);
        gameState.frameTimer->addFrameTime(frame_time);

#ifdef _PERF_MEASURE
        std::cout << "frametime: " << frame_time << std::endl;
#endif

    }
    return 0;
}

