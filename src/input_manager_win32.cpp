//
// Created by mgrus on 03.07.2025.
//
#include <Windows.h>

extern void winConsumeLeftClick();
extern bool winLeftDoubleClick();
extern WPARAM last_key_press();
extern int winMouseX();
extern int winMouseY();
extern bool winLeftMouseUp();

// We support Windows VK_ macros here.
bool keyPressed(int key) {
    //    return GetAsyncKeyState(key) & 0x01;
    return key == last_key_press();
}

bool isKeyDown(int key) {
    return GetKeyState(key) & 0x8000;
}

bool mouseLeftDoubleClick() {
    return winLeftDoubleClick();
}

bool mouseLeftClick() {
    return winLeftMouseUp();
}

void mouseLeftClickConsumed() {
    winConsumeLeftClick();
}

int mouseX() {
    return winMouseX();
}

int mouseY() {
    return winMouseY();
}
