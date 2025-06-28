//
// Created by mgrus on 28.06.2025.
//


#include <map>

#include "engine.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

static int nextHandleId = 0;
static std::map<int, Font> fontMap;

GraphicsHandle createFont(const std::string& fontName, int fontSize) {
        // Read font file
        FILE *fp = fopen(fontName.c_str(), "rb");
        if (!fp) {
            fprintf(stderr, "Failed to open TTF file.\n");
            // TODO
            //throw std::runtime_error("Failed to open TTF file.");
        }
        fseek(fp, 0, SEEK_END);
        int size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char *ttf_buffer = new unsigned char[size];
        fread(ttf_buffer, 1, size, fp);
        fclose(fp);

        // Image atlasImage = {
        //     .pixels = new uint8_t[512 * 512],
        //     .width = 512,
        //     .height = 512,
        //     .channels = 1,
        //
        // };

        // Retrieve font measurements
        stbtt_fontinfo info;
        stbtt_InitFont(&info, ttf_buffer, 0);

        Font font;

        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        float scale = stbtt_ScaleForPixelHeight(&info, fontSize);
        auto scaled_ascent_  = ascent  * scale;  // typically a positive number
        auto scaled_descent_ = descent * scale;  // typically negative
        auto scaled_line_gap_ = lineGap * scale;


        uint8_t* pixels = new uint8_t[512 * 512];
        int width = 512, height = 512;
        font.baseLine = scaled_ascent_;
        font.lineHeight = (scaled_ascent_ - scaled_descent_) + scaled_line_gap_;
        font.bakedChars.resize(96);
        int result = stbtt_BakeFontBitmap(ttf_buffer, 0, fontSize,
                                      pixels, width, height,
                                      32, 96, font.bakedChars.data());

        if (result <= 0) {
            fprintf(stderr, "Failed to bake font bitmap.\n");
            delete[] ttf_buffer;
            // TODO error case?!
        }

        auto atlasTexture = createTexture(width, height, pixels, 1);
        font.atlasTexture = atlasTexture;

        GraphicsHandle fontHandle = {nextHandleId++};
        fontMap[fontHandle.id] = font;
        return fontHandle;

}


MeshData* renderTextIntoQuadGeometry(GraphicsHandle fontHandle, const std::string& text) {

        MeshData* meshData = new MeshData();
        auto font = fontMap[fontHandle.id];

        float penX = 0, penY = 0;
        float minX =  std::numeric_limits<float>::max();
        float maxX = -std::numeric_limits<float>::max();
        float minY =  std::numeric_limits<float>::max();
        float maxY = -std::numeric_limits<float>::max();
        float baseline = font.baseLine;
        int charCounter = 0;
        for (auto c : text) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font.bakedChars.data(), 512, 512, c - 32, &penX, &penY, &q, 0);

            float pixel_aligned_x0 = std::floor(q.x0 + 0.5f);
            float pixel_aligned_y0 = std::floor(q.y0 + 0.5f);
            float pixel_aligned_x1 = std::floor(q.x1 + 0.5f);
            float pixel_aligned_y1 = std::floor(q.y1 + 0.5f);

            q.x0 = pixel_aligned_x0;
            q.y0 = pixel_aligned_y0;
            q.x1 = pixel_aligned_x1;
            q.y1 = pixel_aligned_y1;

            meshData->posMasterList.push_back(glm::vec3(q.x0, q.y0, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x1, q.y0, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x1, q.y1, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x0, q.y1, 0));


            meshData->uvMasterList.push_back({q.s0, q.t0});
            meshData->uvMasterList.push_back({q.s1, q.t0});
            meshData->uvMasterList.push_back({q.s1, q.t1});
            meshData->uvMasterList.push_back({q.s0, q.t1});

            // Flip vertical uv coordinates
            // uvs.push_back({q.s0, q.t1});
            // uvs.push_back({q.s1, q.t1});
            // uvs.push_back({q.s1, q.t0});
            // uvs.push_back({q.s0, q.t0});


            int offset = charCounter * 4;
            // outIndices.push_back(2 + offset);outIndices.push_back(1 + offset);outIndices.push_back(0 + offset);
            // outIndices.push_back(2 + offset);outIndices.push_back(0 + offset);outIndices.push_back(3 + offset);

            // Flipped
            meshData->indicesFlat.push_back(0 + offset);meshData->indicesFlat.push_back(1 + offset);meshData->indicesFlat.push_back(2 + offset);
            meshData->indicesFlat.push_back(3 + offset);meshData->indicesFlat.push_back(0 + offset);meshData->indicesFlat.push_back(2 + offset);
            charCounter++;

           // Track min/max for bounding box
            minX = std::min(minX, q.x0);
            maxX = std::max(maxX, q.x1);

            if (c == 32) continue; // ignore space for Y, as this is always zero and messes things up.
            minY = std::min(minY, q.y0); // lowest part (descenders)
            minY = std::min(minY, q.y1);

            maxY = std::max(maxY, q.y0); // highest part (ascenders)
            maxY = std::max(maxY, q.y1);
        }

        return meshData;

    }
