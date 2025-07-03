#include <iostream>
#include <Win32Window.h>
#include <engine.h>
#define GRAPHICS_IMPLEMENTATION
#include <engine.h>
#include "game.h"
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
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

    gameState.perspectiveCamera->updatenAndGetViewMatrix();


}

void do_frame(const Win32Window & window, GameState& gameState) {

    update_camera(gameState);

    setDepthTesting(true);
    clear(0, 0.0, 0, 1);


    // Render 3D scene
    // Update perspective camera
    uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.perspectiveCamera->matrixBufferPtr(), sizeof(glm::mat4) * 2, 1);

    // Render test for our 2d quad
    {
        bindVertexArray(gameState.graphics.vertexArray);
        bindShaderProgram(gameState.graphics.shaderProgram);
        bindTexture(gameState.graphics.textureHandle, 0);
        static float roto = 0.0f;
        roto += 0.0f * frame_time;
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(3, 0, 2)) * rotationMatrix * scaleMatrix;
        // worldMatrix = (glm::translate(glm::mat4(1), glm::vec3(0, 0, 2.5)));
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);

        scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(-roto*2), glm::vec3(0.0f, 0.0f, 1.0f));
        worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-3, 0, 2)) * rotationMatrix * scaleMatrix;
        // worldMatrix = (glm::translate(glm::mat4(1), glm::vec3(0, 0, 2.5)));
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
    }



    // Render each game object
    bindShaderProgram(gameState.graphics.shaderProgram);
    for (auto go : gameState.gameObjects) {
        bindVertexArray(go->renderData.mesh->meshVertexArray);
        bindTexture(gameState.graphics.textureHandle, 0);

        if (go->renderData.mesh->skeleton != nullptr) {
            static float animationTime = 0.0f;
            animationTime += frame_time;
            if (go->renderData.mesh->skeleton->animations.size() > 0) {
                // Run the first animation here
                auto animation = go->renderData.mesh->skeleton->animations[0];
                for (auto joint : go->renderData.mesh->skeleton->joints) {
                    auto translationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Translation, joint->name);
                    auto rotationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Rotation, joint->name);
                    auto scalingKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Scale, joint->name);

                    // Translation interpolation:
                    float t0 = translationKeys.posKeys.first.mTime;
                    float t1 = translationKeys.posKeys.second.mTime;
                    glm::vec3 p0 = aiToGLM(translationKeys.posKeys.first.mValue);
                    glm::vec3 p1 = aiToGLM(translationKeys.posKeys.second.mValue);
                    float t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    glm::vec3 p = glm::mix(p0, p1, t);

                    // Rotation interpolation
                    t0 = rotationKeys.rotKeys.first.mTime;
                    t1 = rotationKeys.rotKeys.second.mTime;
                    glm::quat q0 = aiToGLM(rotationKeys.rotKeys.first.mValue);
                    glm::quat q1 = aiToGLM(rotationKeys.rotKeys.second.mValue);
                    t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    glm::quat q = glm::slerp(q0, q1, t);

                    // Scaling interpolation
                    t0 = scalingKeys.scaleKeys.first.mTime;
                    t1 = scalingKeys.scaleKeys.second.mTime;
                    glm::quat s0 = aiToGLM(scalingKeys.scaleKeys.first.mValue);
                    glm::quat s1 = aiToGLM(scalingKeys.scaleKeys.second.mValue);
                    t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    glm::quat s = glm::mix(s0, s1, t);

                    joint->poseLocalTransform = translate(glm::mat4(1.0f), p) * glm::toMat4(q) * glm::toMat4(s);

                }

                // Now calculate the global pose for each joint:
                for (auto j  : go->renderData.mesh->skeleton->joints) {
                    if (j->parent) {
                        j->poseGlobalTransform = j->parent->poseGlobalTransform * j->poseLocalTransform;
                    } else {
                        j->poseGlobalTransform = j->poseLocalTransform; // only the root bone has no parent.
                    }
                    j->poseFinalTransform = j->poseGlobalTransform * j->inverseBindMatrix;

                }
            }
        }






        static float roto = 0.0f;
        roto += 0.0f * frame_time;
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 1)) * rotationMatrix * scaleMatrix;

        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        // if (go->renderData.mesh->skeleton != nullptr) {
        //     setFillMode(FillMode::Wireframe);
        // }
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, go->renderData.mesh->index_count, 0);

        // Only show if a special xray button was pressed - TODO
        // If this is a skeletal mesh object we will render every joint of this mesh:
        // TODO bind this to a special debug mode or similar.
        // if (go->renderData.mesh->skeleton != nullptr) {
        //     setFillMode(FillMode::Solid);
        //     bindVertexArray(gameState.graphics.jointDebugMesh->meshVertexArray);
        //     bindTexture(gameState.graphics.jointDebugTexture, 0);
        //     auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
        //     auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(.05, .05, .05));
        //     auto translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 1));


            // Iterate all joints of the skeleton
            // for (auto& joint : go->renderData.mesh->skeleton->joints) {
            //     auto worldMatrix = translationMatrix * glm::inverse(joint->inverseBindMatrix) *  scaleMatrix;
            //     uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            //     setDepthTesting(false);
            //     renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, gameState.graphics.jointDebugMesh->index_count, 0);
            //
            // }
            // setDepthTesting(true);
        // }
    }

    // Render testmesh
    {
        bindShaderProgram(gameState.graphics.shaderProgram);
        bindVertexArray(gameState.meshPool["testmesh4"]->meshVertexArray);
        bindTexture(gameState.texturePool["testmesh4_texture"], 0);
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(.25, .25, .25));
        auto translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-1, 1, 1));
        auto worldMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, gameState.meshPool["testmesh4"]->index_count, 0);


    }


    // Render 2D scene / UI:
    // Render testwise text
    bindShaderProgram(gameState.graphics.textShaderProgram);
    uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.orthoCamera, sizeof(Camera), 1);

    auto textMesh = gameState.textMesh;
    static int frame = 0;
    frame++;
    updateText(*textMesh, gameState.graphics.fontHandle, "FTus: " + std::to_string(frame_time * 1000 * 1000));
    bindVertexArray(textMesh->meshVertexArray);
    bindTexture(getTextureFromFont(gameState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
    auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
    auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, gameState.screen_height-64, 1)) * rotationMatrix * scaleMatrix;
    // worldMatrix = (glm::translate(glm::mat4(1), glm::vec3(0, 0, 2.5)));
    uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
    renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
    // End text rendering



    present();
}


 static std::string vshader_hlsl = R"(


//
//
//         VOut VShader(float4 position : POSITION, float2 tex : TEXCOORD0) {
//
// 	VOut output;
// 	output.pos = mul(position, worldm);
// 	output.pos = mul(output.pos, viewm);
// 	output.pos = mul(output.pos, projm);
//
// 	output.tex = tex;
// 	output.tex.y = 1-output.tex.y;
// 	output.tex.x *= textureScale.x;
// 	output.tex.y *= textureScale.y;
//
// 	return output;
// }
//
//
//
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

void initGame(GameState& gameState) {
#ifdef RENDERER_GL46
    gameState.graphics.shaderProgram = createShaderProgram(vshader_glsl, fshader_glsl);
#endif
#ifdef RENDERER_DX11
    gameState.graphics.shaderProgram = createShaderProgram(vshader_hlsl, fshader_hlsl);
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

    // Render a fixed text:
    gameState.graphics.fontHandle = createFont("assets/consola.ttf", 16);
    gameState.textMesh = createTextMesh(gameState.graphics.fontHandle, "Test text rendering");


    // Mesh Import and vertex buffer creation etc.
    auto importedMeshes = importMeshFromFile("assets/test.glb");
    for (auto im : importedMeshes) {
        auto mesh = im->toMesh();
        describeVertexAttributes(vertexAttributes, mesh->meshVertexBuffer, gameState.graphics.shaderProgram, mesh->meshVertexArray);
        gameState.meshPool["hero"] = mesh;
    }

    {
        auto importedTestMesh = importMeshFromFile("assets/test4.glb")[0]->toMesh();
        gameState.meshPool["testmesh4"] = importedTestMesh;
        describeVertexAttributes(vertexAttributes, importedTestMesh->meshVertexBuffer, gameState.graphics.shaderProgram, importedTestMesh->meshVertexArray);
    }

    // Joint debug mesh import
    {
        gameState.graphics.jointDebugMesh =  importMeshFromFile("assets/joint_debug_mesh.glb")[0]->toMesh();
        describeVertexAttributes(vertexAttributes, gameState.graphics.jointDebugMesh->meshVertexBuffer, gameState.graphics.shaderProgram, gameState.graphics.jointDebugMesh->meshVertexArray);

    }

    auto heroGo = new GameObject();
    heroGo->renderData.mesh = gameState.meshPool["hero"];
    gameState.gameObjects.push_back(heroGo);

    gameState.graphics.vertexArray = createVertexArray();
    bindVertexArray(gameState.graphics.vertexArray);
    gameState.graphics.objectTransformBuffer = createConstantBuffer(sizeof(glm::mat4));
    gameState.graphics.cameraTransformBuffer = createConstantBuffer(sizeof(glm::mat4) * 2);
    gameState.perspectiveCamera = new Camera();
    gameState.perspectiveCamera->location = {0, 2, -2};
    gameState.perspectiveCamera->lookAtTarget = {0, 0, 3};
    gameState.perspectiveCamera->view_matrix =gameState.perspectiveCamera->updatenAndGetViewMatrix();
    gameState.perspectiveCamera->projection_matrix = gameState.perspectiveCamera->updateAndGetPerspectiveProjectionMatrix(65,
        gameState.screen_width, gameState.screen_height, 0.1, 100);
        glm::perspectiveFovLH_ZO<float>(glm::radians(65.0f), gameState.screen_width,
            gameState.screen_height, 0.1, 100);

    gameState.orthoCamera = new Camera();
    gameState.orthoCamera->view_matrix = glm::mat4(1);
    gameState.orthoCamera->projection_matrix = (glm::orthoLH_ZO<float>(0, gameState.screen_width, 0, gameState.screen_height, 0.0, 50));

    gameState.graphics.quadVertexBuffer = createVertexBuffer(tri_vertices.data(), tri_vertices.size() * sizeof(float), sizeof(float) * 5);
    associateVertexBufferWithVertexArray(gameState.graphics.quadVertexBuffer, gameState.graphics.vertexArray);
    gameState.graphics.quadIndexBuffer = createIndexBuffer(tri_indices.data(), tri_indices.size() * sizeof(uint32_t));
    associateIndexBufferWithVertexArray(gameState.graphics.quadIndexBuffer, gameState.graphics.vertexArray);
    describeVertexAttributes(vertexAttributes, gameState.graphics.quadVertexBuffer, gameState.graphics.shaderProgram, gameState.graphics.vertexArray);


    int image_width, image_height;
    auto pixels = load_image("assets/debug_texture.jpg", &image_width, &image_height);
    gameState.graphics.textureHandle = createTexture(image_width, image_height, pixels, 4);

    auto pixelsdj = load_image("assets/debug_joint_texture.png", &image_width, &image_height);
    gameState.graphics.jointDebugTexture = createTexture(image_width, image_height, pixelsdj, 4);

    pixelsdj = load_image("assets/debug_texture_2.png", &image_width, &image_height);
    gameState.texturePool["testmesh4_texture"] = createTexture(image_width, image_height, pixelsdj, 4);

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
    initGame(gameState);

    bool running = true;
    while (running) {
        auto start_tok = window.performance_count();
        running = window.process_messages();

        do_frame(window, gameState);

        auto end_tok = window.performance_count();
        frame_time = window.measure_time_in_seconds(start_tok, end_tok);

#ifdef _PERF_MEASURE
        std::cout << "frametime: " << frame_time << std::endl;
#endif

    }
    return 0;
}

