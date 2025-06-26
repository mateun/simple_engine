//
// Created by mgrus on 26.06.2025.
//


#ifdef RENDERER_SW

static uint32_t* get_pixels_32_for_window(Win32Window& window) {
    void* pixels = window.getBackbufferPixels();
    uint32_t* pixels32 = reinterpret_cast<uint32_t*>(pixels);
    return pixels32;
}

void setPixel(Win32Window& window, int x, int y, int color) {
    uint32_t* pixels32 = get_pixels_32_for_window(window);
    int w = window.getWidth();
    pixels32[y * w + x] = color;
}

void drawLine(Win32Window &window, int x1, int y1, int x2, int y2, int color) {


    // Find dominance of the line, as this changes the way we plot the pixels:
    auto hor = x2 - x1;
    auto ver = y2 - y1;

    if (hor < 0 || ver < 0) {
        auto t = x1;
        if (hor < 0) {
            x1 = x2;
            x2 = t;

        }
        if (ver < 0) {
            t = y1;
            y1 = y2;
            y2 = t;

        }
    }

    // Horizontal dominant
    if (abs(hor) > abs(ver)) {
        float gradient = (float)ver / (float)hor;
        float y = y1;
        for (int x = x1; x < x2; x++) {
            setPixel(window, x, y, color);
            y += gradient;
        }
    }
    // Vertical dominant
    else {
        float gradient = (float)hor / (float)ver;
        float x = x1;
        for (int y = y1; y < y2; y++) {
            setPixel(window, x, y, color);
            x += gradient;
        }
    }

}
#endif


