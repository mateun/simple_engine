//
// Created by mgrus on 28.06.2025.
//


#include <map>

#include "engine.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "engine.h"
#include "engine.h"
#include "engine.h"
#include "engine.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

static int nextHandleId = 0;
static std::map<int, Font> fontMap;

Mesh * MeshData::toMesh() {
    auto mesh = new Mesh;
    mesh->index_count = indicesFlat.size();
    mesh->meshVertexArray = createVertexArray();

    // Now we assemble our pos|uv vertex layout:
    std::vector<float> vertexList;
    for (int i = 0; i < posMasterList.size(); i++) {
        vertexList.push_back(posMasterList[i].x);
        vertexList.push_back(posMasterList[i].y);
        vertexList.push_back(posMasterList[i].z);
        vertexList.push_back(uvMasterList[i].x);
        vertexList.push_back(uvMasterList[i].y);
    }
    mesh->meshVertexBuffer = createVertexBuffer(vertexList.data(), vertexList.size() * sizeof(float), sizeof(float) * 5);
    associateVertexBufferWithVertexArray(mesh->meshVertexBuffer, mesh->meshVertexArray);
    mesh->meshIndexBuffer = createIndexBuffer(indicesFlat.data(), indicesFlat.size() * sizeof(uint32_t));
    associateIndexBufferWithVertexArray( mesh->meshIndexBuffer, mesh->meshVertexArray);
    mesh->skeleton = skeleton;

    return mesh;
}

glm::vec3 Camera::getForward() {
    return glm::normalize(lookAtTarget - location);
}

glm::vec3 Camera::getUp() {
    return normalize(glm::cross(getRight(), getForward()));
}

glm::mat4 Camera::updatenAndGetViewMatrix() {
    view_matrix = glm::lookAtLH(location,lookAtTarget, glm::vec3(0, 1, 0));
    return view_matrix;

}

glm::mat4 Camera::updateAndGetPerspectiveProjectionMatrix(float fovInDegrees, int width, int height, float nearPlane, float farPlane) {
    projection_matrix = glm::perspectiveFovLH_ZO<float>(glm::radians(fovInDegrees), width,
            height, nearPlane, farPlane);
    return projection_matrix;
}


glm::vec3 Camera::getRight() {
    return glm::normalize(glm::cross(getForward(), {0, 1, 0}));
}

void * Camera::matrixBufferPtr() {
    return &(this->view_matrix);
}

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

GraphicsHandle getTextureFromFont(GraphicsHandle fontHandle) {
    auto font = fontMap[fontHandle.id];
    return font.atlasTexture;
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
            float tempPenY = 0;
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font.bakedChars.data(), 512, 512, c - 32, &penX, &tempPenY, &q, 1);

            float pixel_aligned_x0 = std::floor(q.x0 + 0.5f);
            float pixel_aligned_y0 = std::floor(q.y0 + 0.5f);
            float pixel_aligned_x1 = std::floor(q.x1 + 0.5f);
            float pixel_aligned_y1 = std::floor(q.y1 + 0.5f);

            q.x0 = pixel_aligned_x0;
            q.y0 = pixel_aligned_y0;
            // q.y0 = -12;
            // q.y0 = 0;
            q.x1 = pixel_aligned_x1;
            q.y1 = pixel_aligned_y1;
            // q.y1 = 12;

            // meshData->posMasterList.push_back(glm::vec3(q.x0, q.y0, 0));
            // meshData->posMasterList.push_back(glm::vec3(q.x1, q.y0, 0));
            // meshData->posMasterList.push_back(glm::vec3(q.x1, q.y1, 0));
            // meshData->posMasterList.push_back(glm::vec3(q.x0, q.y1, 0));



            float flipped_y0 = baseline - q.y1;
            float flipped_y1 = baseline - q.y0;

            meshData->posMasterList.push_back(glm::vec3(q.x0, flipped_y0, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x1, flipped_y0, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x1, flipped_y1, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x0, flipped_y1, 0));


            // meshData->uvMasterList.push_back({q.s0, q.t0});
            // meshData->uvMasterList.push_back({q.s1, q.t0});
            // meshData->uvMasterList.push_back({q.s1, q.t1});
            // meshData->uvMasterList.push_back({q.s0, q.t1});

            // Flip vertical uv coordinates
            meshData->uvMasterList.push_back({q.s0, q.t1});
            meshData->uvMasterList.push_back({q.s1, q.t1});
            meshData->uvMasterList.push_back({q.s1, q.t0});
            meshData->uvMasterList.push_back({q.s0, q.t0});


            int offset = charCounter * 4;
            meshData->indicesFlat.push_back(2 + offset);meshData->indicesFlat.push_back(1 + offset);meshData->indicesFlat.push_back(0 + offset);
            meshData->indicesFlat.push_back(2 + offset);meshData->indicesFlat.push_back(0 + offset);meshData->indicesFlat.push_back(3 + offset);

            // Flipped
            // meshData->indicesFlat.push_back(0 + offset);meshData->indicesFlat.push_back(1 + offset);meshData->indicesFlat.push_back(2 + offset);
            // meshData->indicesFlat.push_back(3 + offset);meshData->indicesFlat.push_back(0 + offset);meshData->indicesFlat.push_back(2 + offset);
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

Mesh * createTextMesh(GraphicsHandle fontHandle, const std::string &text) {
    auto textData = renderTextIntoQuadGeometry(fontHandle, text);
    std::vector<float> textVertexList;
    for (int i = 0; i < textData->posMasterList.size(); i++) {
        textVertexList.push_back(textData->posMasterList[i].x);
        textVertexList.push_back(textData->posMasterList[i].y);
        textVertexList.push_back(textData->posMasterList[i].z);
        textVertexList.push_back(textData->uvMasterList[i].x);
        textVertexList.push_back(textData->uvMasterList[i].y);
    }
    Mesh *textMesh = new Mesh();
    textMesh->index_count = textData->indicesFlat.size();
    textMesh->meshVertexArray = createVertexArray();
    textMesh->meshVertexBuffer = createVertexBuffer(textVertexList.data(), textVertexList.size() * sizeof(float) * 2, sizeof(float) * 5, BufferUsage::Dynamic);
    associateVertexBufferWithVertexArray(textMesh->meshVertexBuffer, textMesh->meshVertexArray);
    textMesh->meshIndexBuffer = createIndexBuffer(textData->indicesFlat.data(), textData->indicesFlat.size() * sizeof(uint32_t) * 2, BufferUsage::Dynamic);
    associateIndexBufferWithVertexArray( textMesh->meshIndexBuffer, textMesh->meshVertexArray);
    // Our default 2D attributes, position and texture:
    std::vector<VertexAttributeDescription> vertexAttributes =  {
        {"POSITION", 0, 3, DataType::Float, sizeof(float) * 5,
            0 },

        {"TEXCOORD", 0, 2, DataType::Float, sizeof(float) * 5,
            sizeof(float) * 3 }
    };
    describeVertexAttributes(vertexAttributes, textMesh->meshVertexBuffer, getDefaultTextShaderProgram(), textMesh->meshVertexArray);
    return textMesh;
}

void updateText(Mesh &textMesh, GraphicsHandle fontHandle, const std::string &text) {
    auto oldSize = textMesh.index_count * sizeof(uint32_t) * 2;

    auto textData = renderTextIntoQuadGeometry(fontHandle, text);
    std::vector<float> textVertexList;
    for (int i = 0; i < textData->posMasterList.size(); i++) {
        textVertexList.push_back(textData->posMasterList[i].x);
        textVertexList.push_back(textData->posMasterList[i].y);
        textVertexList.push_back(textData->posMasterList[i].z);
        textVertexList.push_back(textData->uvMasterList[i].x);
        textVertexList.push_back(textData->uvMasterList[i].y);
    }
    auto newSize = textData->indicesFlat.size() * sizeof(uint32_t);
    if (newSize > oldSize) {
        exit(1);
    }
    textMesh.index_count = textData->indicesFlat.size();
    updateBuffer(textMesh.meshVertexBuffer, BufferType::Vertex, textVertexList.data(), textVertexList.size() * sizeof(float));
    updateBuffer(textMesh.meshIndexBuffer, BufferType::Index, textData->indicesFlat.data(), textData->indicesFlat.size() * sizeof(uint32_t));


}
