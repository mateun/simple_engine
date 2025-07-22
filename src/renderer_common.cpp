//
// Created by mgrus on 28.06.2025.
//


#include <map>

#include "engine.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <assimp/anim.h>
#include <assimp/vector3.h>

#include "engine.h"
#include "engine.h"
#include "engine.h"
#include "engine.h"
#include "glm/detail/type_quat.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <filesystem>

static int nextHandleId = 0;
static std::map<int, Font> fontMap;

Mesh * MeshData::toMesh() {
    auto mesh = new Mesh;
    mesh->index_count = indicesFlat.size();
    mesh->meshVertexArray = createVertexArray();
    mesh->materialName = materialName;
    mesh->diffuseTexturePath = diffuseTexturePath;
    if (diffuseTexture.id != -1) {
        mesh->diffuseTexture = diffuseTexture;
    } else {
        mesh->diffuseTexture = diffuseTexturePath.empty() ? GraphicsHandle{-1} : createTextureFromFile(diffuseTexturePath);
    }

    mesh->normalMapPath = normalMapPath;

    // Decide on the vertex layout.
    // For skeletal meshes with animations we have to also add blendweights
    // and blend indices.
    // Now we assemble our pos|uv vertex layout:
    std::vector<float> vertexList;
    if (skeleton != nullptr && skeleton->animations.size() > 0) {
        // The "animated one", with extra blend weights and blend  indices.
        for (int i = 0; i < posMasterList.size(); i++) {
            vertexList.push_back(posMasterList[i].x);
            vertexList.push_back(posMasterList[i].y);
            vertexList.push_back(posMasterList[i].z);

            vertexList.push_back(uvMasterList[i].x);
            vertexList.push_back(uvMasterList[i].y);

            vertexList.push_back(jointWeights[i].x);
            vertexList.push_back(jointWeights[i].y);
            vertexList.push_back(jointWeights[i].z);
            vertexList.push_back(jointWeights[i].w);

            vertexList.push_back(jointIndices[i].x);
            vertexList.push_back(jointIndices[i].y);
            vertexList.push_back(jointIndices[i].z);
            vertexList.push_back(jointIndices[i].w);
        }
    } else {
        // "default" layout, for now it is only position and uvs.
        for (int i = 0; i < posMasterList.size(); i++) {
            vertexList.push_back(posMasterList[i].x);
            vertexList.push_back(posMasterList[i].y);
            vertexList.push_back(posMasterList[i].z);
            vertexList.push_back(uvMasterList[i].x);
            vertexList.push_back(uvMasterList[i].y);
        }
    }

    // Also need different vertex buffer in case of skeletal animations:
    if (skeleton != nullptr && skeleton->animations.size() > 0) {
        mesh->meshVertexBuffer = createVertexBuffer(vertexList.data(), vertexList.size() * sizeof(float), sizeof(float) * 13, BufferUsage::Static );
    } else {
        mesh->meshVertexBuffer = createVertexBuffer(vertexList.data(), vertexList.size() * sizeof(float), sizeof(float) * 5);
    }

    associateVertexBufferWithVertexArray(mesh->meshVertexBuffer, mesh->meshVertexArray);
    mesh->meshIndexBuffer = createIndexBuffer(indicesFlat.data(), indicesFlat.size() * sizeof(uint32_t));
    associateIndexBufferWithVertexArray( mesh->meshIndexBuffer, mesh->meshVertexArray);
    mesh->skeleton = skeleton;
    mesh->positions = posMasterList;
    mesh->uvs = uvMasterList;
    mesh->jointIndices = jointIndices;
    mesh->jointWeights = jointWeights;
    mesh->name = meshName;

    return mesh;
}


StartEndKeyFrame getStartEndKeyFrameForTime(float time, Animation* animation, KeyFrameType type, std::string jointName) {
    auto jointAnimationTrack = animation->jointAnimationTracks[jointName];
    auto tps = animation->ticksPerSecond;



    auto extractPositions = [jointAnimationTrack, time, tps] () -> std::pair<aiVectorKey, aiVectorKey> {
        std::pair<aiVectorKey, aiVectorKey> defaultResult = {jointAnimationTrack.positionKeys[0], jointAnimationTrack.positionKeys[0]};

        for (int i = 0; i < jointAnimationTrack.positionKeys.size()-1; i++) {
            aiVectorKey kf = jointAnimationTrack.positionKeys[i];
            aiVectorKey kfPlusOne = jointAnimationTrack.positionKeys[i +1];
            auto timeFrom = kf.mTime / tps;
            auto timeTo = kfPlusOne.mTime / tps;
            if (time >= timeFrom && time < timeTo) {
                return std::make_pair(kf, kfPlusOne);
            }
        }

        return defaultResult;
    };

    auto extractRotations = [jointAnimationTrack, time, tps] () -> std::pair<aiQuatKey, aiQuatKey> {
        std::pair<aiQuatKey, aiQuatKey> defaultResult = {jointAnimationTrack.rotationKeys[0], jointAnimationTrack.rotationKeys[0]};

        for (int i = 0; i < jointAnimationTrack.rotationKeys.size()-1; i++) {
            aiQuatKey kf = jointAnimationTrack.rotationKeys[i];
            aiQuatKey kfPlusOne = jointAnimationTrack.rotationKeys[i +1];
            if (time >= (kf.mTime / tps) && time < (kfPlusOne.mTime / tps)) {
                return std::make_pair(kf, kfPlusOne);
            }
        }

        return defaultResult;
    };


    auto extractScalings = [jointAnimationTrack, time, tps] () -> std::pair<aiVectorKey, aiVectorKey> {
        std::pair<aiVectorKey, aiVectorKey> defaultResult = {jointAnimationTrack.scaleKeys[0], jointAnimationTrack.scaleKeys[0]};
        for (int i = 0; i < jointAnimationTrack.scaleKeys.size()-1; i++) {
            aiVectorKey kf = jointAnimationTrack.scaleKeys[i];
            aiVectorKey kfPlusOne = jointAnimationTrack.scaleKeys[i +1];
            if (time >= (kf.mTime / tps)  && time < (kfPlusOne.mTime / tps)) {
                return std::make_pair(kf, kfPlusOne);
            }
        }
        return defaultResult;
    };

    if (type == KeyFrameType::Translation) {
        StartEndKeyFrame se ={};
        se.posKeys= extractPositions();
        return se;
    }

    if (type == KeyFrameType::Rotation) {
        StartEndKeyFrame se ={};
        se.rotKeys= extractRotations();
        return se;
    }

    if (type == KeyFrameType::Scale) {
        StartEndKeyFrame se ={};
        se.scaleKeys= extractScalings();
        return se;
    }

}

glm::vec3 Camera::getForward() {
    return glm::normalize(lookAtTarget - location);
}

glm::vec3 Camera::getUp() {
    return normalize(glm::cross(getRight(), getForward()));
}

glm::mat4 Camera::updateAndGetViewMatrix() {
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

FrameTimer::FrameTimer(size_t maxSamples) : maxSamples_(maxSamples){

}

void FrameTimer::addFrameTime(float dt) {
    frameTimes.push_back(dt);
    sum += dt;

    if (frameTimes.size() > maxSamples_) {
        sum -= frameTimes.front();
        frameTimes.pop_front();
    }
}

float FrameTimer::getAverage() const {
    if (frameTimes.empty()) return 0.0f;
    return sum / frameTimes.size();
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

float findWidestMenuItemSize(std::vector<MenuItem*>& menuItems, GraphicsHandle fontHandle) {
    float widest = std::numeric_limits<float>::min();
    for (auto menuItem : menuItems) {
        auto bb = measureText(fontHandle, menuItem->name);
        auto width = bb.right - bb.left;
        widest = std::max<float>(widest, width);
    }
    return widest;
}

BoundingBox measureText(GraphicsHandle fontHandle, const std::string& text) {
    auto font = fontMap[fontHandle.id];
    float penX = 0, penY = 0;
    float minX =  std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float minY =  std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
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

        // Track min/max for bounding box
        minX = std::min(minX, q.x0);
        maxX = std::max(maxX, q.x1);

        if (c == 32) continue; // ignore space for Y, as this is always zero and messes things up.
        minY = std::min(minY, q.y0); // lowest part (descenders)
        minY = std::min(minY, q.y1);

        maxY = std::max(maxY, q.y0); // highest part (ascenders)
        maxY = std::max(maxY, q.y1);
    }
    return BoundingBox(minX, minY, maxX, maxY);
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

            // Positions
            meshData->posMasterList.push_back(glm::vec3(q.x0, q.y0, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x1, q.y0, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x1, q.y1, 0));
            meshData->posMasterList.push_back(glm::vec3(q.x0, q.y1, 0));

            float flipped_y0 = baseline - q.y1;
            float flipped_y1 = baseline - q.y0;
            // meshData->posMasterList.push_back(glm::vec3(q.x0, flipped_y0, 0));
            // meshData->posMasterList.push_back(glm::vec3(q.x1, flipped_y0, 0));
            // meshData->posMasterList.push_back(glm::vec3(q.x1, flipped_y1, 0));
            // meshData->posMasterList.push_back(glm::vec3(q.x0, flipped_y1, 0));


            // UVS
            meshData->uvMasterList.push_back({q.s0, q.t0});
            meshData->uvMasterList.push_back({q.s1, q.t0});
            meshData->uvMasterList.push_back({q.s1, q.t1});
            meshData->uvMasterList.push_back({q.s0, q.t1});

            // Flip vertical uv coordinates
            // meshData->uvMasterList.push_back({q.s0, q.t1});
            // meshData->uvMasterList.push_back({q.s1, q.t1});
            // meshData->uvMasterList.push_back({q.s1, q.t0});
            // meshData->uvMasterList.push_back({q.s0, q.t0});

            //---------------------------------------
            // INDICES
            int offset = charCounter * 4;
            meshData->indicesFlat.push_back(2 + offset);meshData->indicesFlat.push_back(1 + offset);meshData->indicesFlat.push_back(0 + offset);
            meshData->indicesFlat.push_back(2 + offset);meshData->indicesFlat.push_back(0 + offset);meshData->indicesFlat.push_back(3 + offset);

            // Flipped
            // meshData->indicesFlat.push_back(0 + offset);meshData->indicesFlat.push_back(1 + offset);meshData->indicesFlat.push_back(2 + offset);
            // meshData->indicesFlat.push_back(3 + offset);meshData->indicesFlat.push_back(0 + offset);meshData->indicesFlat.push_back(2 + offset);
            // ----------------------------------------------------------------------------------

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
    textMesh->creation_index_count = textData->indicesFlat.size();
    textMesh->meshVertexArray = createVertexArray();
    textMesh->meshVertexBuffer = createVertexBuffer(textVertexList.data(), textVertexList.size() * sizeof(float), sizeof(float) * 5, BufferUsage::Dynamic);
    associateVertexBufferWithVertexArray(textMesh->meshVertexBuffer, textMesh->meshVertexArray);
    textMesh->meshIndexBuffer = createIndexBuffer(textData->indicesFlat.data(), textData->indicesFlat.size() * sizeof(uint32_t), BufferUsage::Dynamic);
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
    auto oldSize = textMesh.index_count;

    auto textData = renderTextIntoQuadGeometry(fontHandle, text);
    std::vector<float> textVertexList;
    for (int i = 0; i < textData->posMasterList.size(); i++) {
        textVertexList.push_back(textData->posMasterList[i].x);
        textVertexList.push_back(textData->posMasterList[i].y);
        textVertexList.push_back(textData->posMasterList[i].z);
        textVertexList.push_back(textData->uvMasterList[i].x);
        textVertexList.push_back(textData->uvMasterList[i].y);
    }

    // TODO we should replace this with a function which checks against the actual
    // original vertex buffer size. Because, as long as we are within these bounds, we are fine.
    auto new_index_count = textData->indicesFlat.size();
    if (new_index_count > textMesh.creation_index_count) {
         exit(1);
    }
    textMesh.index_count = textData->indicesFlat.size();
    updateBuffer(textMesh.meshVertexBuffer, BufferType::Vertex, textVertexList.data(), textVertexList.size() * sizeof(float));
    updateBuffer(textMesh.meshIndexBuffer, BufferType::Index, textData->indicesFlat.data(), textData->indicesFlat.size() * sizeof(uint32_t));


}

glm::quat aiToGLM(aiQuaternion aiQuat) {
     return glm::quat(aiQuat.w, aiQuat.x, aiQuat.y, aiQuat.z);
}

glm::vec3 aiToGLM(aiVector3D v) {
    return glm::vec3(v.x, v.y, v.z);
}

std::string fileNameFromPath(const std::string &filePath) {
    auto fp = std::filesystem::path(filePath);
    return fp.filename().string();

}
