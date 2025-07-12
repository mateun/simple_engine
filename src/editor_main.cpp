#include <iostream>
#include <Win32Window.h>
#include <engine.h>
#define GRAPHICS_IMPLEMENTATION
#include <engine.h>
#include "editor.h"
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <codecvt>
#include <d3d11_1.h>
#include <ranges>
#include <stb_image.h>
#include <assimp/anim.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "glm/gtx/quaternion.hpp"

static float frame_time = 0.0f;

void renderLevelEditor(int originX, int originY, int width, int height, EditorState & editorState, Level* meshGroup);
void renderAnimationPanel(int originX, int originY, int width, int height, EditorState & editorState, Mesh* mesh);
void render3DScenePanel(int originX, int originY, int width, int height, EditorState & editorState);
void blitFramebufferToScreen(GraphicsHandle framebuffer, EditorState& editorState, int width, int height, int originX, int originY, float depth);

void clear_screen(int width, int height, Win32Window& window, uint32_t *pixels32) {
    auto start_clear = window.performance_count();
    memset(pixels32, 0, width * height * 4);
    auto end_clear = window.performance_count();
    auto clear_time = window.measure_time_in_seconds(start_clear, end_clear);
#ifdef _PERF_MEASURE
    std::cout << "clear time: " << (clear_time * 1000.0f) << " ms" << std::endl;
#endif

}

#ifdef RENDERER_SW
void do_sw_frame(Win32Window& window) {
    clear_screen(window.getWidth(), window.getHeight(), window, (uint32_t*)window.getBackbufferPixels());

    static float offset = 0;
    offset += 8.0f * frame_time;
    for (int i = 0; i < 100; i++) {
        setPixel(window, 10 + i + offset, 10 , 0xFFFFF00);
    }

    drawLine(window, 100, 100, 118, 80, 0xFFFF0000);
    drawLine(window, 100, 100, 118, 120, 0xFFFF0000);
    drawLine(window, 118, 120, 128, 80, 0xFFFF0000);
    drawLine(window, 100, 100, 128, 80, 0xFFFF0000);
    window.present();
}

#endif

void update_camera(EditorState& gameState) {
    float movementSpeed = 10.0f;
    float camspeed = movementSpeed * frame_time;
    float rotspeed = movementSpeed * frame_time;
    float dir = 0;
    float hdir = 0;
    float yaw = 0;
    float pitch = 0;

    // This allows moving fwd/bwd in the current plane.
    // So even if you look down pitched, you would still move level.
    float panFwd = 0;
    if (isKeyDown('E')) {
        yaw = 1;
    }
    if (isKeyDown('Q')) {
        yaw = -1;
    }

    if (isKeyDown('W')) {
        dir = 1;
    }
    if (isKeyDown('S')) {
        dir = -1;
    }
    if (isKeyDown('A')) {
        hdir = 1;
    }
    if (isKeyDown('D')) {
        hdir = -1;
    }

    auto fwd = gameState.perspectiveCamera->getForward();

    auto up = gameState.perspectiveCamera->getUp();

    glm::vec4 fwdAfterYaw = glm::rotate(glm::mat4(1), yaw * rotspeed * 0.1f, up) * glm::vec4(fwd, 0);

    auto locCandidate = gameState.perspectiveCamera->location + glm::vec3{camspeed * fwdAfterYaw.x, 0, camspeed * fwdAfterYaw.z} * dir;

    auto right = gameState.perspectiveCamera->getRight();
    locCandidate += glm::vec3{camspeed * right.x * 0.33, 0, camspeed * right.z * 0.33} * hdir;

    // Do any checks if needed.
    gameState.perspectiveCamera->location = locCandidate;
    gameState.perspectiveCamera->lookAtTarget = locCandidate + glm::normalize(glm::vec3{fwdAfterYaw.x, fwdAfterYaw.y, fwdAfterYaw.z});
    gameState.perspectiveCamera->updateAndGetViewMatrix();

}

Camera activateOrthoCameraForPanel(int width, int height, EditorState& editorState) {
    // Specific projection cam for this panel:
    Camera cam;
    cam.view_matrix = glm::mat4(1);
    cam.projection_matrix = glm::orthoLH_ZO<float>(0.0f, (float) width, (float) height, 0.0f, 0.0, 30);
    uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, cam.matrixBufferPtr(), sizeof(Camera), 1);
    return cam;
}

void renderTopMenu(int originX, int originY, int width, int height, EditorState& editorState) {

    // Render topmenu
    setFrontCulling(true);
    bindVertexArray(editorState.graphics.quadVertexArray);
    bindFrameBuffer(editorState.graphics.frameBufferTopMenu, 0, 0, width, height);
    clearFrameBuffer(editorState.graphics.frameBufferTopMenu, 0.1, .1, 0.11, 1);

    // Specific projection cam for this panel:
    Camera cam;
    cam.view_matrix = glm::mat4(1);
    cam.projection_matrix = glm::orthoLH_ZO<float>(0.0f, (float) width, (float) height, 0.0f, 0.0, 30);
    uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, cam.matrixBufferPtr(), sizeof(Camera), 1);

    // Render actual menu items
    enableBlending(true);
    int accumulatedTextLength = 0;
    for (auto menuItem : editorState.topMenuItems) {

        int hOffset = 8 + accumulatedTextLength;
        auto textMesh = menuItem->textMesh;
        auto textBB = measureText(editorState.graphics.fontHandleBig, menuItem->name);
        auto itemWidth = textBB.right - textBB.left;
        accumulatedTextLength += itemWidth + 24;

        // The background rectangle for hovering effect:
        enableBlending(false);
        bindVertexArray(editorState.graphics.quadVertexArray);
        bindShaderProgram(editorState.graphics.shaderProgram);
        if (editorState.currentHoverMenuItem == menuItem) {
            bindTexture(editorState.texturePool["mid_blue_bg"], 0);
            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(itemWidth+16, 32, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(hOffset-4 + (itemWidth/2) + 4, 16, 2)) * scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
        }

        enableBlending(true);
        bindShaderProgram(editorState.graphics.textShaderProgram);
        bindVertexArray(textMesh->meshVertexArray);
        bindTexture(getTextureFromFont(editorState.graphics.fontHandleBig), 0);
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(hOffset, 20, 1)) *  scaleMatrix;
        menuItem->renderBoundingBox = {(float)hOffset-4, 0, (float)hOffset-4 + itemWidth + 16, 32};
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
    }




    // Should be obsolete:
    // Import Model menu item:
    // textMesh = editorState.menuTextMeshes.tmModelImport;
    // bindVertexArray(textMesh->meshVertexArray);
    // scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
    // worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(64, 24, 1)) *  scaleMatrix;
    // uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
    // renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);

    blitFramebufferToScreen(editorState.graphics.frameBufferTopMenu, editorState, width, height, originX, originY, 0.9);

}

void renderGameObjectsRecursive(EditorState & editorState, GameObject* rootGameObject, int hOffsetVarying) {
    static int invocationCounter = 0;
    if (rootGameObject == nullptr) {
        rootGameObject = editorState.rootGameObject;
        invocationCounter = 0;
    }

    int hOffset = 24;
    int vOffset = 64;
    for (auto go : rootGameObject->children) {
        enableBlending(true);
        updateText(*editorState.textMesh, editorState.graphics.fontHandle, go->name);
        bindVertexArray(editorState.textMesh->meshVertexArray);
        bindShaderProgram(editorState.graphics.textShaderProgram);
        bindTexture(getTextureFromFont(editorState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(hOffset + hOffsetVarying * 16, vOffset + invocationCounter* 24, 1)) *  scaleMatrix;
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, editorState.textMesh->index_count, 0);

        // Render a background rect for selected items
        if (editorState.currentSelectedGameObjectTreeItem == go) {
            enableBlending(true);
            bindVertexArray(editorState.graphics.quadVertexArray);
            bindShaderProgram(editorState.graphics.shaderProgram);
            bindTexture(editorState.texturePool["gray_bg"], 0);
            auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(200, 24, 1));
            worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(100, (vOffset+ invocationCounter * 24) -4 , 2)) * rotationMatrix * scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);

        }

        // Render an expansion icon
        // for now just dummy rects.
        if (go->children.size() > 0) {
            enableBlending(true);
            bindVertexArray(editorState.graphics.quadVertexArray);
            bindShaderProgram(editorState.graphics.shaderProgram);
            if (go->expandedInTree) {
                bindTexture(editorState.texturePool["expand_active"], 0);
            } else {
                bindTexture(editorState.texturePool["expand_start"], 0);
            }
            auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(12, 12, 1));
            worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(12 + hOffsetVarying * 16, (vOffset+ invocationCounter * 24) -4 , 1)) * rotationMatrix * scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);

            go->treeBoundingBox = {(float)hOffsetVarying * 16, -8 + (float)vOffset + invocationCounter * 24,
                (float)hOffsetVarying * 16 + 24, (float) (vOffset + invocationCounter * 24) + 8};

            editorState.visibleGameObjectsWithChildrenTreeItems.push_back(go);
        }



        editorState.visibleGameObjectTreeItems.push_back(go);

        // Now all the things are rendered for one tree item, we can increment the invocationCount,
        // so we make sure following renders are moved vertically down.
        invocationCounter++;


        // Only recurse if this node is set to expand:
        if (go->expandedInTree) {
            renderGameObjectsRecursive(editorState, go, hOffsetVarying+1);
        }
    }

}

void renderGameObjectTree(int originX, int originY, int width, int height, EditorState& editorState) {
        setFrontCulling(true);
        bindVertexArray(editorState.graphics.quadVertexArray);
        bindShaderProgram(editorState.graphics.shaderProgram);
        bindFrameBuffer(editorState.graphics.frameBufferGameObjectTree, 0, 0, width, height);
        clearFrameBuffer(editorState.graphics.frameBufferGameObjectTree, .1, .1, 0.1, 1);
        bindTexture(editorState.graphics.textureHandle, 0);
        Camera sceneTreeCamera;
        sceneTreeCamera.view_matrix = glm::mat4(1);
        sceneTreeCamera.projection_matrix = glm::orthoLH_ZO<float>(0.0f, (float) width, (float) height, 0.0f, 0.0, 30);
        uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, sceneTreeCamera.matrixBufferPtr(), sizeof(Camera), 1);


        // Scroll "protection" behind the title:
        {
            bindVertexArray(editorState.graphics.quadVertexArray);
            bindShaderProgram(editorState.graphics.shaderProgram);
            bindTexture(editorState.texturePool["gray_bg"], 0);
            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(200, 32, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(100, 16 , 1.8)) * scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
        }


        enableBlending(true);
        // Render title
        bindShaderProgram(editorState.graphics.textShaderProgram);
        auto textMesh = editorState.textMesh;
        updateText(*textMesh, editorState.graphics.fontHandle, "Game Objects");
        bindVertexArray(textMesh->meshVertexArray);
        bindTexture(getTextureFromFont(editorState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(8, 20, 1)) *  scaleMatrix;
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
        // End text rendering

        // Render the actual tree for all game objects
        editorState.visibleGameObjectTreeItems.clear();
        editorState.visibleGameObjectsWithChildrenTreeItems.clear();
        renderGameObjectsRecursive(editorState, nullptr, 0);

        blitFramebufferToScreen(editorState.graphics.frameBufferGameObjectTree, editorState, width, height, originX, originY, 2);

}


Tab* findTabByTitle(const std::vector<Tab *> & tabs, const std::string & title) {
    for (auto tab : tabs) {
        if (tab->tabHeader.title == title) {
            return tab;
        }
    }

    return nullptr;
}

void renderGrid(GraphicsHandle targetFrameBuffer, EditorState& editorState, int viewport_width, int viewport_height, int lineCount, GraphicsHandle gridVertexArray) {
    setFrontCulling(false);

    bindFrameBuffer(targetFrameBuffer, 0, 0, viewport_width, viewport_height);
    uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.perspectiveCamera->matrixBufferPtr(), sizeof(glm::mat4) * 2, 1);

    bindShaderProgram(editorState.graphics.lineShaderProgram);
    bindVertexArray(gridVertexArray);
    auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
    auto worldMatrix = glm::translate(glm::mat4(1.0f), {0, 0, 0}) * scaleMatrix;
    uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
    renderGeometryIndexed(PrimitiveType::LINE_LIST, lineCount * 4, 0);

}

void renderMeshEditor(int originX, int originY, int width, int height, EditorState & editorState, MeshGroup* meshGroup) {
    setFrontCulling(false);

    bindFrameBuffer(editorState.graphics.frameBuffer3DPanel, 0, 0, width, height);
    clearFrameBuffer(editorState.graphics.frameBuffer3DPanel, .0, .0, 0.0, 1);

    renderGrid(editorState.graphics.frameBuffer3DPanel, editorState, width, height, editorState.meshEditorGridLines, editorState.graphics.meshEditorGridVertexArray);

    // Animation timing
    static float animationTime = 0.0f;
    animationTime += frame_time;

    // Render each mesh of this group:
    for (auto mesh : meshGroup->meshes) {

        // Skeleton/joint posing:
        if (mesh->skeleton != nullptr) {
            if (mesh->skeleton->animations.size() > 0) {
                // We need a different shader here, which knows our attributes for blend-weights, blend-indices and the
                // matrix palette cbuffer.
                bindShaderProgram(editorState.graphics.animatedShaderProgram);

                // Run the first animation here
                auto animation = mesh->skeleton->animations[0];
                if (animationTime >= animation->duration) {
                    std::cout << "Animation looping" << std::to_string(animationTime) << std::endl;
                    animationTime = 0.0f;
                }
                for (auto joint : mesh->skeleton->joints) {
                    auto translationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Translation, joint->name);
                    auto rotationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Rotation, joint->name);
                    auto scalingKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Scale, joint->name);

                    // Translation interpolation:
                    float t0 = translationKeys.posKeys.first.mTime / animation->ticksPerSecond;;
                    float t1 = translationKeys.posKeys.second.mTime / animation->ticksPerSecond;
                    glm::vec3 p0 = aiToGLM(translationKeys.posKeys.first.mValue);
                    glm::vec3 p1 = aiToGLM(translationKeys.posKeys.second.mValue);
                    float t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    glm::vec3 p = glm::mix(p0, p1, t);

                    // Rotation interpolation
                    t0 = rotationKeys.rotKeys.first.mTime / animation->ticksPerSecond;
                    t1 = rotationKeys.rotKeys.second.mTime / animation->ticksPerSecond;
                    glm::quat q0 = aiToGLM(rotationKeys.rotKeys.first.mValue);
                    glm::quat q1 = aiToGLM(rotationKeys.rotKeys.second.mValue);
                    t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    // if (joint->name == "spine") {
                    //     printf("time: %.2f | t0: %.2f, t1: %.2f, t: %.2f\n", animationTime, t0, t1, t);
                    // }
                    glm::quat q = glm::slerp(q0, q1, t);

                    // Scaling interpolation
                    t0 = scalingKeys.scaleKeys.first.mTime / animation->ticksPerSecond;
                    t1 = scalingKeys.scaleKeys.second.mTime / animation->ticksPerSecond;
                    glm::vec3 s0 = aiToGLM(scalingKeys.scaleKeys.first.mValue);
                    glm::vec3 s1 = aiToGLM(scalingKeys.scaleKeys.second.mValue);
                    t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    glm::vec3 s = glm::mix(s0, s1, t);

                    joint->poseLocalTransform = translate(glm::mat4(1.0f), p) * glm::toMat4(q) *
                        glm::scale(glm::mat4(1.0f), s);
                }

                // Now calculate the global pose for each joint:
                for (auto j  : mesh->skeleton->joints) {
                    if (j->parent) {
                        j->poseGlobalTransform = j->parent->poseGlobalTransform * j->poseLocalTransform;
                    } else {
                        j->poseGlobalTransform = j->poseLocalTransform; // only the root bone has no parent.
                    }
                    j->poseFinalTransform = j->poseGlobalTransform * j->inverseBindMatrix;

                }

                // build frame matrix palette.
                // One transform per joint, we upload this to the gpu for skinning inside the vertex shader:
                std::vector<glm::mat4> framePalette;
                for (auto& j : mesh->skeleton->joints) {
                    framePalette.push_back(j->poseFinalTransform);
                }

                // Upload to gpu:
                uploadConstantBufferData( editorState.graphics.skinningMatricesCBuffer, framePalette.data(), sizeof(glm::mat4) * framePalette.size(), 2);



                }

            renderAnimationPanel(200 + (editorState.screen_width - 400)/2, editorState.screen_height - 32 - 200 + 100, editorState.screen_width - 400, 200, editorState, mesh);

        }

        if (mesh->skeleton != nullptr) {
            bindShaderProgram(editorState.graphics.animatedShaderProgram);
        } else {
            bindShaderProgram(editorState.graphics.shaderProgram3D);
        }
        bindFrameBuffer(editorState.graphics.frameBuffer3DPanel, 0, 0, width, height);
        bindVertexArray(mesh->meshVertexArray);
        uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.perspectiveCamera->matrixBufferPtr(), sizeof(glm::mat4) * 2, 1);
        if (mesh->diffuseTexture.id != -1) {
            bindTexture(mesh->diffuseTexture, 0);
        } else {
            bindTexture(editorState.texturePool["debug_texture"], 0);
        }
        static float roto = 0.0f;
        roto += 0.0f * frame_time;
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 1.0f, 0.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), {0, 0, 0}) * rotationMatrix * scaleMatrix;
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, mesh->index_count, 0);
    }
    blitFramebufferToScreen(editorState.graphics.frameBuffer3DPanel, editorState, width, height, 200+width/2, 32 + height/2, 0.9);

}

void renderGameObjectInLevelEditor(int originX, int originY, int width, int height, EditorState & editorState, GameObject* go) {
    // Render each mesh of this game object:
    if (go->renderData.meshGroup == nullptr) {
        for (auto child_go : go->children) {
            renderGameObjectInLevelEditor(originX, originY, width, height, editorState, child_go);
        }
        return;
    }

    for (auto mesh : go->renderData.meshGroup->meshes) {
        bindVertexArray(mesh->meshVertexArray);
        bindShaderProgram(editorState.graphics.shaderProgram3D);

        if (go->shaderProgram.id != -1) {
            bindShaderProgram(go->shaderProgram);
        } else {
            bindShaderProgram(editorState.graphics.shaderProgram3D);
        }

        // Texture: first we take one of the gameObject, if it is set.
        // If not present, we take the one from the mesh.
        // So, the gameObject can override the "bakedIn" original texture
        // if needed.
        // Otherwise, fall back to dummy texture for now.
        if (go->diffuseTexture.id != -1) {
            bindTexture(go->diffuseTexture, 0);
        } else {
            if (mesh->diffuseTexture.id != -1) {
                bindTexture(mesh->diffuseTexture, 0);
            } else {
                bindTexture(editorState.texturePool["debug_texture"], 0);
            }

        }

        // Skeleton/joint posing:
#ifdef LEVEL_EDITOR_WITH_ANIMATION
        if (mesh->skeleton != nullptr) {
            if (mesh->skeleton->animations.size() > 0) {
                // We need a different shader here, which knows our attributes for blend-weights, blend-indices and the
                // matrix palette cbuffer.
                bindShaderProgram(editorState.graphics.animatedShaderProgram);

                // Run the first animation here
                auto animation = mesh->skeleton->animations[0];
                if (animationTime >= animation->duration) {
                    std::cout << "Animation looping" << std::to_string(animationTime) << std::endl;
                    animationTime = 0.0f;
                }
                for (auto joint : mesh->skeleton->joints) {
                    auto translationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Translation, joint->name);
                    auto rotationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Rotation, joint->name);
                    auto scalingKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Scale, joint->name);

                    // Translation interpolation:
                    float t0 = translationKeys.posKeys.first.mTime / animation->ticksPerSecond;;
                    float t1 = translationKeys.posKeys.second.mTime / animation->ticksPerSecond;
                    glm::vec3 p0 = aiToGLM(translationKeys.posKeys.first.mValue);
                    glm::vec3 p1 = aiToGLM(translationKeys.posKeys.second.mValue);
                    float t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    glm::vec3 p = glm::mix(p0, p1, t);

                    // Rotation interpolation
                    t0 = rotationKeys.rotKeys.first.mTime / animation->ticksPerSecond;
                    t1 = rotationKeys.rotKeys.second.mTime / animation->ticksPerSecond;
                    glm::quat q0 = aiToGLM(rotationKeys.rotKeys.first.mValue);
                    glm::quat q1 = aiToGLM(rotationKeys.rotKeys.second.mValue);
                    t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    // if (joint->name == "spine") {
                    //     printf("time: %.2f | t0: %.2f, t1: %.2f, t: %.2f\n", animationTime, t0, t1, t);
                    // }
                    glm::quat q = glm::slerp(q0, q1, t);

                    // Scaling interpolation
                    t0 = scalingKeys.scaleKeys.first.mTime / animation->ticksPerSecond;
                    t1 = scalingKeys.scaleKeys.second.mTime / animation->ticksPerSecond;
                    glm::vec3 s0 = aiToGLM(scalingKeys.scaleKeys.first.mValue);
                    glm::vec3 s1 = aiToGLM(scalingKeys.scaleKeys.second.mValue);
                    t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                    glm::vec3 s = glm::mix(s0, s1, t);

                    joint->poseLocalTransform = translate(glm::mat4(1.0f), p) * glm::toMat4(q) *
                        glm::scale(glm::mat4(1.0f), s);

                }

                // Now calculate the global pose for each joint:
                for (auto j  : mesh->skeleton->joints) {
                    if (j->parent) {
                        j->poseGlobalTransform = j->parent->poseGlobalTransform * j->poseLocalTransform;
                    } else {
                        j->poseGlobalTransform = j->poseLocalTransform; // only the root bone has no parent.
                    }
                    j->poseFinalTransform = j->poseGlobalTransform * j->inverseBindMatrix;

                }

                // build frame matrix palette.
                // One transform per joint, we upload this to the gpu for skinning inside the vertex shader:
                std::vector<glm::mat4> framePalette;
                for (auto& j : mesh->skeleton->joints) {
                    framePalette.push_back(j->poseFinalTransform);
                }

                // Upload to gpu:
                uploadConstantBufferData( editorState.graphics.skinningMatricesCBuffer, framePalette.data(), sizeof(glm::mat4) * framePalette.size(), 2);


            }
        }
#endif

        static float roto = 0.0f;
        roto += 0.0f * frame_time;
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), go->transform.position) * rotationMatrix * scaleMatrix;
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, mesh->index_count, 0);
    }
}

void renderLevelEditor(int originX, int originY, int width, int height, EditorState & editorState, Level* level) {
    setFrontCulling(false);

    bindShaderProgram(editorState.graphics.shaderProgram3D);
    bindFrameBuffer(editorState.graphics.frameBufferLevelEditorPanel, 0, 0, width, height);
    clearFrameBuffer(editorState.graphics.frameBufferLevelEditorPanel, .0, .01, 0.0, 1);
    uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.perspectiveCamera->matrixBufferPtr(), sizeof(glm::mat4) * 2, 1);

    renderGrid(editorState.graphics.frameBufferLevelEditorPanel, editorState, width, height, editorState.meshEditorGridLines, editorState.graphics.levelEditorGridVertexArray);

    bindShaderProgram(editorState.graphics.lineShaderProgram);
    // Render the post at the origin
    {
        auto postMeshGroup = editorState.meshGroupPool["origin_post.glb"];
        if (postMeshGroup == nullptr) {
            return;
        }
        for (auto mesh : postMeshGroup->meshes) {
            bindVertexArray(mesh->meshVertexArray);

            if (mesh->diffuseTexture.id != -1) {
                bindTexture(mesh->diffuseTexture, 0);
            } else {
                bindTexture(editorState.texturePool["debug_texture"], 0);
            }
                static float roto = 0.0f;
                roto += 0.0f * frame_time;
                auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
                auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
                auto worldMatrix = glm::translate(glm::mat4(1.0f), {0,0,0}) * rotationMatrix * scaleMatrix;
                uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
                renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, mesh->index_count, 0);
            }
    }


    // Render each game object:
    for (auto go : level->gameObjects) {
        renderGameObjectInLevelEditor(originX, originY, width, height, editorState, go);
    }

    blitFramebufferToScreen(editorState.graphics.frameBufferLevelEditorPanel, editorState, width, height, 200+width/2, 32 + height/2, 0.9);

}


void renderMainViewportBody(int originX, int originY, int width, int height, EditorState & editorState) {
    auto currentTab = findTabByTitle(editorState.mainTabs, editorState.currentMainTabTitle);
    if (currentTab) {
        if (currentTab->type == TabType::Model) {
            renderMeshEditor(originX, originY, width, height, editorState, currentTab->meshGroup);
        }
        else if (currentTab->type == TabType::Level) {
            renderLevelEditor(originX, originY, width, height, editorState, currentTab->level);
        }
    }
}

void render3DScenePanel(int originX, int originY, int width, int height, EditorState & editorState) {
    // Render 3D scene, first into framebuffer, then as quad on to the final backbuffer.
    setFrontCulling(false);

    bindShaderProgram(editorState.graphics.shaderProgram3D);
    bindFrameBuffer(editorState.graphics.frameBuffer3DPanel, 0, 0, width, height);
    clearFrameBuffer(editorState.graphics.frameBuffer3DPanel, .0, .0, 0.0, 1);
    uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.perspectiveCamera->matrixBufferPtr(), sizeof(glm::mat4) * 2, 1);

    // Animation timing
    static float animationTime = 0.0f;
    animationTime += frame_time;

    // Render each game object:
    for (auto go : editorState.gameObjects) {
        // Render each mesh of this game object:
        for (auto mesh : go->renderData.meshGroup->meshes) {
            bindVertexArray(mesh->meshVertexArray);

            // Texture: first we take one of the gameObject, if it is set.
            // If not present, we take the one from the mesh.
            // So, the gameObject can override the "bakedIn" original texture
            // if needed.
            // Otherwise, fall back to dummy texture for now.
            if (go->diffuseTexture.id != -1) {
                bindTexture(go->diffuseTexture, 0);
            } else {
                if (mesh->diffuseTexture.id != -1) {
                    bindTexture(mesh->diffuseTexture, 0);
                } else {
                    bindTexture(editorState.texturePool["debug_texture"], 0);
                }

            }

            // Skeleton/joint posing:
            if (mesh->skeleton != nullptr) {
                if (mesh->skeleton->animations.size() > 0) {
                    // We need a different shader here, which knows our attributes for blend-weights, blend-indices and the
                    // matrix palette cbuffer.
                    bindShaderProgram(editorState.graphics.animatedShaderProgram);

                    // Run the first animation here
                    auto animation = mesh->skeleton->animations[0];
                    if (animationTime >= animation->duration) {
                        std::cout << "Animation looping" << std::to_string(animationTime) << std::endl;
                        animationTime = 0.0f;
                    }
                    for (auto joint : mesh->skeleton->joints) {
                        auto translationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Translation, joint->name);
                        auto rotationKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Rotation, joint->name);
                        auto scalingKeys = getStartEndKeyFrameForTime(animationTime, animation, KeyFrameType::Scale, joint->name);

                        // Translation interpolation:
                        float t0 = translationKeys.posKeys.first.mTime / animation->ticksPerSecond;;
                        float t1 = translationKeys.posKeys.second.mTime / animation->ticksPerSecond;
                        glm::vec3 p0 = aiToGLM(translationKeys.posKeys.first.mValue);
                        glm::vec3 p1 = aiToGLM(translationKeys.posKeys.second.mValue);
                        float t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                        glm::vec3 p = glm::mix(p0, p1, t);

                        // Rotation interpolation
                        t0 = rotationKeys.rotKeys.first.mTime / animation->ticksPerSecond;
                        t1 = rotationKeys.rotKeys.second.mTime / animation->ticksPerSecond;
                        glm::quat q0 = aiToGLM(rotationKeys.rotKeys.first.mValue);
                        glm::quat q1 = aiToGLM(rotationKeys.rotKeys.second.mValue);
                        t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                        // if (joint->name == "spine") {
                        //     printf("time: %.2f | t0: %.2f, t1: %.2f, t: %.2f\n", animationTime, t0, t1, t);
                        // }
                        glm::quat q = glm::slerp(q0, q1, t);

                        // Scaling interpolation
                        t0 = scalingKeys.scaleKeys.first.mTime / animation->ticksPerSecond;
                        t1 = scalingKeys.scaleKeys.second.mTime / animation->ticksPerSecond;
                        glm::vec3 s0 = aiToGLM(scalingKeys.scaleKeys.first.mValue);
                        glm::vec3 s1 = aiToGLM(scalingKeys.scaleKeys.second.mValue);
                        t = ((float) animationTime - t0 ) / (float) (t1 - t0);
                        glm::vec3 s = glm::mix(s0, s1, t);

                        joint->poseLocalTransform = translate(glm::mat4(1.0f), p) * glm::toMat4(q) *
                            glm::scale(glm::mat4(1.0f), s);

                    }

                    // Now calculate the global pose for each joint:
                    for (auto j  : mesh->skeleton->joints) {
                        if (j->parent) {
                            j->poseGlobalTransform = j->parent->poseGlobalTransform * j->poseLocalTransform;
                        } else {
                            j->poseGlobalTransform = j->poseLocalTransform; // only the root bone has no parent.
                        }
                        j->poseFinalTransform = j->poseGlobalTransform * j->inverseBindMatrix;

                    }

                    // build frame matrix palette.
                    // One transform per joint, we upload this to the gpu for skinning inside the vertex shader:
                    std::vector<glm::mat4> framePalette;
                    for (auto& j : mesh->skeleton->joints) {
                        framePalette.push_back(j->poseFinalTransform);
                    }

                    // Upload to gpu:
                    uploadConstantBufferData( editorState.graphics.skinningMatricesCBuffer, framePalette.data(), sizeof(glm::mat4) * framePalette.size(), 2);


                }
            }

            static float roto = 0.0f;
            roto += 0.0f * frame_time;
            auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roto), glm::vec3(0.0f, 0.0f, 1.0f));
            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), go->transform.position) * rotationMatrix * scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, mesh->index_count, 0);
        }

    }

    // Render the 3D panel framebuffer as quad
    {


        bindDefaultBackbuffer(0, 0, editorState.screen_width, editorState.screen_height);
        bindFrameBufferTexture(editorState.graphics.frameBuffer3DPanel, 0);
        uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);

        bindVertexArray(editorState.graphics.quadVertexArray);
        bindShaderProgram(editorState.graphics.shaderProgram);

        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(width, height, 1));
        auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(200 + width/2, 32 + height/2, 0.9)) * rotationMatrix * scaleMatrix;
        //setFrontCulling(false);
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
    }

    // End 3d scene

}

void blitFramebufferToScreen(GraphicsHandle framebuffer, EditorState& editorState, int width, int height, int originX, int originY, float depth) {
    // Blit quad to default framebuffer:
    enableBlending(false);
    bindDefaultBackbuffer(0, 0, editorState.screen_width, editorState.screen_height);
    bindVertexArray(editorState.graphics.quadVertexArray);
    bindShaderProgram(editorState.graphics.shaderProgram);
    bindFrameBufferTexture(framebuffer, 0);
    uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);
    auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(width, height, 1));
    auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(originX, originY, depth)) * rotationMatrix * scaleMatrix;
    uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
    renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
}

void renderAnimationPanel(int originX, int originY, int width, int height, EditorState & editorState, Mesh* mesh) {

    auto annotation = beginRenderAnnotation(L"AnimationPanel");

    setFrontCulling(true);
    bindVertexArray(editorState.graphics.quadVertexArray);
    bindShaderProgram(editorState.graphics.shaderProgram);
    bindFrameBuffer(editorState.graphics.frameBufferAnimationPanel, 0, 0, width, height);
    clearFrameBuffer(editorState.graphics.frameBufferAnimationPanel, .11, .11, 0.11, 1);

    activateOrthoCameraForPanel(width, height, editorState);

    // Camera sceneTreeCamera;
    // sceneTreeCamera.view_matrix = glm::mat4(1);
    // sceneTreeCamera.projection_matrix = glm::orthoLH_ZO<float>(0.0f, (float) width, (float) height, 0.0f, 0.0, 30);
    // uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, sceneTreeCamera.matrixBufferPtr(), sizeof(Camera), 1);

    // Write out the animation names
    {
        int vOffset = 50;
        int animationIndex = 0;
        bindShaderProgram(editorState.graphics.textShaderProgram);
        enableBlending(true);
        for (auto animation : mesh->skeleton->animations) {
            auto textMesh = editorState.textMesh;
            updateText(*textMesh, editorState.graphics.fontHandle, animation->name);
            bindVertexArray(textMesh->meshVertexArray);
            bindTexture(getTextureFromFont(editorState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
            auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(200, vOffset + 20 * animationIndex++, 1));
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
        }
    }

    blitFramebufferToScreen(editorState.graphics.frameBufferAnimationPanel, editorState, width, height, originX, originY, 0.9);
    endRenderAnnotation(annotation);
}

void renderAssetPanel(int originX, int originY, int width, int height, EditorState & editorState) {
        setFrontCulling(true);
        bindVertexArray(editorState.graphics.quadVertexArray);
        bindShaderProgram(editorState.graphics.shaderProgram);
        bindFrameBuffer(editorState.graphics.frameBufferAssetPanel, 0, 0, width, height);
        clearFrameBuffer(editorState.graphics.frameBufferAssetPanel, .1, .1, 0.1, 1);

        Camera sceneTreeCamera;
        sceneTreeCamera.view_matrix = glm::mat4(1);
        sceneTreeCamera.projection_matrix = glm::orthoLH_ZO<float>(0.0f, (float) width, (float) height, 0.0f, 0.0, 30);
        uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, sceneTreeCamera.matrixBufferPtr(), sizeof(Camera), 1);

        // Render the assets:
        enableBlending(true);

        int meshIndex = 0;
        int verticalSize = 128;
        int horizontalSize = 128;
        float marginLeft = horizontalSize/2 + 24;
        for (auto meshGroup : editorState.importedMeshGroups) {
            int verticalOffset = 40 - editorState.assetVBrowserVScrollOffset + verticalSize/2 + 144 * meshIndex++;
            auto thumbnailTexture = meshGroup->meshes[0]->thumbnail;
            bindTexture(thumbnailTexture, 0);
            auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(128, 128, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(marginLeft, verticalOffset, 2)) * rotationMatrix * scaleMatrix;

            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);

            if (editorState.hoveredAssetIndex == meshIndex-1) {
                bindTexture(editorState.texturePool["light_gray_bg"], 0);
                auto rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(136, 136, 2));
                auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(marginLeft, verticalOffset, 2.5)) * rotationMatrix * scaleMatrix;
                uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
                renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
            }
        }

        // Vertical scrollbar in case we have more assets then can be displayed:
        if (editorState.importedMeshGroups.size() * 144 > height) {
            //enableBlending(true);
            bindVertexArray(editorState.graphics.quadVertexArray);
            bindShaderProgram(editorState.graphics.shaderProgram);
            bindTexture(editorState.texturePool["gray_bg"], 0);

            auto panelHeight = height - 32;
            auto totalContentHeight = editorState.importedMeshGroups.size() * 144;
            auto thumbHeightPercentage = (float)((float)panelHeight / (float)totalContentHeight);
            editorState.assetBrowserVScrollThumbHeight = thumbHeightPercentage * panelHeight;

            float maxContentTravel = totalContentHeight - panelHeight;
            float maxThumbTravel = panelHeight- editorState.assetBrowserVScrollThumbHeight;
            editorState.assetVBrowserVScrollOffset = (editorState.assetVBrowserVScrollPosition / maxThumbTravel) * maxContentTravel;

            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(16, editorState.assetBrowserVScrollThumbHeight, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(width - 8, 32 + 8 + editorState.assetVBrowserVScrollPosition + editorState.assetBrowserVScrollThumbHeight/2 , 1)) * scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
        }

        // Scroll "protection" behind the title text but in front of the actual assets, so they
        // slip under this protection panel:
        {
                bindVertexArray(editorState.graphics.quadVertexArray);
                bindShaderProgram(editorState.graphics.shaderProgram);
                bindTexture(editorState.texturePool["gray_bg"], 0);
                auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(200, 32, 1));
                auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(100, 16 , 1.8)) * scaleMatrix;
                uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
                renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
        }

        // Panel title text
        {
            bindShaderProgram(editorState.graphics.textShaderProgram);
            auto textMesh = editorState.textMesh;
            updateText(*textMesh, editorState.graphics.fontHandle, "Asset Browser");
            bindVertexArray(textMesh->meshVertexArray);
            bindTexture(getTextureFromFont(editorState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(8, 20, 1)) *  scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
        }

        blitFramebufferToScreen(editorState.graphics.frameBufferAssetPanel, editorState, width, height, originX, originY, 2);

}

void renderStatusBar(int originX, int originY, int width, int height, EditorState & editorState) {
    // Render topmenu
        setFrontCulling(true);
        bindVertexArray(editorState.graphics.quadVertexArray);
        bindFrameBuffer(editorState.graphics.frameBufferTopMenu, 0, 0, width, height);
        clearFrameBuffer(editorState.graphics.frameBufferTopMenu, 0.1, .1, 0.11, 1);

        // Specific projection cam for this panel:
        Camera cam;
        cam.view_matrix = glm::mat4(1);
        cam.projection_matrix = glm::orthoLH_ZO<float>(0.0f, (float) width, (float) height, 0.0f, 0.0, 30);
        uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, cam.matrixBufferPtr(), sizeof(Camera), 1);

        // Render menu texts
        enableBlending(true);
        {
            // bindShaderProgram(editorState.graphics.textShaderProgram);
            // auto textMesh = editorState.menuTextMeshes.tmFile;
            // bindVertexArray(textMesh->meshVertexArray);
            // bindTexture(getTextureFromFont(editorState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
            // auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
            // auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(8, 24, 1)) *  scaleMatrix;
            // uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            // renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
        }

        // Render frametime status label:
        {
            auto textMesh = editorState.textMesh;
            bindShaderProgram(editorState.graphics.textShaderProgram);
            updateText(*textMesh, editorState.graphics.fontHandle, "FT(us): " + std::to_string(editorState.frameTimer->getAverage() * 1000 * 1000));
            bindVertexArray(textMesh->meshVertexArray);
            bindTexture(getTextureFromFont(editorState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(8, 18, 1)) *  scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
        }

        // Render mouse coordinates:
        {
            auto textMesh = editorState.textMesh;
            bindShaderProgram(editorState.graphics.textShaderProgram);
            updateText(*textMesh, editorState.graphics.fontHandle, "mouse: " + std::to_string(mouseX()) + "/" + std::to_string(mouseY()));
            bindVertexArray(textMesh->meshVertexArray);
            bindTexture(getTextureFromFont(editorState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
            auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
            auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(editorState.screen_width- 160, 18, 1)) *  scaleMatrix;
            uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
            renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
        }


        // Draw to default backbuffer
        blitFramebufferToScreen(editorState.graphics.frameBufferTopMenu, editorState, width, height, originX, originY, 0.9);
        // {
        //     enableBlending(false);
        //     bindDefaultBackbuffer(0, 0, editorState.screen_width, editorState.screen_height);
        //     bindVertexArray(editorState.graphics.quadVertexArray);
        //     bindShaderProgram(editorState.graphics.shaderProgram);
        //     bindFrameBufferTexture(editorState.graphics.frameBufferTopMenu, 0);
        //     uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);
        //     auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(width, height, 1));
        //     auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(originX, originY, 0.9)) * scaleMatrix;
        //     //setFrontCulling(false);
        //     uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        //     renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
        //
        // }
}

void renderMainTabPanel(int originX, int originY, int width, int height, EditorState & editorState) {

    setFrontCulling(true);
    bindVertexArray(editorState.graphics.quadVertexArray);
    bindFrameBuffer(editorState.graphics.frameBufferMainTabPanel, 0, 0, width, height);
    clearFrameBuffer(editorState.graphics.frameBufferMainTabPanel, 0.0, .0, 0.0, 1);

    activateOrthoCameraForPanel(width, height, editorState);

    // First render all tab headers.
    // Each tab-header is a rect with the name of the file/resource it is displaying/editing.
    // Tabs consist of a header and a body.
    // Only the body of the currently active tab is rendered.
    enableBlending(true);

    int accumulatedTextLength = 0;
    for (auto tab : editorState.mainTabs) {

        int horizontalOffset = 8 + accumulatedTextLength;
        auto textMesh = tab->tabHeader.titleTextMesh;
        auto titleBoundingBox = measureText(editorState.graphics.fontHandle, tab->tabHeader.title);
        auto titleWidth = titleBoundingBox.right - titleBoundingBox.left;
        accumulatedTextLength += titleWidth + 24;

        // The background rectangle:
        enableBlending(false);
        bindVertexArray(editorState.graphics.quadVertexArray);
        bindShaderProgram(editorState.graphics.shaderProgram);
        if (editorState.currentMainTabTitle == tab->tabHeader.title) {
            bindTexture(editorState.texturePool["light_gray_bg"], 0);
        } else {
            bindTexture(editorState.texturePool["gray_bg"], 0);
        }

        if (editorState.currentHoverTabTitle == tab->tabHeader.title) {
            bindTexture(editorState.texturePool["mid_blue_bg"], 0);
        }

        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(titleWidth+16, 32, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(horizontalOffset-4 + (titleWidth/2) + 4, 16, 2)) * scaleMatrix;
        tab->renderBoundingBox = {(float)horizontalOffset-4, 0, (float)horizontalOffset-4 + titleWidth + 16, 32};
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);

        // Render the title text:
        enableBlending(true);
        bindShaderProgram(editorState.graphics.textShaderProgram);
        bindVertexArray(textMesh->meshVertexArray);
        bindTexture(getTextureFromFont(editorState.graphics.fontHandle), 0); // This is not the texture, but the font handle, which carries the texture.
        worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(horizontalOffset, 20, 1));
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, textMesh->index_count, 0);
    }

    // Separation line at the end of the tab header:
    {
        enableBlending(false);
        bindVertexArray(editorState.graphics.quadVertexArray);
        bindShaderProgram(editorState.graphics.shaderProgram);
        bindTexture(editorState.texturePool["gray_bg"], 0);
        auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(width, 2, 1));
        auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(width/2 , 31, 2)) * scaleMatrix;
        uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
        renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
    }




    // Blit fulltime quad
    enableBlending(false);
    bindDefaultBackbuffer(0, 0, editorState.screen_width, editorState.screen_height);
    bindVertexArray(editorState.graphics.quadVertexArray);
    bindShaderProgram(editorState.graphics.shaderProgram);
    bindFrameBufferTexture(editorState.graphics.frameBufferMainTabPanel, 0);
    uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);
    auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(width, height, 1));
    auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(originX, originY, 0.9)) * scaleMatrix;
    //setFrontCulling(false);
    uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
    renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);



}

void renderPanels(EditorState& editorState) {

    // Here we should know where every panel is, so we can give it its position as arguments.
    // This is bettter than letting the panel know where it is supposed to be. It normally can not know it.
    renderTopMenu(editorState.screen_width/2, 16, editorState.screen_width, 32, editorState);
    renderGameObjectTree(200/2, 32+ (editorState.screen_height-64)/2, 200, editorState.screen_height-64, editorState);
    renderMainTabPanel(editorState.screen_width/2, 32 + 16, editorState.screen_width - 400, 32, editorState);
    renderMainViewportBody(200, 32 + (editorState.screen_height - 96/2), editorState.screen_width - 400, editorState.screen_height - 96, editorState);
    renderAssetPanel(editorState.screen_width - 100, 32+ (editorState.screen_height-64)/2, 200, editorState.screen_height-64, editorState);
    renderStatusBar(editorState.screen_width/2, editorState.screen_height - 16, editorState.screen_width, 32, editorState);

}

bool assetIsAlreadyOpenInTab(const std::string& tabTitle, EditorState& editorState) {
    for (auto tab : editorState.mainTabs) {
        if (tab->tabHeader.title == tabTitle) {
            return true;
        }
    }
    return false;
}



void check_game_object_tree_inputs(EditorState& editorState) {

    bool expandedAnItem = false;
    // First, check for clicking on the expanse icons:
    {
        int vOffset = 24;
        int hOffset = 0;
        editorState.currentHoverExpandItem = nullptr;

        int mouse_x = mouseX();
        int mouse_y = mouseY();
        for (auto treeItem : editorState.visibleGameObjectTreeItems) {
            if (mouse_x > (hOffset + treeItem->treeBoundingBox.left) && mouse_x < (hOffset + treeItem->treeBoundingBox.right)  &&
                mouse_y > (vOffset + treeItem->treeBoundingBox.top) && mouse_y < (vOffset + treeItem->treeBoundingBox.bottom) ) {

                editorState.currentHoverExpandItem = treeItem;
                if (mouseLeftClick()) {
                    expandedAnItem = true;
                    treeItem->expandedInTree = !treeItem->expandedInTree;
                }
            }
        }
    }

    // If we did not expand an icon, we will also check for selecting of the actual
    // tree items.
    if (!expandedAnItem) {
        {
            int topOffset = 72;
            int vSize = 24;
            if (mouseX() > 20 && mouseX() < 192 &&
                mouseY() > 64 && mouseY() < editorState.screen_height-32) {
                editorState.hoveredGameObjectTreeItemIndex  = (mouseY() - topOffset) / (vSize);

                } else {
                    editorState.hoveredGameObjectTreeItemIndex = -1;
                }
        }

        // Check if the calculated index corresponds to an actual asset.
        // TODO: accommodate for scrolling offsets.
        if (editorState.hoveredGameObjectTreeItemIndex > -1) {
            if (editorState.hoveredGameObjectTreeItemIndex < editorState.visibleGameObjectTreeItems.size()) {
                if (mouseLeftClick()) {
                    editorState.currentSelectedGameObjectTreeItem = editorState.visibleGameObjectTreeItems[editorState.hoveredGameObjectTreeItemIndex];
                }
            }
        }

    }


}

void check_main_tab_inputs(EditorState& editorState) {
    int vOffset = 32;
    int hOffset = 200;
    editorState.currentHoverTabTitle = "";
    int mouse_x = mouseX();
    int mouse_y = mouseY();
    for (auto tab : editorState.mainTabs) {
        if (mouse_x > (hOffset + tab->renderBoundingBox.left) && mouse_x < (hOffset + tab->renderBoundingBox.right)  &&
            mouse_y > (vOffset + tab->renderBoundingBox.top) && mouse_y < (vOffset + tab->renderBoundingBox.bottom) ) {
                editorState.currentHoverTabTitle = tab->tabHeader.title;
                if (mouseLeftClick()) {
                    editorState.currentMainTabTitle = tab->tabHeader.title;
                }
            }
    }

}

void check_asset_browser_inputs(EditorState & editorState) {
    int topOffset = 72 - editorState.assetVBrowserVScrollOffset ;
    int vSize = 128;
    int vMargin = 16;
    if (mouseX() > editorState.screen_width - 184 && mouseX() < editorState.screen_width - 48 &&
        mouseY() > 64 && mouseY() < editorState.screen_height - 32) {
        float assetIndexWithMargin  = (mouseY() - topOffset) / (vSize + vMargin);
        editorState.hoveredAssetIndex = assetIndexWithMargin;
    } else {
        editorState.hoveredAssetIndex = -1;
    }

    // Check if the calculated index corresponds to an actual asset.
    // TODO: accommodate for scrolling offsets.
    if (editorState.hoveredAssetIndex >= 0) {
        if (editorState.hoveredAssetIndex < editorState.importedMeshGroups.size()) {
            if (mouseLeftDoubleClick() || mouseLeftClick()) {
                auto meshGroup = editorState.importedMeshGroups[editorState.hoveredAssetIndex];
                auto name = meshGroup->name;
                if (assetIsAlreadyOpenInTab(name, editorState)) {
                    // Set the current tab to be the selected one here:
                    editorState.currentMainTabTitle = name;
                } else {
                    auto modelTab = new Tab{name};
                    modelTab->tabHeader.titleTextMesh = createTextMesh(editorState.graphics.fontHandle, name);
                    modelTab->type = TabType::Model;
                    modelTab->meshGroup = meshGroup;
                    editorState.mainTabs.push_back(modelTab);
                }
            }
        }
    }

    // Check vscrollbar.
    // For now we just listen to some keys to move the scrollbar, so we can test that
    // the actual content display works. So, later, we can concentrate on the picking of the scrollbar.
    if (keyPressed('K')) {
        auto travel = editorState.assetBrowserVScrollThumbHeight + editorState.assetVBrowserVScrollPosition;
        std::cout << "Travel: " << std::to_string(travel) << " pos: " << std::to_string(editorState.assetVBrowserVScrollPosition) << std::endl;
        if (travel < (editorState.screen_height - 64 - 32 - 8)) {
            editorState.assetVBrowserVScrollPosition += 1;
        }
    }

    if (keyPressed('I')) {
        if (editorState.assetVBrowserVScrollPosition > 0) {
            editorState.assetVBrowserVScrollPosition -= 1;
        }
    }
}

void check_menu_inputs(EditorState & editorState) {
    int vOffset = 0;
    int hOffset = 0;
    editorState.currentHoverMenuItem = nullptr;
    int mouse_x = mouseX();
    int mouse_y = mouseY();
    for (auto menuItem : editorState.topMenuItems) {
        if (mouse_x > (hOffset + menuItem->renderBoundingBox.left) && mouse_x < (hOffset + menuItem->renderBoundingBox.right)  &&
            mouse_y > (vOffset + menuItem->renderBoundingBox.top) && mouse_y < (vOffset + menuItem->renderBoundingBox.bottom) ) {
            editorState.currentHoverMenuItem = menuItem;
            if (mouseLeftClick()) {
                if (menuItem->action) {
                    menuItem->action(editorState);
                }
            }
        }
    }

    //
    // if (mouseX() > 64 && mouseX() < 200 && mouseY() > 12 && mouseY() < 32) {
    //     if (mouseLeftClick()) {
    //
    //
    //
    //     }
    // }

}

void check_resizing(EditorState& editorState) {
    auto new_width = resizedWidth();
    auto new_height = resizedHeight();
    if (new_width != editorState.screen_width || new_height != editorState.screen_height) {
        editorState.screen_width = new_width;
        editorState.screen_height = new_height;
        resizeSwapChain(editorState.window->get_hwnd(), new_width, new_height);

        editorState.orthoCamera->view_matrix = glm::mat4(1);
        editorState.orthoCamera->projection_matrix = (glm::orthoLH_ZO<float>(0, editorState.screen_width,  editorState.screen_height, 0.0f, 0.0, 50));

        editorState.perspectiveCamera->location = {0, 3, -6};
        editorState.perspectiveCamera->lookAtTarget = {0, 0, 3};
        editorState.perspectiveCamera->view_matrix =editorState.perspectiveCamera->updateAndGetViewMatrix();
        editorState.perspectiveCamera->projection_matrix = editorState.perspectiveCamera->updateAndGetPerspectiveProjectionMatrix(65,
            editorState.screen_width, editorState.screen_height, 0.1, 100);

        resizeFrameBuffer(editorState.graphics.frameBuffer3DPanel, editorState.screen_width - 200 * 2, editorState.screen_height - 64);
        resizeFrameBuffer(editorState.graphics.frameBufferMainTabPanel, editorState.screen_width - 200 * 2, 32);
        resizeFrameBuffer(editorState.graphics.frameBufferTopMenu, new_width, 32);
        resizeFrameBuffer(editorState.graphics.frameBufferStatusBar, editorState.screen_width, 32);
        resizeFrameBuffer(editorState.graphics.frameBufferGameObjectTree, 200, editorState.screen_height-64);
        resizeFrameBuffer(editorState.graphics.frameBufferAssetPanel, 200, editorState.screen_height-64);
        resizeFrameBuffer(editorState.graphics.frameBufferAnimationPanel, editorState.screen_width - 200 * 2, 200);
    }
}

// We currently have two horizontal splitters:
// gameObjectTree <-> Main Tab viewport
// Main Tab viewport <-> Asset Browser
void renderSplitters(EditorState & editorState) {

    enableBlending(false);
    bindDefaultBackbuffer(0, 0, editorState.screen_width, editorState.screen_height);
    bindVertexArray(editorState.graphics.quadVertexArray);
    bindShaderProgram(editorState.graphics.shaderProgram);
    bindTexture(editorState.texturePool["gray_bg"], 0);
    uploadConstantBufferData( editorState.graphics.cameraTransformBuffer, editorState.orthoCamera->matrixBufferPtr(), sizeof(Camera), 1);
    auto scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(4, editorState.screen_height - 64, 1));
    auto worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(200-2, 32  + (editorState.screen_height - 64)/2.0f, 0.5)) * scaleMatrix;
    uploadConstantBufferData( editorState.graphics.objectTransformBuffer, glm::value_ptr(worldMatrix), sizeof(glm::mat4), 0);
    renderGeometryIndexed(PrimitiveType::TRIANGLE_LIST, 6, 0);
}

void do_frame(const Win32Window & window, EditorState& editorState) {

    update_camera(editorState);
    check_resizing(editorState);
    check_menu_inputs(editorState);
    check_asset_browser_inputs(editorState);
    check_main_tab_inputs(editorState);
    check_game_object_tree_inputs(editorState);

    setDepthTesting(true);
    clear(0, 0.0, 0, 1);

    renderPanels(editorState);
    renderSplitters(editorState);

    present();
}

static std::string vshader_line_hlsl = R"(
    struct VOutput
    {
        float4 pos : SV_POSITION;
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

    VOutput main(float4 pos : POSITION) {
        VOutput output;

        output.pos = mul(pos, world_matrix);
        output.pos = mul(output.pos, view_matrix);
        output.pos = mul(output.pos, projection_matrix);

        return output;
     }
)";


static std::string vshader_hlsl = R"(
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



 static std::string vshader_hlsl_animated = R"(
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

    cbuffer SkinningMatrices : register(b2) {
        row_major float4x4 jointTransforms[50];
    };

    VOutput main(float4 pos : POSITION, float2 uv : TEXCOORD0,
                float4 blend_weights : BLENDWEIGHT,
                float4 blend_indices : BLENDINDICES) {

        VOutput output;


        float4 skinned = mul(pos, jointTransforms[blend_indices.x]) * blend_weights.x;
        skinned += mul(pos, jointTransforms[blend_indices.y]) * blend_weights.y;
        skinned += mul(pos, jointTransforms[blend_indices.z]) * blend_weights.z;
        skinned += mul(pos, jointTransforms[blend_indices.w]) * blend_weights.w;

        output.pos = mul(skinned, world_matrix);
        output.pos = mul(output.pos, view_matrix);
        output.pos = mul(output.pos, projection_matrix);
        output.uv = uv;

        return output;
     }
)";

static std::string fs_text_hlsl = R"(
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

static std::string fshader_3d_hlsl=  R"(

    struct VOutput
     {
     	float4 pos : SV_POSITION;
     	float2 uv : TEXCOORD0;
     };

    Texture2D imageTexture;
    SamplerState samplerState;

    float4 main(VOutput pixelShaderInput) : SV_TARGET
    {
        return imageTexture.Sample(samplerState, pixelShaderInput.uv);
        //return float4(1.0, 1.0, 0.0, 1.0); // yellow for dx11
        //return float4(pixelShaderInput.uv.x, pixelShaderInput.uv.y, 0, 1);
        //return float4(1.0, 0.5, 0.0, 1.0);
    }
)";

static std::string fshader_color_hlsl = R"(

    struct VOutput
     {
     	float4 pos : SV_POSITION;
     };


    float4 main(VOutput pixelShaderInput) : SV_TARGET
    {
        return float4(1.0, 0.5, 0.0, 1.0);
    }
)";

static std::string fshader_hlsl = R"(

    struct VOutput
     {
     	float4 pos : SV_POSITION;
     	float2 uv : TEXCOORD0;
     };

    Texture2D imageTexture;
    SamplerState samplerState;

    float4 main(VOutput pixelShaderInput) : SV_TARGET
    {
        return imageTexture.Sample(samplerState, pixelShaderInput.uv);
        //return float4(1.0, 1.0, 0.0, 1.0); // yellow for dx11
        //return float4(pixelShaderInput.uv.x, pixelShaderInput.uv.y, 0, 1);
        //return float4(1.0, 0.5, 0.0, 1.0);
    }
)";


static std::string vshader_glsl = R"(

    #version 460

    layout(location = 0) in vec3 pos;
    layout(location = 1) in vec2 uv;
    //layout(location = 2) in vec3 normal;

    layout(std140, binding = 0) uniform ObjectTransformBuffer {
        mat4 worldMatrix;
    } objectTransform;

    layout(std140, binding = 1) uniform CameraBuffer {
        mat4 viewMatrix;
        mat4 projectionMatrix;
    } camera;

    out vec2 fs_uv;

    void main() {
        gl_Position = camera.projectionMatrix * camera.viewMatrix * objectTransform.worldMatrix * vec4(pos, 1);
        fs_uv = uv;

    };

)";



static std::string fshader_glsl = R"(
    #version 460

    layout(binding = 0) uniform sampler2D diffuseTexture;


    out vec4 color;

    in vec2 fs_uv;

    void main() {
        //color = vec4(1, 0, 0, 1);
        color = texture(diffuseTexture, fs_uv);
    }

)";


uint8_t* load_image(const std::string& fileName, int* width, int* height) {
    int imageChannels;

    auto pixels = stbi_load(
            fileName.c_str(), width, height,
            &imageChannels,
            4);

    return pixels;

}

struct VOutput
{
    glm::vec4 pos;
    glm::vec2 tex;
};

void createTestDummyTabs(EditorState & editorState) {
    auto tab1 = new Tab{"Soldier"};
    tab1->tabHeader.titleTextMesh = createTextMesh(editorState.graphics.fontHandle, "Soldier");
    tab1->type = TabType::Model;
    auto tab2 = new Tab{"Knight"};
    tab2->tabHeader.titleTextMesh = createTextMesh(editorState.graphics.fontHandle, "Knight");
    tab2->type = TabType::Model;
    auto tab3 = new Tab{"Diffuse_texture_soldier"};
    tab3->tabHeader.titleTextMesh = createTextMesh(editorState.graphics.fontHandle, "Diffuse_texture_soldier");
    tab3->type = TabType::Model;
    editorState.mainTabs.push_back(tab1);
    editorState.mainTabs.push_back(tab2);
    editorState.mainTabs.push_back(tab3);
}

void createTestProject(EditorState & editorState) {
    editorState.project = new Project();
    editorState.project->name = "TestProject1";

    auto level1 = new Level();
    level1->name = "Level1";
    level1->gameObjects.push_back(editorState.rootGameObject);
    editorState.project->levels.push_back(level1);

}

void createTestGameObjects(EditorState & editorState) {
    auto heroGo = new GameObject();
    heroGo->transform.position = glm::vec3(2, 0, 6);
    heroGo->renderData.meshGroup = editorState.meshGroupPool["test.glb"];
    heroGo->name = "Hero";
    heroGo->expandedInTree = false;
    editorState.gameObjects.push_back(heroGo);

    auto enemy1Go = new GameObject();
    enemy1Go->transform.position = glm::vec3(2, 0, 5);
    enemy1Go->renderData.meshGroup = editorState.meshGroupPool["enemy_mesh"];
    enemy1Go->name = "Enemy1";
    enemy1Go->expandedInTree = true;
    editorState.gameObjects.push_back(enemy1Go);

    auto enemy1DroneGo = new GameObject();
    enemy1DroneGo->transform.position = glm::vec3(-1, 0, 2);
    enemy1DroneGo->renderData.meshGroup = editorState.meshGroupPool["drone_mesh"];
    enemy1DroneGo->name = "Enemy1Drone";
    enemy1Go->children.push_back(enemy1DroneGo);
    //enemy1DroneGo->expandedInTree = true;

    auto enemy1DroneDeviceGo = new GameObject();
    enemy1DroneDeviceGo->transform.position = glm::vec3(-1, 0, 2);
    enemy1DroneDeviceGo->renderData.meshGroup = editorState.meshGroupPool["drone_device_mesh"];
    enemy1DroneDeviceGo->name = "DroneDevice";
    enemy1DroneGo->children.push_back(enemy1DroneDeviceGo);

    auto enemy2Go = new GameObject();
    enemy2Go->transform.position = glm::vec3(-1, 0, 2);
    enemy2Go->renderData.meshGroup = editorState.meshGroupPool["enemy_mesh"];
    enemy2Go->name = "Enemy2";
    editorState.gameObjects.push_back(enemy2Go);

    editorState.rootGameObject->children.push_back(heroGo);
    editorState.rootGameObject->children.push_back(enemy1Go);
    editorState.rootGameObject->children.push_back(enemy2Go);


}

void importModelAction(EditorState & editorState) {
    auto fileName = showFileDialog(L"All\0*.*\0fbx\0*.fbx\0gltf\0*.glb");
    //editorState.meshGroupPool.clear();
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string fileNameNonWide = converter.to_bytes(fileName);
    if (fileNameNonWide.empty()) return;
    auto importedMeshDataItems = importMeshFromFile(fileNameNonWide);
    auto thumbnailTexture = createThumbnailForMesh(importedMeshDataItems, editorState.graphics.shaderProgram3D, editorState.graphics.objectTransformBuffer, editorState.graphics.cameraTransformBuffer, editorState.vertexAttributes, 128, 128);
    auto meshGroup = new MeshGroup();
    meshGroup->name = fileNameFromPath(fileNameNonWide);
    for (auto im : importedMeshDataItems) {
        auto mesh = im->toMesh();
        mesh->thumbnail = thumbnailTexture;
        // Now we decide which attributes to link with:
        // For skeletal meshes with animations we use the gpu vertex skinned version, otherwise the "default one":
        if (mesh->skeleton != nullptr && mesh->skeleton->animations.size() > 0) {
            describeVertexAttributes(editorState.vertexAttributesAnimated, mesh->meshVertexBuffer, editorState.graphics.animatedShaderProgram, mesh->meshVertexArray);
        } else {
            describeVertexAttributes(editorState.vertexAttributes, mesh->meshVertexBuffer, editorState.graphics.shaderProgram, mesh->meshVertexArray);
        }
        editorState.meshGroupPool[meshGroup->name] = meshGroup;
        meshGroup->meshes.push_back(mesh);
    }
    editorState.importedMeshGroups.push_back(meshGroup);

}

std::tuple<std::vector<float>, std::vector<uint32_t>> buildGridVertices(int numberOfLines) {

    std::vector<float> grid_vertices;

    float start = -(numberOfLines / 2.0f) + 0.5;
    for (int nl = 0; nl < numberOfLines; ++nl) {

        float offset = nl;

        // Lines reaching into the scene
        grid_vertices.push_back(start + offset);
        grid_vertices.push_back(0.0f);
        grid_vertices.push_back(-100.0f);

        grid_vertices.push_back(start + offset);
        grid_vertices.push_back(0.0f);
        grid_vertices.push_back(100.0f);

        // Lines perpendicular to us:
        grid_vertices.push_back(-100);
        grid_vertices.push_back(0.0f);
        grid_vertices.push_back(start + offset);

        grid_vertices.push_back(100);
        grid_vertices.push_back(0.0f);
        grid_vertices.push_back(start + offset);

    }

    std::vector<uint32_t> grid_indices;
    for (int nl = 0; nl < numberOfLines * 4; nl += 4) {
        grid_indices.push_back(nl);
        grid_indices.push_back(nl+1);
        grid_indices.push_back(nl+2);
        grid_indices.push_back(nl+3);
    }

    return {grid_vertices, grid_indices};

}

void importModelByFileName(EditorState& editorState, const std::string& fileName) {
    auto importedMeshData = importMeshFromFile(fileName);
    auto thumbnailTexture = createThumbnailForMesh(importedMeshData, editorState.graphics.shaderProgram3D,
        editorState.graphics.objectTransformBuffer, editorState.graphics.cameraTransformBuffer, editorState.vertexAttributes, 128, 128);
    auto meshGroup = new MeshGroup();
    meshGroup->name = fileNameFromPath(fileName);
    editorState.meshGroupPool[meshGroup->name] = meshGroup;
    for (auto im : importedMeshData) {
        auto mesh = im->toMesh();
        mesh->thumbnail = thumbnailTexture;
        // Now we decide which attributes to link with:
        // For skeletal meshes with animations we use the gpu vertex skinned version, otherwise the "default one":
        if (mesh->skeleton != nullptr && mesh->skeleton->animations.size() > 0) {
            describeVertexAttributes(editorState.graphics.animatedVertexAttributes, mesh->meshVertexBuffer, editorState.graphics.animatedShaderProgram, mesh->meshVertexArray);
        } else {
            describeVertexAttributes(editorState.graphics.posUVVertexAttributes, mesh->meshVertexBuffer, editorState.graphics.shaderProgram, mesh->meshVertexArray);
        }

        meshGroup->meshes.push_back(mesh);
    }
    editorState.importedMeshGroups.push_back(meshGroup);
}

void initEditor(EditorState& editorState) {
#ifdef RENDERER_GL46
    editorState.graphics.shaderProgram = createShaderProgram(vshader_glsl, fshader_glsl);
#endif
#ifdef RENDERER_DX11
    editorState.graphics.shaderProgram = createShaderProgram(vshader_hlsl, fshader_hlsl);
    editorState.graphics.shaderProgram3D = createShaderProgram(vshader_hlsl, fshader_3d_hlsl);
    editorState.graphics.animatedShaderProgram = createShaderProgram(vshader_hlsl_animated, fshader_hlsl);
    editorState.graphics.textShaderProgram = createShaderProgram(vshader_hlsl, fs_text_hlsl);
    editorState.graphics.lineShaderProgram = createShaderProgram(vshader_line_hlsl, fshader_color_hlsl);
#endif

    std::vector<float> tri_vertices = {
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
         -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
        0.5, 0.5, 0.0f, 1.0f, 1.0f

    };


    std::vector<uint32_t> tri_indices = {
        0,2, 1,
        1, 2, 3
    };



    // Our "current" default layout: pos|uv
    editorState.graphics.posUVVertexAttributes =  {
        {"POSITION", 0, 3, DataType::Float, sizeof(float) * 5,
            0 },

        {"TEXCOORD", 0, 2, DataType::Float, sizeof(float) * 5,
            sizeof(float) * 3 }
    };

    // Simplified pos only layout, e.g. for lines (grid):
    editorState.graphics.positionOnlyVertexAttributes =  {
        {"POSITION", 0, 3, DataType::Float, sizeof(float) * 3,
            0 },
    };

    // For animated meshes
    editorState.graphics.animatedVertexAttributes =  {
                {"POSITION", 0, 3, DataType::Float, sizeof(float) * 13,
                0 },

                {"TEXCOORD", 0, 2, DataType::Float, sizeof(float) * 13,
                sizeof(float) * 3 },

                {"BLENDWEIGHT", 0, 4, DataType::Float, sizeof(float) * 13,
                    sizeof(float) * 5 },
                {"BLENDINDICES", 0, 4, DataType::Float, sizeof(float) * 13,
                    sizeof(float) * 9 },
    };

    editorState.vertexAttributesAnimated = editorState.graphics.animatedVertexAttributes;
    editorState.vertexAttributes = editorState.graphics.posUVVertexAttributes;

    // Prepare text render meshes:
    editorState.graphics.fontHandle = createFont("assets/Inter_18pt-Light.ttf", 16);
    editorState.graphics.fontHandleBig = createFont("assets/Inter_18pt-Light.ttf", 18);
    editorState.textMesh = createTextMesh(editorState.graphics.fontHandle, "Long placeholder text so updates will fit in nicely");
    editorState.menuTextMeshes.tmFile = createTextMesh(editorState.graphics.fontHandleBig, "File");
    editorState.menuTextMeshes.tmGameObjects = createTextMesh(editorState.graphics.fontHandleBig, "GameObjects");
    editorState.menuTextMeshes.tmSettings = createTextMesh(editorState.graphics.fontHandleBig, "Settings");
    editorState.menuTextMeshes.tmModelImport = createTextMesh(editorState.graphics.fontHandleBig, "Import");
    editorState.topMenuItems.push_back(new MenuItem{"File",  editorState.menuTextMeshes.tmFile });
    editorState.topMenuItems.push_back(new MenuItem{"Import", editorState.menuTextMeshes.tmModelImport, {}, importModelAction});
    editorState.topMenuItems.push_back(new MenuItem{"GameObjects", editorState.menuTextMeshes.tmGameObjects});
    editorState.topMenuItems.push_back(new MenuItem{"Settings", editorState.menuTextMeshes.tmSettings});

    // Prepare an offscreen framebuffer for the 3d scene:
    editorState.graphics.frameBuffer3DPanel = createFrameBuffer(editorState.screen_width - 200 * 2, editorState.screen_height - 64, true);
    editorState.graphics.frameBufferMainTabPanel = createFrameBuffer(editorState.screen_width - 200 * 2, 32 , true);
    editorState.graphics.frameBufferTopMenu = createFrameBuffer(editorState.screen_width, 32, true);
    editorState.graphics.frameBufferStatusBar = createFrameBuffer(editorState.screen_width, 32, true);
    editorState.graphics.frameBufferGameObjectTree = createFrameBuffer(200, editorState.screen_height-64, true);
    editorState.graphics.frameBufferAssetPanel = createFrameBuffer(200, editorState.screen_height-64, true);
    editorState.graphics.frameBufferAnimationPanel = createFrameBuffer(editorState.screen_width - 200 * 2, 200, true);
    editorState.graphics.frameBufferLevelEditorPanel = createFrameBuffer(editorState.screen_width - 200 * 2, editorState.screen_height - 64, true);

    // Create constant buffers:
    editorState.graphics.skinningMatricesCBuffer = createConstantBuffer(sizeof(glm::mat4) * 50);  // TODO have one per skeleton and adjust to the number of actual joints
    editorState.graphics.objectTransformBuffer = createConstantBuffer(sizeof(glm::mat4));
    editorState.graphics.cameraTransformBuffer = createConstantBuffer(sizeof(glm::mat4) * 2);

    // Mesh import and vertex buffer creation etc.
    {
       importModelByFileName(editorState, "assets/test.glb");
       importModelByFileName(editorState, "assets/origin_post.glb");
    }

    // Root game object creation:
    // Holds all "real" user created game objects:
    editorState.rootGameObject = new GameObject();
    editorState.rootGameObject->name = "Root";

    // Create a few test game objects
    createTestGameObjects(editorState);

    editorState.graphics.quadVertexArray = createVertexArray();
    bindVertexArray(editorState.graphics.quadVertexArray);

    editorState.perspectiveCamera = new Camera();
    editorState.perspectiveCamera->location = {0, 2, -2};
    editorState.perspectiveCamera->lookAtTarget = {0, 0, 3};
    editorState.perspectiveCamera->view_matrix =editorState.perspectiveCamera->updateAndGetViewMatrix();
    editorState.perspectiveCamera->projection_matrix = editorState.perspectiveCamera->updateAndGetPerspectiveProjectionMatrix(65,
        editorState.screen_width, editorState.screen_height, 0.1, 100);
        // glm::perspectiveFovLH_ZO<float>(glm::radians(65.0f), editorState.screen_width,
        //     editorState.screen_height, 0.1, 100);

    editorState.orthoCamera = new Camera();
    editorState.orthoCamera->view_matrix = glm::mat4(1);
    editorState.orthoCamera->projection_matrix = (glm::orthoLH_ZO<float>(0, editorState.screen_width,  editorState.screen_height, 0.0f, 0.0, 50));

    // Quad creation
    {
        editorState.graphics.quadVertexBuffer = createVertexBuffer(tri_vertices.data(), tri_vertices.size() * sizeof(float), sizeof(float) * 5);
        associateVertexBufferWithVertexArray(editorState.graphics.quadVertexBuffer, editorState.graphics.quadVertexArray);
        editorState.graphics.quadIndexBuffer = createIndexBuffer(tri_indices.data(), tri_indices.size() * sizeof(uint32_t));
        associateIndexBufferWithVertexArray(editorState.graphics.quadIndexBuffer, editorState.graphics.quadVertexArray);
        describeVertexAttributes(editorState.graphics.posUVVertexAttributes, editorState.graphics.quadVertexBuffer, editorState.graphics.shaderProgram, editorState.graphics.quadVertexArray);
    }

    // Grid creation for mesh editor
    {
        editorState.graphics.meshEditorGridVertexArray = createVertexArray();
        bindVertexArray(editorState.graphics.meshEditorGridVertexArray);
        editorState.meshEditorGridLines = 11;
        auto grid_vertices_tuple = buildGridVertices(editorState.meshEditorGridLines);
        auto grid_vertices = std::get<0>(grid_vertices_tuple);
        auto grid_indices = std::get<1>(grid_vertices_tuple);
        editorState.graphics.meshEditorGridVertexBuffer = createVertexBuffer(grid_vertices.data(), grid_vertices.size() * sizeof(float), sizeof(float) * 3);
        associateVertexBufferWithVertexArray(editorState.graphics.meshEditorGridVertexBuffer, editorState.graphics.meshEditorGridVertexArray);
        editorState.graphics.meshEditorGridIndexBuffer = createIndexBuffer(grid_indices.data(), grid_indices.size() * sizeof(uint32_t));
        associateIndexBufferWithVertexArray(editorState.graphics.meshEditorGridIndexBuffer, editorState.graphics.meshEditorGridVertexArray);
        describeVertexAttributes(editorState.graphics.positionOnlyVertexAttributes, editorState.graphics.meshEditorGridVertexBuffer, editorState.graphics.lineShaderProgram, editorState.graphics.meshEditorGridVertexArray);
    }

    // Grid creation for level editor
    {
        editorState.graphics.levelEditorGridVertexArray = createVertexArray();
        bindVertexArray(editorState.graphics.levelEditorGridVertexArray);
        editorState.meshEditorGridLines = 200;
        auto grid_vertices_tuple = buildGridVertices(editorState.levelEditorGridLines);
        auto grid_vertices = std::get<0>(grid_vertices_tuple);
        auto grid_indices = std::get<1>(grid_vertices_tuple);
        editorState.graphics.levelEditorGridVertexBuffer = createVertexBuffer(grid_vertices.data(), grid_vertices.size() * sizeof(float), sizeof(float) * 3);
        associateVertexBufferWithVertexArray(editorState.graphics.levelEditorGridVertexBuffer, editorState.graphics.levelEditorGridVertexArray);
        editorState.graphics.levelEditorGridIndexBuffer = createIndexBuffer(grid_indices.data(), grid_indices.size() * sizeof(uint32_t));
        associateIndexBufferWithVertexArray(editorState.graphics.levelEditorGridIndexBuffer, editorState.graphics.levelEditorGridVertexArray);
        describeVertexAttributes(editorState.graphics.positionOnlyVertexAttributes, editorState.graphics.levelEditorGridVertexBuffer, editorState.graphics.lineShaderProgram, editorState.graphics.levelEditorGridVertexArray);
    }

    int image_width, image_height;
    auto pixels = load_image("assets/debug_texture.jpg", &image_width, &image_height);
    editorState.graphics.textureHandle = createTexture(image_width, image_height, pixels, 4);
    editorState.texturePool["debug_texture"] = editorState.graphics.textureHandle;

    auto pixelsdj = load_image("assets/debug_joint_texture.png", &image_width, &image_height);
    editorState.graphics.jointDebugTexture = createTexture(image_width, image_height, pixelsdj, 4);

    pixelsdj = load_image("assets/debug_texture_2.png", &image_width, &image_height);
    editorState.texturePool["testmesh4_texture"] = createTexture(image_width, image_height, pixelsdj, 4);

    pixelsdj = load_image("assets/Ch15_1002_Diffuse.png", &image_width, &image_height);
    editorState.texturePool["ch15"] = createTexture(image_width, image_height, pixelsdj, 4);

    editorState.texturePool["gray_bg"] = createTextureFromFile("assets/gray_bg.png");
    editorState.texturePool["light_gray_bg"] = createTextureFromFile("assets/light_gray_bg.png");
    editorState.texturePool["mid_blue_bg"] = createTextureFromFile("assets/mid_blue_bg.png");
    editorState.texturePool["expand_start"] = createTextureFromFile("assets/expand_start.png");
    editorState.texturePool["expand_active"] = createTextureFromFile("assets/expand_active.png");

    // Create for now a tab with the "current" level editor:
    // TODO in the future we want to be able to edit multiple levels in separate tabs.

    createTestProject(editorState);

    auto currentLevelTab = new Tab{"Level1"};
    currentLevelTab->type = TabType::Level;
    currentLevelTab->tabHeader.titleTextMesh = createTextMesh(editorState.graphics.fontHandle, "Level1");
    currentLevelTab->level = editorState.project->levels[0];
    editorState.mainTabs.push_back(currentLevelTab);

    editorState.currentMainTabTitle = "Level1";

}





int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev_iinst, LPSTR, int) {
    int width = 1280;
    int height = 720;
    auto window = Win32Window(hInstance, SW_SHOWDEFAULT, L"my window", width, height);
    initGraphics(window, false, 0);

    // Shader initialization
    // auto vs = createShader(vshader_code, ShaderType::Vertex);
    // auto fs = createShader(fshader_code, ShaderType::Fragment);

    EditorState editorState;
    editorState.window = &window;
    editorState.screen_width = width;
    editorState.screen_height = height;
    initEditor(editorState);

    editorState.frameTimer = new FrameTimer(600);;

    bool running = true;
    while (running) {
        auto start_tok = window.performance_count();
        running = window.process_messages();

        do_frame(window, editorState);

        auto end_tok = window.performance_count();
        frame_time = window.measure_time_in_seconds(start_tok, end_tok);
        editorState.frameTimer->addFrameTime(frame_time);

#ifdef _PERF_MEASURE
        std::cout << "frametime: " << frame_time << std::endl;
#endif

    }
    return 0;
}

