//
// Created by mgrus on 26.06.2025.
//

#ifdef RENDERER_DX11
#include <cassert>
#include <graphics.h>
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <iostream>
#include <map>
#include <vector>
#include <ostream>

static ID3D11Device *device;
static ID3D11DeviceContext *ctx;
static IDXGISwapChain1 *swapChain;
static ID3D11Debug* debugger;
static ID3D11Texture2D *backBuffer;
static ID3D11RenderTargetView* rtv;
static ID3D11Texture2D* depthStencilBuffer;
static ID3D11DepthStencilView *depthStencilView;
static ID3D11DepthStencilState *m_DepthStencilState;


struct DX11VertexArray {
    std::vector<ID3D11Buffer*> vertexBuffers;
    ID3D11InputLayout* inputLayout = nullptr;
};

struct DX11Shader {
    ID3DBlob* blob;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
};

struct DX11ShaderProgram {
    DX11Shader vertexShader;
    DX11Shader pixelShader;
};




static int nextHandleId = 0;
static std::map<int, DX11Shader> pixelShaderMap;
static std::map<int, DX11Shader> vertexShaderMap;
static std::map<int, DX11ShaderProgram> shaderProgramMap;
static std::map<int, ID3D11Buffer*> vertexBufferMap;
static std::map<int, ID3D11Buffer*> indexBufferMap;
static std::map<int, ID3D11InputLayout*> inputLayoutMap;
static std::map<int, DX11VertexArray*> vertexArrayMap;



void initGraphics(Win32Window& window, bool msaa, int msaa_samples) {
    int w = window.getWidth();
    int h = window.getHeight();
    //GetWindowClientSize(hwnd, w, h);
    D3D_FEATURE_LEVEL featureLevels =  D3D_FEATURE_LEVEL_11_1;
    HRESULT result = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, &featureLevels, 1, D3D11_SDK_VERSION,
                                       &device, NULL, &ctx);
    if (FAILED(result)) {
        std::cerr << "CreateDevice failed" << std::endl;
        exit(1);
    }

     UINT ql;
    device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 8, &ql);


    DXGI_SWAP_CHAIN_DESC1 scdesc1 = {};
    scdesc1.Width = w;
    scdesc1.Height = h;
    scdesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scdesc1.Stereo = FALSE;
    scdesc1.SampleDesc.Count = 1;
    scdesc1.SampleDesc.Quality = 0;
    scdesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scdesc1.BufferCount = 2;
    scdesc1.Scaling = DXGI_SCALING_NONE;
    scdesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scdesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    scdesc1.Flags = 0;


    IDXGIDevice * pDXGIDevice = nullptr;
    result = device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
    IDXGIAdapter * pDXGIAdapter = nullptr;
    result = pDXGIDevice->GetAdapter(&pDXGIAdapter);
    // IDXGIFactory * pIDXGIFactory = nullptr;
    // pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&pIDXGIFactory);

    IDXGIFactory2* pFactory2 = nullptr;
    auto hr= pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pFactory2);
    if (FAILED(hr)) {
        exit(5557788);
    }

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsdesc = {};
    fsdesc.Windowed = TRUE;
    result = pFactory2->CreateSwapChainForHwnd(pDXGIDevice, window.get_hwnd(), &scdesc1, &fsdesc, nullptr, &swapChain);
    if (FAILED(result)) {
        printf("error creating swapchain 0x%08X\n ", result);
        exit(1);
    }

    pFactory2->Release();
    // pIDXGIFactory->Release();
    pDXGIAdapter->Release();
    pDXGIDevice->Release();


    // Gather the debug interface
    debugger = 0;
    result = device->QueryInterface(__uuidof(ID3D11Debug), (void**)&debugger);
    if (FAILED(result)) {
        OutputDebugString(L"debuger creation failed\n");
        exit(1);
    }



    // Create a backbuffer
    result = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(result)) {
        OutputDebugString(L"backbuffer creation failed\n");
        exit(1);
    }

    // Bind the backbuffer as our render target
    result = device->CreateRenderTargetView(backBuffer, NULL, &rtv);
    if (FAILED(result)) {
        OutputDebugString(L"rtv creation failed\n");
        exit(1);
    }

    // Create a depth/stencil buffer
    D3D11_TEXTURE2D_DESC td;
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_D32_FLOAT;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;

    result = device->CreateTexture2D(&td, 0, &depthStencilBuffer);
    if (FAILED(result)) {
        OutputDebugString(L"D S buffer creation failed\n");
        exit(1);
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dpd;
    ZeroMemory(&dpd, sizeof(dpd));
    dpd.Flags = 0;
    dpd.Format = DXGI_FORMAT_D32_FLOAT;
    dpd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    result = device->CreateDepthStencilView(depthStencilBuffer, &dpd, &depthStencilView);
    if (FAILED(result)) {
        OutputDebugString(L"D S view creation failed\n");
        exit(1);
    }

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.StencilReadMask = 0xFF;
    depthStencilDesc.StencilWriteMask = 0xFF;

    // Stencil operations if pixel is front-facing
    depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    // Stencil operations if pixel is back-facing
    depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    result = device->CreateDepthStencilState(&depthStencilDesc, &m_DepthStencilState);
    if (FAILED(result)) {
        OutputDebugString(L"failed to set depth stencil state\n");
        exit(1);
    }
    ctx->OMSetDepthStencilState(m_DepthStencilState, 0);

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_BACK;
    rsDesc.FrontCounterClockwise = TRUE;
    rsDesc.DepthClipEnable = TRUE;

    ID3D11RasterizerState* rasterState = nullptr;
    device->CreateRasterizerState(&rsDesc, &rasterState);
    ctx->RSSetState(rasterState);

    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = (FLOAT)w;
    vp.Height = (FLOAT)h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}
#include <comdef.h>
void present() {
    auto hr = swapChain->Present(0, 0);
    if (FAILED(hr)) {
        _com_error err(hr);
        std::wcerr << L"Present failed: " << _com_error(hr).ErrorMessage() << std::endl;
        auto reason = device->GetDeviceRemovedReason();
        std::cerr << "Device removed reason: " << reason << std::endl;
        exit(1);
    }

}
GraphicsHandle createShader(const std::string& code, ShaderType type) {
    ID3DBlob* shaderByteCode;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    #if _DEBUG
    flags |= D3DCOMPILE_DEBUG;
    #endif


    auto versionTarget = type == ShaderType::Vertex ? "vs_5_0" : "ps_5_0";

    ID3DBlob* errorBlob;
    HRESULT hr = D3DCompile(code.c_str(), code.size(), nullptr, nullptr, nullptr,
         "main", versionTarget , flags, 0, &shaderByteCode, &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            std::string errorMsg((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
            OutputDebugStringA(errorMsg.c_str());
            std::cerr << "[Shader Compilation Error]: " << errorMsg.c_str() << std::endl;
            errorBlob->Release();
        } else {
            printf("[Shader Compilation Error]: Unknown error (no error blob)\n");
        }

    }

    GraphicsHandle handle = {nextHandleId++};

    if (type == ShaderType::Vertex) {
        ID3D11VertexShader* ps = nullptr;
        auto hr = device->CreateVertexShader(shaderByteCode->GetBufferPointer(),
            shaderByteCode->GetBufferSize(), nullptr, &ps);
        assert(SUCCEEDED(hr));
        vertexShaderMap[handle.id] =  { shaderByteCode, ps, nullptr};
    }

    else if (type == ShaderType::Fragment) {
        ID3D11PixelShader* ps = nullptr;
        auto hr = device->CreatePixelShader(shaderByteCode->GetBufferPointer(),
            shaderByteCode->GetBufferSize(), nullptr, &ps);
        assert(SUCCEEDED(hr));
        pixelShaderMap[handle.id] =  {shaderByteCode, nullptr, ps};


    }
    return handle;

}
GraphicsHandle createShaderProgram(const std::string& vsCode, const std::string& fsCode) {
    auto vsHandle = createShader(vsCode, ShaderType::Vertex);
    auto fsHandle = createShader(fsCode, ShaderType::Fragment);

    auto vs = vertexShaderMap[vsHandle.id];
    auto fs = pixelShaderMap[fsHandle.id];

    GraphicsHandle handle = {nextHandleId++};
    DX11ShaderProgram shaderProgram;
    shaderProgram.vertexShader = vs;
    shaderProgram.pixelShader = fs;
    shaderProgramMap[handle.id] = shaderProgram;
    return handle;

}
GraphicsHandle createVertexBuffer(void* data, int size) {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = size;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    ID3D11Buffer* vertexBuffer = nullptr;
    device->CreateBuffer(&bd, &initData, &vertexBuffer);

    GraphicsHandle handle = {nextHandleId++};
    vertexBufferMap[handle.id] = vertexBuffer;
    return  handle;


}
void bindVertexBuffer(int bufferHandle) {

}

D3D11_PRIMITIVE_TOPOLOGY getPrimitiveTopology(PrimitiveType primitiveType) {
    switch (primitiveType) {
        case PrimitiveType::TRIANGLE_LIST: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

void renderGeometry(PrimitiveType primitiveType) {
    ctx->IASetPrimitiveTopology(getPrimitiveTopology(primitiveType));
    ctx->Draw(3, 0);
}
void clear(float r, float g, float b, float a) {
    ID3D11RenderTargetView* rtvs[1] = { rtv };
    ctx->OMSetRenderTargets(1, rtvs, depthStencilView);
    //ctx->OMSetRenderTargets(1, rtvs, nullptr);
    float color[] = {r, g, b, a};
    ctx->ClearRenderTargetView(rtv, color);

    // clear our depth target as well
    ctx->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1, 0);
}

void bindShaderProgram(GraphicsHandle programHandle) {
    auto shaderProgram = shaderProgramMap[programHandle.id];
    ctx->VSSetShader(shaderProgram.vertexShader.vertexShader, nullptr, 0);
    ctx->PSSetShader(shaderProgram.pixelShader.pixelShader, nullptr, 0);
}


GraphicsHandle createIndexBuffer(void* data, int size) {
    return {};

}
GraphicsHandle createVertexArray() {
    // TODO find the good abstraction for the vertex array in DX world!
    // not really useful as it seems, so we will just create a dummy here
    DX11VertexArray* va = new DX11VertexArray();
    GraphicsHandle handle = {nextHandleId++};
    vertexArrayMap[handle.id] = va;
    return handle;
}

void associateVertexBufferWithVertexArray(GraphicsHandle vertexBuffer, GraphicsHandle vertexArray) {
    auto va = vertexArrayMap[vertexArray.id];
    va->vertexBuffers.push_back(vertexBufferMap[vertexBuffer.id]);
}

DXGI_FORMAT getDXFormatForType(DataType type, int numberOfComponents = 3) {
    switch (type) {
        case DataType::Float:
            if (numberOfComponents == 3)
                return DXGI_FORMAT_R32G32B32_FLOAT;
            if (numberOfComponents == 4)
                return DXGI_FORMAT_R32G32B32A32_FLOAT;
            if (numberOfComponents == 2)
                return DXGI_FORMAT_R32G32_FLOAT;
            if (numberOfComponents == 1)
                return DXGI_FORMAT_R32_FLOAT;

        default: return DXGI_FORMAT_R32G32B32_FLOAT;
    }
}

void associateVertexAttribute(uint32_t attributeLocation, int numberOfComponents, DataType type,
    int stride, int offset, GraphicsHandle bufferHandle, GraphicsHandle shaderProgramHandle, GraphicsHandle vertexArrayHandle) {

    auto vs = shaderProgramMap[shaderProgramHandle.id];

    ID3D11InputLayout* outLayout;
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
         { "POSITION", attributeLocation, getDXFormatForType(type), 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
     };

    auto result = device->CreateInputLayout(layoutDesc, 1, vs.vertexShader.blob->GetBufferPointer(), vs.vertexShader.blob->GetBufferSize(), &outLayout);
    assert(SUCCEEDED(result));

    auto va = vertexArrayMap[vertexArrayHandle.id];
    va->inputLayout = outLayout;

}
void bindVertexBuffer(GraphicsHandle bufferHandle) {

}
void bindVertexArray(GraphicsHandle vaoHandle) {
    auto va = vertexArrayMap[vaoHandle.id];

    uint32_t strides[] = {12};// TODO make dynamic
    uint32_t offsets[] = {0};

    ctx->IASetInputLayout(va->inputLayout);
    // The stride is not just 0, but it must be set for the actual size of the vertex.
    ctx->IASetVertexBuffers(0, va->vertexBuffers.size(), va->vertexBuffers.data(), strides, offsets);
}



#endif