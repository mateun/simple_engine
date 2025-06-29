#include <iostream>
#include <Win32Window.h>
#include <engine.h>
#define GRAPHICS_IMPLEMENTATION
#include <engine.h>
#include "game.h"
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

void do_frame(const Win32Window & window, GameState& gameState) {
    clear(0, 0.2, 0, 1);

    // Update camera
    uploadConstantBufferData( gameState.graphics.cameraTransformBuffer, gameState.cameraData, sizeof(CameraBuffer), 1);

    // Render test for our 2d quad
    {
        bindVertexArray(gameState.graphics.vertexArray);
        bindShaderProgram(gameState.graphics.shaderProgram);
        bindTexture(gameState.graphics.textureHandle, 0);
        static float roto = 0.0f;
        roto += 10.0f * frame_time;
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 2)) * rotationMatrix * scaleMatrix;
        // worldMatrix = (glm::translate(glm::mat4(1), glm::vec3(0, 0, 2.5)));
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);

        scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(.6, .6, .6));
        rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(-roto*2), glm::vec3(0.0f, 0.0f, 1.0f));
        worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-3, 0, 2)) * rotationMatrix * scaleMatrix;
        // worldMatrix = (glm::translate(glm::mat4(1), glm::vec3(0, 0, 2.5)));
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
    }

    // Render testwise text
    auto textMesh = gameState.meshPool["text1"];
    bindVertexArray(textMesh->meshVertexArray);
    bindShaderProgram(gameState.graphics.shaderProgram);
    bindTexture(getTextureFromFont(gameState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
    auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(.04, .04, 1));
    auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-2, 0, 1)) * rotationMatrix * scaleMatrix;
    // worldMatrix = (glm::translate(glm::mat4(1), glm::vec3(0, 0, 2.5)));
    uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
    renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);

    // Render each game object
    for (auto go : gameState.gameObjects) {
        bindVertexArray(go->renderData.mesh->meshVertexArray);
        bindShaderProgram(gameState.graphics.shaderProgram);
        bindTexture(gameState.graphics.textureHandle, 0);
        static float roto = 0.0f;
        roto += 5.0f * frame_time;
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(2, 0, 1)) * rotationMatrix * scaleMatrix;
        // worldMatrix = (glm::translate(glm::mat4(1), glm::vec3(0, 0, 2.5)));
        uploadConstantBufferData( gameState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, go->renderData.mesh->index_count, 0);
    }


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

// Pipeline:
// raw_vertex_data -> vertexShder -> pixelShader -> frameBuffer (screen)

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



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev_iinst, LPSTR, int) {
    int width = 800;
    int height = 600;
    auto window = Win32Window(hInstance, SW_SHOWDEFAULT, L"my window", width, height);
    initGraphics(window, false, 0);

    // Shader initialization
    // auto vs = createShader(vshader_code, ShaderType::Vertex);
    // auto fs = createShader(fshader_code, ShaderType::Fragment);

    GameState gameState;

#ifdef RENDERER_GL46
    gameState.graphics.shaderProgram = createShaderProgram(vshader_glsl, fshader_glsl);
#endif
#ifdef RENDERER_DX11
    gameState.graphics.shaderProgram = createShaderProgram(vshader_hlsl, fshader_hlsl);
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
    auto textData = renderTextIntoQuadGeometry(gameState.graphics.fontHandle, "hi, good game!");
    std::vector<float> textVertexList;
    for (int i = 0; i < textData->posMasterList.size(); i++) {
        textVertexList.push_back(textData->posMasterList[i].x);
        textVertexList.push_back(textData->posMasterList[i].y);
        textVertexList.push_back(textData->posMasterList[i].z);
        textVertexList.push_back(textData->uvMasterList[i].x);
        textVertexList.push_back(textData->uvMasterList[i].y);
    }
    Mesh *textMesh = new Mesh();
    textMesh->index_count = textData->indicesFlat.size();
    textMesh->meshVertexArray = createVertexArray();
    textMesh->meshVertexBuffer = createVertexBuffer(textVertexList.data(), textVertexList.size() * sizeof(float), sizeof(float) * 5);
    associateVertexBufferWithVertexArray(textMesh->meshVertexBuffer, textMesh->meshVertexArray);
    textMesh->meshIndexBuffer = createIndexBuffer(textData->indicesFlat.data(), textData->indicesFlat.size() * sizeof(uint32_t));
    associateIndexBufferWithVertexArray( textMesh->meshIndexBuffer, textMesh->meshVertexArray);
    describeVertexAttributes(vertexAttributes, textMesh->meshVertexBuffer, gameState.graphics.shaderProgram, textMesh->meshVertexArray);
    gameState.meshPool["text1"] = textMesh;
    // End text rendering (huge... TODO improve)

    // Mesh Import and vertex buffer creation etc.
    auto importedMeshes = importMeshFromFile("assets/test.glb");



    for (auto im : importedMeshes) {
        auto mesh = new Mesh();
        mesh->index_count = im->indicesFlat.size();
        mesh->meshVertexArray = createVertexArray();

        // Now we assemble our pos|uv vertex layout:
        std::vector<float> vertexList;
        for (int i = 0; i < im->posMasterList.size(); i++) {
            vertexList.push_back(im->posMasterList[i].x);
            vertexList.push_back(im->posMasterList[i].y);
            vertexList.push_back(im->posMasterList[i].z);
            vertexList.push_back(im->uvMasterList[i].x);
            vertexList.push_back(im->uvMasterList[i].y);
        }
        mesh->meshVertexBuffer = createVertexBuffer(vertexList.data(), vertexList.size() * sizeof(float), sizeof(float) * 5);
        associateVertexBufferWithVertexArray(mesh->meshVertexBuffer, mesh->meshVertexArray);
        mesh->meshIndexBuffer = createIndexBuffer(im->indicesFlat.data(), im->indicesFlat.size() * sizeof(uint32_t));
        associateIndexBufferWithVertexArray( mesh->meshIndexBuffer, mesh->meshVertexArray);
        describeVertexAttributes(vertexAttributes, mesh->meshVertexBuffer, gameState.graphics.shaderProgram, mesh->meshVertexArray);
        gameState.meshPool[im->meshName] = mesh;
    }

    auto heroGo = new GameObject();
    heroGo->renderData.mesh = gameState.meshPool["Cube.001"];
    gameState.gameObjects.push_back(heroGo);

    gameState.graphics.vertexArray = createVertexArray();
    bindVertexArray(gameState.graphics.vertexArray);
    gameState.graphics.objectTransformBuffer = createConstantBuffer(sizeof(glm::mat4));
    gameState.graphics.cameraTransformBuffer = createConstantBuffer(sizeof(glm::mat4) * 2);
    gameState.cameraData = new CameraBuffer();
    gameState.cameraData->view_matrix = glm::lookAtLH(glm::vec3(0, 0, -2), glm::vec3(0, 0, 3), glm::vec3(0, 1, 0));
    // gameState.cameraData->view_matrix = glm::mat4(1);
    gameState.cameraData->projection_matrix = (glm::orthoLH_ZO<float>(0, 800, 0, 600, 0.0, 50));
    gameState.cameraData->projection_matrix = glm::perspectiveFovLH_ZO<float>(glm::radians(65.0f), width, height, 0.1, 100);

    gameState.graphics.quadVertexBuffer = createVertexBuffer(tri_vertices.data(), tri_vertices.size() * sizeof(float), sizeof(float) * 5);
    associateVertexBufferWithVertexArray(gameState.graphics.quadVertexBuffer, gameState.graphics.vertexArray);
    gameState.graphics.quadIndexBuffer = createIndexBuffer(tri_indices.data(), tri_indices.size() * sizeof(uint32_t));
    associateIndexBufferWithVertexArray(gameState.graphics.quadIndexBuffer, gameState.graphics.vertexArray);
    describeVertexAttributes(vertexAttributes, gameState.graphics.quadVertexBuffer, gameState.graphics.shaderProgram, gameState.graphics.vertexArray);


    int image_width, image_height;
    auto pixels = load_image("assets/debug_texture.jpg", &image_width, &image_height);
    gameState.graphics.textureHandle = createTexture(image_width, image_height, pixels, 4);

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
