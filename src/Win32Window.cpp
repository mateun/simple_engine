#include "Win32Window.h"

#include <map>
#include <string>
#include <windowsx.h>

static bool in_size_move = false;
static bool lbuttonDown = false;
static bool lbuttonUp = false;
static bool leftDoubleClick = false;
static int mouse_x = 0;
static int mouse_y = 0;
static bool useMouse = true;
static int resized_width = 0;
static int resized_height = 0;
static int timer_token_counter = 0;
std::map<int, LARGE_INTEGER> timer_tokens;
static WPARAM last_key_press_ = 0;

bool winLeftDoubleClick() {
    return leftDoubleClick;
}

bool winLeftMouseUp() {
    return lbuttonUp;
}

int winMouseX() {
    return mouse_x;
}

int winMouseY() {
    return mouse_y;
}

int resizedWidth() {
    return resized_width;
}

int resizedHeight() {
    return resized_height;
}



WPARAM last_key_press() {
    return last_key_press_;
}

TimerToken Win32Window::performance_count() {

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    timer_tokens[timer_token_counter++] = counter;
    return {timer_token_counter - 1};
}

float Win32Window::measure_time_in_seconds(TimerToken& startToken, TimerToken endToken) {
    auto start = timer_tokens[startToken.token];
    auto end = timer_tokens[endToken.token];
    return (end.QuadPart - start.QuadPart) / (float)performanceFreq_.QuadPart;
}

int Win32Window::getWidth() {
    return width_;
}

int Win32Window::getHeight() {
    return height_;
}

LRESULT CALLBACK Win32Window::wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            last_key_press_ = wParam;
            break;

        case WM_MOUSEMOVE:
            if (useMouse) {
                mouse_x = GET_X_LPARAM(lParam);
                mouse_y = GET_Y_LPARAM(lParam);
            }
            break;

        case WM_SIZE:

            resized_width = LOWORD(lParam);
            resized_height = HIWORD(lParam);


            // switch (wParam) {
            //     case SIZE_RESTORED:
            //         // Handle normal resize
            //
            //         break;
            //     case SIZE_MINIMIZED:
            //         // Handle minimization
            //
            //         break;
            //     case SIZE_MAXIMIZED:
            //         // Handle maximization
            //
            //         break;
            // }
            break;

        case WM_ENTERSIZEMOVE:
            in_size_move = true;
            break;

        case WM_EXITSIZEMOVE:
            in_size_move = false;
            break;

        case WM_LBUTTONDBLCLK:
            leftDoubleClick = true;
            break;

        case WM_LBUTTONDOWN:
            lbuttonDown = true;
            break;

        case WM_LBUTTONUP:
            lbuttonUp = true;
            lbuttonDown = false;
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

Win32Window::Win32Window(HINSTANCE hInstance, int nCmdShow, const std::wstring& title, int width, int height) : hInstance_(hInstance),
    width_(width), height_(height) {
    resized_width = width;
    resized_height = height;

    const wchar_t CLASS_NAME[] = L"SampleWin32WindowClass";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = Win32Window::wnd_proc;
    wc.hInstance = hInstance_;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassExW(&wc);

    // Adjust window size to have the desired size as the client area.
    // So we must make the win32 window a bit bigger normally, to have
    // the desired size fully available to the application:
    RECT clientRect = {0, 0, width, height};
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&clientRect, style, FALSE); // FALSE = no menu bar
    int windowWidth = clientRect.right - clientRect.left;
    int windowHeight = clientRect.bottom - clientRect.top;

    hwnd_ = CreateWindowEx(
        0,
        CLASS_NAME,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
        nullptr,
        nullptr,
        hInstance_,
        nullptr
    );

    if (hwnd_) {
        ShowWindow(hwnd_, nCmdShow);
        UpdateWindow(hwnd_);
    }


    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;

    backbuffer_ = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &backBufferPixels_, nullptr, 0);

    memoryDC_ = CreateCompatibleDC(nullptr);
    SelectObject(memoryDC_, backbuffer_);

    uint32_t* pixels32 = reinterpret_cast<uint32_t*>(backBufferPixels_);
    // for (int i = 0; i < 100; i++) {
    //     pixels32[10 * width + 10 + i] = 0xFFFFFFFF;
    // }


    windowDC_ = GetDC(hwnd_);
    QueryPerformanceFrequency(&performanceFreq_);

}



void* Win32Window::getBackbufferPixels() {
    return backBufferPixels_;
}

void Win32Window::present() {
    BitBlt(windowDC_, 0, 0, width_, height_, memoryDC_, 0, 0, SRCCOPY);
}

Win32Window::~Win32Window() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

bool Win32Window::process_messages() {
    last_key_press_ = 0;
    lbuttonDown = false;
    lbuttonUp = false;
    leftDoubleClick = false;

    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}


std::wstring showFileDialog(const std::wstring& typeFilter) {
    OPENFILENAME ofn;       // Common dialog box structure
    wchar_t szFile[260] = {0}; // Buffer for file name
    HWND hwnd = NULL;       // Owner window
    HANDLE hf;              // File handle

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = typeFilter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display the Open dialog box
    if (GetOpenFileName(&ofn) == TRUE) {
        std::wcout << "Selected file: " << ofn.lpstrFile << std::endl;
        return ofn.lpstrFile;

    } else {
        std::cout << "No file selected or operation canceled." << std::endl;
        return L"";
    }
}