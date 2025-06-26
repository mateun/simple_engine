#include <iostream>
#include <Win32Window.h>
#include <graphics.h>
#define GRAPHICS_IMPLEMENTATION
#include <graphics.h>
#include "game.h"
#include <vector>


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

    bindVertexArray(gameState.graphics.vertexArray);
    bindShaderProgram(gameState.graphics.shaderProgram);
    renderGeometry(PrimitiveType::TRIANGLE_LIST);

    present();
}


 static std::string vshader_hlsl = R"(

// struct VOut
// {
// 	float4 pos : SV_POSITION;
// 	float2 tex : TEXCOORD0;
// };
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
     float4 main(float4 pos : POSITION) : SV_POSITION {
         return pos;
     }
)";


static std::string vshader_code = R"(

    #version 460

    layout(location = 0) in vec3 pos;
    layout(location = 1) in vec2 uv;
    layout(location = 2) in vec3 normal;

    void main() {
        gl_Position = vec4(pos, 1);
    };

)";

static std::string fshader_hlsl = R"(



    float4 main() : SV_TARGET
    {
        return float4(1.0, 1.0, 0.0, 1.0); // yellow for dx11
    }
)";

static std::string fshader_code = R"(
    #version 460

    out vec4 color;

    void main() {
        color = vec4(1, 0, 0, 1);
    }

)";


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev_iinst, LPSTR, int) {
    int width = 800;
    int height = 600;
    auto window = Win32Window(hInstance, SW_SHOWDEFAULT, L"my window", width, height);
    initGraphics(window, false, 0);

    // Shader initialization
    // auto vs = createShader(vshader_code, ShaderType::Vertex);
    // auto fs = createShader(fshader_code, ShaderType::Fragment);

    GameState gameState;
    gameState.graphics.shaderProgram = createShaderProgram(vshader_hlsl, fshader_hlsl);

    std::vector<float> tri_vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f,
    };

    gameState.graphics.vertexArray = createVertexArray();
    gameState.graphics.vertexBuffer = createVertexBuffer(tri_vertices.data(), tri_vertices.size() * sizeof(float));
    associateVertexBufferWithVertexArray(gameState.graphics.vertexBuffer, gameState.graphics.vertexArray);
    associateVertexAttribute(0, 3, DataType::Float, 0,
        0, gameState.graphics.vertexBuffer, gameState.graphics.shaderProgram, gameState.graphics.vertexArray);

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
