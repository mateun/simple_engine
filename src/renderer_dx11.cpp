//
// Created by mgrus on 26.06.2025.
//

#ifdef RENDERER_DX11
#include <cassert>
#include <engine.h>
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
static ID3D11Debug *debugger;
static ID3D11Texture2D *backBuffer;
static ID3D11RenderTargetView *rtv;
static ID3D11Texture2D *depthStencilBuffer;
static ID3D11DepthStencilView *depthStencilView;
static ID3D11DepthStencilState *m_DepthStencilState;
static ID3D11DepthStencilState * m_depthStencilStateNoDepth;
static ID3D11SamplerState *defaultSamplerState;
ID3D11RasterizerState* rasterStateSolid = nullptr;
ID3D11RasterizerState* rasterStateWireframe = nullptr;
static GraphicsHandle defaultTextShaderProgram = {};

struct BufferWrapper {
    ID3D11Buffer* buffer;
    uint32_t stride = 0;
    uint32_t offset = 0;
};

struct DX11VertexArray {
    std::vector<BufferWrapper> vertexBuffers;
    ID3D11Buffer* indexBuffer = nullptr;
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

struct DX11Texture {
    ID3D11Texture2D* texture;
    ID3D11ShaderResourceView* srv;
    ID3D11RenderTargetView* rtv;
    ID3D11DepthStencilView* dsv;
};

struct OffscreenRenderTarget {
    ID3D11Texture2D* texture = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11Texture2D* depthTexture = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    int width = 0;
    int height = 0;

    void release(); // cleanup
};



static int nextHandleId = 0;
static std::map<int, DX11Shader> pixelShaderMap;
static std::map<int, DX11Shader> vertexShaderMap;
static std::map<int, DX11ShaderProgram> shaderProgramMap;
static std::map<int, BufferWrapper> vertexBufferMap;
static std::map<int, BufferWrapper> constantBufferMap;
static std::map<int, ID3D11Buffer*> indexBufferMap;
static std::map<int, ID3D11InputLayout*> inputLayoutMap;
static std::map<int, DX11VertexArray*> vertexArrayMap;
static std::map<int, DX11Texture> textureMap;
static std::map<int, OffscreenRenderTarget> frameBufferMap;




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
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // ← THIS is where SRGB comes in
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    result = device->CreateRenderTargetView(backBuffer, &rtvDesc, &rtv);
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
        OutputDebugString(L"failed to create depth stencil state\n");
        exit(1);
    }

    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    result = device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilStateNoDepth);
    if (FAILED(result)) {
        OutputDebugString(L"failed to create depth stencil state\n");
        exit(1);
    }

    ctx->OMSetDepthStencilState(m_DepthStencilState, 0);

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_BACK;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rsDesc, &rasterStateSolid);

    rsDesc.FillMode = D3D11_FILL_WIREFRAME;
    device->CreateRasterizerState(&rsDesc, &rasterStateWireframe);
    ctx->RSSetState(rasterStateSolid);



    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = (FLOAT)w;
    vp.Height = (FLOAT)h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    // Setup default sampler state

    D3D11_SAMPLER_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&sd, &defaultSamplerState);
    if (FAILED(hr)) {
        exit(1);
    }

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable           = TRUE;
    blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* blendState = nullptr;
    device->CreateBlendState(&blendDesc, &blendState);

    // Then set it before drawing:
    float blendFactor[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(blendState, blendFactor, 0xffffffff);
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

DXGI_FORMAT getFormatByChannelNumber(int num_channels) {
    switch (num_channels) {
        case 1: return DXGI_FORMAT_R8_UNORM;
        case 2: return DXGI_FORMAT_R8G8_UNORM;
        case 3:
        case 4: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}

GraphicsHandle createTexture(int width, int height, uint8_t *pixels, uint8_t num_channels) {
    assert(pixels != nullptr);
    ID3D11Texture2D *texture;
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
    desc.Width = width;
    desc.Height = height;
    desc.Format = getFormatByChannelNumber(num_channels);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = pixels;
    initialData.SysMemPitch = width * num_channels;        // This is only true for 4 byte colors
    initialData.SysMemSlicePitch = 0;

    HRESULT res = device->CreateTexture2D(&desc, &initialData, &texture);
    if (FAILED(res)) {
        std::cerr << "texture creation failed" << std::endl;
        exit(1);
    }

    ID3D11ShaderResourceView *srv = nullptr;
    res = device->CreateShaderResourceView(texture, NULL, &srv);
    assert(SUCCEEDED(res));

    GraphicsHandle handle = {nextHandleId++};
    textureMap[handle.id] = {texture, srv, nullptr, nullptr };  // Currently we are only dealing with "standard" textures here
    return handle;
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

void updateBuffer(GraphicsHandle bufferHandle, BufferType bufferType, void* data, uint32_t size_in_bytes) {
    ID3D11Buffer* buffer = nullptr;
    if (bufferType == BufferType::Vertex) {
        buffer = vertexBufferMap[bufferHandle.id].buffer;
    } else if (bufferType == BufferType::Index) {
        buffer = indexBufferMap[bufferHandle.id];
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, data, size_in_bytes);
    ctx->Unmap(buffer, 0);

}

void setFillMode(FillMode mode) {
    if (mode == FillMode::Wireframe) {
        ctx->RSSetState(rasterStateWireframe);
    }
    else if (mode == FillMode::Solid) {
        ctx->RSSetState(rasterStateSolid);
    }
}

void setDepthTesting(bool on) {
    if (on) {
        ctx->OMSetDepthStencilState(m_DepthStencilState, 0);
    } else {
        ctx->OMSetDepthStencilState(m_depthStencilStateNoDepth, 0);
    }
}

void setViewport(int originX, int originY, int width, int height) {
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = originX;
    vp.TopLeftY = originY;
    vp.Width = (FLOAT) width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

D3D11_USAGE dx11UsageFromBufferUsage(BufferUsage bufferUsage) {
    switch (bufferUsage) {
        case BufferUsage::Static: return D3D11_USAGE_DEFAULT;
        case BufferUsage::Dynamic: return D3D11_USAGE_DYNAMIC;
        default: return D3D11_USAGE_DEFAULT;
    }
}

GraphicsHandle createVertexBuffer(void* data, int size, uint32_t stride, BufferUsage bufferUsage) {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = dx11UsageFromBufferUsage(bufferUsage);
    bd.ByteWidth = size;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = bufferUsage == BufferUsage::Dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    ID3D11Buffer* vertexBuffer = nullptr;
    device->CreateBuffer(&bd, &initData, &vertexBuffer);

    GraphicsHandle handle = {nextHandleId++};
    vertexBufferMap[handle.id] = {vertexBuffer, stride};
    return  handle;


}

GraphicsHandle createConstantBuffer(uint32_t size) {
    ID3D11Buffer* buffer = nullptr;
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = size;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    HRESULT hr = device->CreateBuffer(&desc, nullptr, &buffer);
    if (FAILED(hr)) {
        printf("Failed to create object buffer\n");
        exit(1234);
    }
    GraphicsHandle handle = {nextHandleId++};
    constantBufferMap[handle.id] = BufferWrapper {buffer, size, 0};
    return handle;
}

void bindVertexBuffer(int bufferHandle) {

}

D3D11_PRIMITIVE_TOPOLOGY getPrimitiveTopology(PrimitiveType primitiveType) {
    switch (primitiveType) {
        case PrimitiveType::TRIANGLE_LIST: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

void renderGeometryIndexed(PrimitiveType primitiveType, int count, int startIndex) {
    ctx->IASetPrimitiveTopology(getPrimitiveTopology(primitiveType));
    ctx->DrawIndexed(count, startIndex, 0);
}

void renderGeometry(PrimitiveType primitiveType) {
    ctx->IASetPrimitiveTopology(getPrimitiveTopology(primitiveType));
    ctx->Draw(3, 0);
}

void clearFrameBuffer(GraphicsHandle fbHandle, float r, float g, float b, float a) {
    auto fb = frameBufferMap[fbHandle.id];
    //ctx->OMSetRenderTargets(1, &fb.rtv, fb.dsv);
    float color[] = {r, g, b, a};
    ctx->ClearRenderTargetView(fb.rtv, color);
    ctx->ClearDepthStencilView(fb.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

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



void bindTexture(GraphicsHandle textureHandle, uint8_t slot) {
    auto texture = textureMap[textureHandle.id];
    ctx->PSSetShaderResources(slot, 1, &texture.srv);
    ctx->PSSetSamplers(slot, 1, &defaultSamplerState);
}

void bindDefaultBackbuffer(int originX, int originY, int width, int height) {
    ctx->OMSetRenderTargets(1, &rtv, depthStencilView);
    setViewport(originX, originY, width, height);
}

void uploadConstantBufferData(GraphicsHandle bufferHandle, void *data, uint32_t size_in_bytes,  uint32_t bufferSlot) {
    D3D11_MAPPED_SUBRESOURCE mapped;
    auto buffer = constantBufferMap[bufferHandle.id].buffer;

    ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, data, size_in_bytes);
    ctx->Unmap(buffer, 0);
    ID3D11Buffer* buffers[] = { buffer };
    ctx->VSSetConstantBuffers(bufferSlot, 1, buffers);

}


GraphicsHandle createIndexBuffer(void* data, int size, BufferUsage bufferUsage) {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = dx11UsageFromBufferUsage(bufferUsage);
    bd.ByteWidth = size;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = bufferUsage == BufferUsage::Dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    ID3D11Buffer* buf = nullptr;
    device->CreateBuffer(&bd, &initData, &buf);
    GraphicsHandle handle = {nextHandleId++};
    indexBufferMap[handle.id] = buf;
    return handle;

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

void associateIndexBufferWithVertexArray(GraphicsHandle indexBuffer, GraphicsHandle vertexArray) {
    auto va = vertexArrayMap[vertexArray.id];
    va->indexBuffer = indexBufferMap[indexBuffer.id];
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

void describeVertexAttributes(std::vector<VertexAttributeDescription>& attributeDescriptions,  GraphicsHandle bufferHandle, GraphicsHandle shaderProgramHandle, GraphicsHandle vertexArrayHandle) {

    auto vs = shaderProgramMap[shaderProgramHandle.id];

    ID3D11InputLayout* outLayout;
    std::vector<D3D11_INPUT_ELEMENT_DESC> layoutDescs;
    std::vector<std::string> savedStrings;
    for (auto& vad : attributeDescriptions) {
        savedStrings.push_back(vad.semanticName);
    }
    int counter = 0;
    for (auto& vad : attributeDescriptions) {
        auto desc = D3D11_INPUT_ELEMENT_DESC { savedStrings[counter].c_str(), 0, getDXFormatForType(vad.type, vad.numberOfComponents),
            vad.attributeLocation, vad.offset, D3D11_INPUT_PER_VERTEX_DATA, 0 };
        layoutDescs.push_back(desc);
        counter++;

    }

    auto result = device->CreateInputLayout(layoutDescs.data(),
        layoutDescs.size(),
        vs.vertexShader.blob->GetBufferPointer(),
        vs.vertexShader.blob->GetBufferSize(),
        &outLayout);
    if(FAILED(result)) {
        _com_error err(result);
        std::wcerr << L"creation of input layout failed: " << _com_error(result).ErrorMessage() << std::endl;
    }

    auto va = vertexArrayMap[vertexArrayHandle.id];
    va->inputLayout = outLayout;

}
void bindVertexBuffer(GraphicsHandle bufferHandle) {

}
void bindVertexArray(GraphicsHandle vaoHandle) {
    auto va = vertexArrayMap[vaoHandle.id];

    std::vector<uint32_t> strides;
    std::vector<uint32_t> offsets;
    std::vector<ID3D11Buffer*> vbs;

    for (auto vb : va->vertexBuffers) {
        strides.push_back(vb.stride);
        offsets.push_back(vb.offset);
        vbs.push_back(vb.buffer);
    }

    ctx->IASetInputLayout(va->inputLayout);
    // The stride is not just 0, but it must be set for the actual size of the vertex.
    ctx->IASetVertexBuffers(0, va->vertexBuffers.size(), vbs.data(), strides.data(), offsets.data());
    if (va->indexBuffer) {
        ctx->IASetIndexBuffer(va->indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }
}




GraphicsHandle getDefaultTextShaderProgram() {

    static std::string text_vshader_hlsl = R"(
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

    static std::string text_fs_hlsl = R"(
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

    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        defaultTextShaderProgram = createShaderProgram(text_vshader_hlsl, text_fs_hlsl);
    } else {
        return defaultTextShaderProgram;
    }

}

void bindFrameBuffer(GraphicsHandle frameBufferHandle, int viewportOriginX, int viewportOriginY, int viewportWidth, int viewportHeight) {
    auto fb = frameBufferMap[frameBufferHandle.id];
    ctx->OMSetRenderTargets(1, &fb.rtv, fb.dsv);
    setViewport(viewportOriginX, viewportOriginY, viewportWidth, viewportHeight);

}

void bindFrameBufferTexture(GraphicsHandle frameBufferHandle, int slot) {
    auto fb = frameBufferMap[frameBufferHandle.id];

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ctx->PSSetShaderResources(slot, 1, nullSRV);

    ctx->PSSetShaderResources(slot, 1, &fb.srv);
}



GraphicsHandle createFrameBuffer(int width, int height, bool includeDepthBuffer) {
    OffscreenRenderTarget rt;
    rt.width = width;
    rt.height = height;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    device->CreateTexture2D(&desc, nullptr, &rt.texture);
    device->CreateRenderTargetView(rt.texture, nullptr, &rt.rtv);
    device->CreateShaderResourceView(rt.texture, nullptr, &rt.srv);

    if (includeDepthBuffer) {
        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        auto result = device->CreateTexture2D(&depthDesc, nullptr, &rt.depthTexture);
        if (FAILED(result)) {
            exit(1);
        }

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = depthDesc.Format;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
        result = device->CreateDepthStencilView(rt.depthTexture, &dsvDesc, &rt.dsv);
        if (FAILED(result)) {
            exit(1);
        }
    }

    GraphicsHandle handle = {nextHandleId++};
    frameBufferMap[handle.id] = rt;
    return handle;

}


#endif
