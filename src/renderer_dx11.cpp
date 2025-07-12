//
// Created by mgrus on 26.06.2025.
//

#ifdef RENDERER_DX11
#include <cassert>
#include <engine.h>
#include <Windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <iostream>
#include <map>
#include <vector>
#include <ostream>
#include <stb_image.h>
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

static ID3D11Device *device;
static ID3D11DeviceContext *ctx;
static IDXGISwapChain *swapChain;
static IDXGISwapChain1 *swapChain1;
static ID3D11Debug *debugger;
static ID3D11Texture2D *backBuffer;
static ID3D11RenderTargetView *rtv;
static ID3D11Texture2D *depthStencilBuffer;
static ID3D11DepthStencilView *depthStencilView;
static ID3D11DepthStencilState *m_DepthStencilState;
static ID3D11DepthStencilState * m_depthStencilStateNoDepth;
static ID3D11SamplerState *defaultSamplerState;
static ID3D11BlendState* opaqueBlendState = nullptr;
static ID3D11BlendState* blendState = nullptr;
static float blendFactor[4] = { 0, 0, 0, 0 };
static ID3D11RasterizerState* rasterStateSolid = nullptr;
static ID3D11RasterizerState* rasterStateWireframe = nullptr;
static ID3D11RasterizerState * rasterStateFrontCounter = nullptr;
static GraphicsHandle defaultTextShaderProgram = {};

static GraphicsHandle thumbnailFrameBuffer = {-1};

struct OffscreenRenderTarget {
    ID3D11Texture2D* texture = nullptr;
    GraphicsHandle textureHandle = {-1};
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11Texture2D* depthTexture = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    int width = 0;
    int height = 0;

    void release(); // cleanup
};


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
static std::map<int, ID3DUserDefinedAnnotation*> annotationMap;


void createDefaultDepthStencilBuffer(int width, int height) {

    // Create a depth/stencil buffer
    D3D11_TEXTURE2D_DESC td;
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_D32_FLOAT;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;

    auto result = device->CreateTexture2D(&td, 0, &depthStencilBuffer);
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
}

void PrintDXGIError(HRESULT hr) {
    LPWSTR errorText = nullptr;
    DWORD result = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&errorText), 0, nullptr );
    if (result > 0)
    {
        // errorText contains the description of the error code hr
        std::wcout << "DXGI Error: " << errorText << std::endl;
        LocalFree( errorText );
    }
    else
    {
        // Error not known by the OS
    }
}

void createSwapChain(HWND hwnd, int width, int height) {
    IDXGIDevice * pDXGIDevice = nullptr;
    auto result = device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);

    IDXGIAdapter * pDXGIAdapter = nullptr;
    result = pDXGIDevice->GetAdapter(&pDXGIAdapter);


    IDXGIOutput* pOutput = nullptr;
    result = pDXGIAdapter->EnumOutputs(0, &pOutput);
    if (SUCCEEDED(result)) {
        UINT numModes = 0;
        // First, get the number of modes
        result = pOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, &numModes, nullptr);
        if (SUCCEEDED(result) && numModes > 0) {
            std::vector<DXGI_MODE_DESC> modeList(numModes);
            // Retrieve the full list
            result = pOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, &numModes, modeList.data());
            if (SUCCEEDED(result)) {
                std::cout << "Supported _SRGB modes found: " << numModes << std::endl;
                bool modeFound = false;
                for (const auto& mode : modeList) {

                    // Check for exact or close match
                    if (mode.Width == width && mode.Height == height &&
                        mode.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
                        mode.RefreshRate.Numerator / (float)mode.RefreshRate.Denominator >= 59.0f &&
                        mode.RefreshRate.Numerator / (float)mode.RefreshRate.Denominator <= 61.0f) {
                        modeFound = true;
                        std::cout << "Matching mode found!" << std::endl;
                        std::cout << "Width: " << mode.Width << ", Height: " << mode.Height
                              << ", Refresh: " << mode.RefreshRate.Numerator << "/"
                              << mode.RefreshRate.Denominator << std::endl;
                        // Update fsdesc with this mode if needed
                        //sd.RefreshRate = mode.RefreshRate;
                        break;
                        }
                }
                if (!modeFound) {
                    std::cout << "No exact match for " << width << "x" << height << " at ~60 Hz" << std::endl;
                }
            } else {
                std::cout << "GetDisplayModeList failed: 0x" << std::hex << result << std::endl;
            }
        } else {
            std::cout << "Format not supported or no modes: 0x" << std::hex << result << std::endl;
        }
        pOutput->Release();
    }


    IDXGIFactory2* pFactory2 = nullptr;
    auto hr= pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pFactory2);
    if (FAILED(hr)) {
        exit(1);
    }

    IDXGIFactory* pFactory = nullptr;
    result = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pFactory);
    if (FAILED(result)) {
        exit(1);
    }

    // Dxfactory1 creation code:
    {
        DXGI_SWAP_CHAIN_DESC sd;
        sd.BufferDesc.Width  = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        sd.SampleDesc.Count   = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount  = 1;
        sd.OutputWindow = hwnd;
        sd.Windowed     = true;
        sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
        sd.Flags        = 0;
        result = pFactory->CreateSwapChain(device, &sd, &swapChain );
        if (FAILED(result)) {
            PrintDXGIError(result);
            exit(2);
        }
    }

    std::cout << "hwnd: " << hwnd << std::endl;
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        std::cout << "Invalid or hidden window" << std::endl;
        exit(1);
    }

#ifdef USE_FLIP_DISCARD_EFFECT
    {
        // These are descriptions needed for a new swapchain1 specified SC:
        DXGI_SWAP_CHAIN_DESC1 scdesc1 = {};
        scdesc1.Width = width;
        scdesc1.Height = height;
        scdesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scdesc1.Stereo = FALSE;
        scdesc1.SampleDesc.Count = 1;
        scdesc1.SampleDesc.Quality = 0;
        scdesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scdesc1.BufferCount = 2;
        scdesc1.Scaling = DXGI_SCALING_NONE;
        scdesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scdesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        scdesc1.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsdesc = {};
        fsdesc.Windowed = TRUE;
        fsdesc.RefreshRate.Numerator = 60;
        fsdesc.RefreshRate.Denominator = 1;
        fsdesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        fsdesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

        result = pFactory2->CreateSwapChainForHwnd(device, hwnd, &scdesc1, &fsdesc, nullptr, &swapChain1);
        if (FAILED(result)) {
            std::cout << "error creating swapchain 0x" << std::hex << result << std::endl;
            PrintDXGIError(result);
            exit(1);
        }

        // Set sRGB color space
        IDXGISwapChain3* swapChain3;
        result = swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain3));
        if (SUCCEEDED(result)) {
            result = swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709); // sRGB
            if (FAILED(result)) {
                std::cout << "Failed to set color space: 0x" << std::hex << result << std::endl;
            }
        }
        pFactory2->Release();
    }
#endif


    pFactory->Release();
    pDXGIAdapter->Release();
    pDXGIDevice->Release();

    resizeSwapChain(hwnd, width, height);

}


void initGraphics(Win32Window& window, bool msaa, int msaa_samples) {
    int w = window.getWidth();
    int h = window.getHeight();
    //GetWindowClientSize(hwnd, w, h);
    D3D_FEATURE_LEVEL featureLevels =  D3D_FEATURE_LEVEL_11_1;
    UINT flags = 0;
#ifdef _DEBUG_BUILD
    flags = D3D11_CREATE_DEVICE_DEBUG;;
#endif
    HRESULT result = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, &featureLevels, 1, D3D11_SDK_VERSION,
                                       &device, NULL, &ctx);
    if (FAILED(result)) {
        std::cerr << "CreateDevice failed" << std::endl;
        exit(1);
    }

    // Gather the debug interface
    debugger = 0;
    if (flags == D3D11_CREATE_DEVICE_DEBUG) {
        result = device->QueryInterface(__uuidof(ID3D11Debug), (void**)&debugger);
        if (FAILED(result)) {
            OutputDebugString(L"debuger creation failed\n");
            exit(1);
        }
    }



    UINT ql;
    device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &ql);
    assert(ql > 0);


    createSwapChain(window.get_hwnd(), w, h);


    // Prepare some rasterizer states
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rsDesc, &rasterStateSolid);

    rsDesc.FillMode = D3D11_FILL_WIREFRAME;
    device->CreateRasterizerState(&rsDesc, &rasterStateWireframe);
    ctx->RSSetState(rasterStateSolid);

    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.FrontCounterClockwise = TRUE;
    device->CreateRasterizerState(&rsDesc, &rasterStateFrontCounter);

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
    result = device->CreateSamplerState(&sd, &defaultSamplerState);
    if (FAILED(result)) {
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
    device->CreateBlendState(&blendDesc, &blendState);
    ctx->OMSetBlendState(blendState, blendFactor, 0xffffffff);

    blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;


    device->CreateBlendState(&blendDesc, &opaqueBlendState);
}
#include <comdef.h>
void present() {
    auto sc = swapChain ? swapChain : swapChain1;
    auto hr = sc->Present(0, 0);
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
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    D3D11_SUBRESOURCE_DATA initialData = {};

    if (pixels) {
        initialData.pSysMem = pixels;
        initialData.SysMemPitch = width * num_channels;        // This is only true for 4 byte colors
        initialData.SysMemSlicePitch = 0;
    }

    HRESULT res = 0;
    if (pixels) {
         res = device->CreateTexture2D(&desc, &initialData, &texture);
    } else {
        res = device->CreateTexture2D(&desc, nullptr, &texture);
    }


    if (FAILED(res)) {
        std::cerr << "texture creation failed" << std::endl;
        exit(1);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = getFormatByChannelNumber(num_channels);;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView *srv = nullptr;
    res = device->CreateShaderResourceView(texture, &srvDesc, &srv);
    assert(SUCCEEDED(res));

    GraphicsHandle handle = {nextHandleId++};
    textureMap[handle.id] = {texture, srv, nullptr, nullptr };  // Currently we are only dealing with "standard" textures here
    return handle;
}

GraphicsHandle createTextureFromFile(const std::string &texturePath) {
    int imageChannels, width, height;

    auto pixels = stbi_load(
            texturePath.c_str(), &width, &height,
            &imageChannels,
            4);

    return createTexture(width, height, pixels, imageChannels);
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

void resizeSwapChain(HWND hwnd, int width, int height) {


    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullsrvs[16] = {nullptr};
    ctx->VSSetShaderResources(0, 16, nullsrvs);
    ctx->PSSetShaderResources(0, 16, nullsrvs);
    ctx->CSSetShaderResources(0, 16, nullsrvs);
    ID3D11UnorderedAccessView* nullUAVs[8] = { nullptr };
    ctx->CSSetUnorderedAccessViews(0, 8, nullUAVs, nullptr);

    if (rtv) {
        rtv->Release();
        rtv = nullptr;
    }
    if (depthStencilView) {
        depthStencilView->Release();
        depthStencilView = nullptr;
    }

    if (depthStencilBuffer) {
        depthStencilBuffer->Release();
        depthStencilBuffer = nullptr;
    }

    auto sc = swapChain ? swapChain : swapChain1;
    auto hr = sc->Present(0, 0);
    if (FAILED(hr)) {
        exit(2);
    }

    // width = 1280;
    // height = 720;

    DXGI_SWAP_CHAIN_DESC desc;

    sc->GetDesc(&desc);
    std::cout << "Swap chain state: BufferCount=" << desc.BufferCount
              << ", Format=" << desc.BufferDesc.Format
              << ", Windowed=" << (desc.Windowed ? "Yes" : "No")
              << ", Flags=" << desc.Flags << std::endl;

    DXGI_MODE_DESC modeDesc = {};
    modeDesc.Width = width;
    modeDesc.Height = height;
    modeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    modeDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    modeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    auto result = sc->ResizeTarget(&modeDesc);
    if (FAILED(result)) {
        std::cout << "backbuffer target resizing failed" << std::to_string(result) << std::endl;
        exit(1);
    }

    result = sc->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result)) {
        std::cout << "backbuffer resizing on swapchain resizing failed" << std::to_string(result) << std::endl;
        exit(1);
    }

    result = sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(result)) {
        std::cout << "backbuffer creation/retrieval on swapchain resizing failed" << std::endl;
        exit(1);
    }

    // Step 4: Create new render target view
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    result = device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release(); // Release temporary back buffer pointer
    if (FAILED(result)) {
        std::cout << "Render target view creation failed" << std::endl;
        exit(1);
    }

    createDefaultDepthStencilBuffer(width, height);

    setViewport(0, 0, width, height);


}

void resizeFrameBuffer(GraphicsHandle fbHandle, int width, int height) {
    auto old_fb = frameBufferMap[fbHandle.id];
    if (old_fb.texture) {
        old_fb.texture->Release();
        old_fb.texture = nullptr;
    }

    if (old_fb.rtv) {
        old_fb.rtv->Release();
        old_fb.rtv = nullptr;
    }

    auto newHandle = createFrameBuffer(width, height, true);
    auto new_fb = frameBufferMap[newHandle.id];
    frameBufferMap[fbHandle.id] = new_fb;
}

void copyTexture(GraphicsHandle targetTexture, GraphicsHandle sourceTexture) {
    auto target = textureMap[targetTexture.id];
    auto source = textureMap[sourceTexture.id];
    ctx->CopyResource(target.texture, source.texture);
}

void setFrontCulling(bool front) {
    if (front) {
        ctx->RSSetState(rasterStateFrontCounter);
    } else {
        ctx->RSSetState(rasterStateSolid);
    }
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

void enableBlending(bool enable) {
    if (enable) {
        ctx->OMSetBlendState(blendState, blendFactor, 0xffffffff);
    } else {
        ctx->OMSetBlendState(opaqueBlendState, nullptr, 0xffffffff);
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

    if (data == nullptr && size > 0) {
        std::cerr << "Null data pointer with non-zero size" << std::endl;
        exit(1);
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(data, &mbi, sizeof(mbi))) {
        if (mbi.Protect == PAGE_NOACCESS) {
            std::cerr << "Data pointer points to no-access memory" << std::endl;
            exit(1);
        }
    }

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = dx11UsageFromBufferUsage(bufferUsage);
    bd.ByteWidth = size;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = bufferUsage == BufferUsage::Dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    ID3D11Buffer* vertexBuffer = nullptr;
    auto result = device->CreateBuffer(&bd, &initData, &vertexBuffer);
    if (FAILED(result)) {
        std::cerr << "Failed to create vertex buffer: " << std::hex << result << std::endl;
        if (result == DXGI_ERROR_DEVICE_REMOVED && device) {
            HRESULT hr = device->GetDeviceRemovedReason();
            std::cerr << "Device removed reason: " << std::hex << hr << std::endl;
        }
        exit(1);
    }

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
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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

    rt.textureHandle = { nextHandleId++ };
    textureMap[rt.textureHandle.id] = { rt.texture, rt.srv, rt.rtv, rt.dsv};

    GraphicsHandle handle = {nextHandleId++};
    frameBufferMap[handle.id] = rt;
    return handle;

}


GraphicsHandle getTextureFromFrameBuffer(GraphicsHandle frameBufferHandle) {
    auto fb = frameBufferMap[frameBufferHandle.id];
    return fb.textureHandle;
}



GraphicsHandle beginRenderAnnotation(std::wstring name) {
    ID3DUserDefinedAnnotation* annotation;
    ctx->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                       reinterpret_cast<void**>(&annotation));
    annotation->BeginEvent(name.c_str());

    GraphicsHandle handle = {nextHandleId++};
    annotationMap[handle.id] = annotation;
    return handle;
}

void endRenderAnnotation(GraphicsHandle annotationHandle) {
    auto annotation = annotationMap[annotationHandle.id];
    annotation->EndEvent();
}

GraphicsHandle createThumbnailForMesh(std::vector<MeshData*> meshDataItems, GraphicsHandle shaderProgramHandle,
    GraphicsHandle objectTransformBuffer, GraphicsHandle cameraTransformBuffer, std::vector<VertexAttributeDescription> vertexAttributes,
     int width, int height) {

    ID3DUserDefinedAnnotation* annotation;
    ctx->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                       reinterpret_cast<void**>(&annotation));
    annotation->BeginEvent(L"Thumbnail rendering");

    setFrontCulling(false);

    if (thumbnailFrameBuffer.id == -1) {
        thumbnailFrameBuffer = createFrameBuffer(width, height, true);
    }

    bindShaderProgram(shaderProgramHandle);
    bindFrameBuffer(thumbnailFrameBuffer, 0, 0, width, height);
    clearFrameBuffer(thumbnailFrameBuffer, .0, .0, 0.0, 1);

    auto thumbnailCamera = new Camera();
    thumbnailCamera->location = {0, 2, -2};
    thumbnailCamera->lookAtTarget = {0, 0, 3};
    thumbnailCamera->view_matrix =thumbnailCamera->updateAndGetViewMatrix();
    thumbnailCamera->projection_matrix = thumbnailCamera->updateAndGetPerspectiveProjectionMatrix(65,
        width, height, 0.1, 100);
    uploadConstantBufferData( cameraTransformBuffer, thumbnailCamera->matrixBufferPtr(), sizeof(glm::mat4) * 2, 1);

    // Render each (sub)-mesh into our common thumbnail buffer:
    for (auto meshData : meshDataItems) {
        auto mesh = meshData->toMesh();
        describeVertexAttributes(vertexAttributes, mesh->meshVertexBuffer, shaderProgramHandle, mesh->meshVertexArray);
        bindVertexArray(mesh->meshVertexArray);


        if (mesh->diffuseTexture.id != -1) {
            bindTexture(mesh->diffuseTexture, 0);
        }

        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f),{0, 0, 0}) * scaleMatrix;
        uploadConstantBufferData( objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, mesh->index_count, 0);
    }


    ID3D11Query* query;
    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_EVENT;
    device->CreateQuery(&queryDesc, &query);




    auto fbTexture = getTextureFromFrameBuffer(thumbnailFrameBuffer);
    // auto targetTexture = createTextureFromFile("assets/debug_texture.jpg");
    auto targetTexture = createTexture(width, height, nullptr, 4);
    ID3D11DeviceChild* tex = textureMap[targetTexture.id].texture;
    auto name = "thumbnailTexture";
    tex->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("thumbnailTexture") + 1, name);
    copyTexture(targetTexture, fbTexture);
    ctx->Flush();

    ctx->End(query);

    BOOL queryData;
    while (ctx->GetData(query, &queryData, sizeof(BOOL), 0) == S_FALSE) {
        // Wait for GPU to complete
    }
    query->Release();
    annotation->EndEvent();

    return targetTexture;

}


#endif
