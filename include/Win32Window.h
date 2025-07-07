#pragma once

#include <iostream>
#include <ostream>
#include <string>
#include <windows.h>
#include <shobjidl.h>

struct TimerToken {
    int token;
};

class Win32Window {
public:
    Win32Window(HINSTANCE hInstance, int nCmdShow, const std::wstring& title, int width, int height);

    void *getBackbufferPixels();

    void present();

    ~Win32Window();
    bool process_messages();
    HWND get_hwnd() const { return hwnd_; }

    TimerToken performance_count();

    float measure_time_in_seconds(TimerToken &startToken, TimerToken endToken);

    int getWidth();
    int getHeight();

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    HWND hwnd_ = nullptr;
    HINSTANCE hInstance_ = nullptr;

    HBITMAP backbuffer_ = nullptr;
    HDC memoryDC_;
    HDC windowDC_;
    int width_ = 800;
    int height_ = 600;
    void* backBufferPixels_ = nullptr;
    LARGE_INTEGER performanceFreq_;

};


