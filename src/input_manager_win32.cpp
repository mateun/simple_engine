//
// Created by mgrus on 03.07.2025.
//
#include <Windows.h>

extern WPARAM last_key_press();

// We support Windows VK_ macros here.
bool keyPressed(int key) {
    //    return GetAsyncKeyState(key) & 0x01;
    return key == last_key_press();
}

bool isKeyDown(int key) {
    return GetKeyState(key) & 0x8000;
}
