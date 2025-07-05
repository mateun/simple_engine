//
// Created by mgrus on 27.06.2025.
//

#include <cassert>
#include <iostream>
#include <map>
#include <stack>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include "engine.h"
#include "stb_image.h"
#include "glm/gtc/type_ptr.hpp"

static glm::mat4 matrixAssimpToGLM(const aiMatrix4x4& from) {

    // Create a glm::mat4 from aiMatrix4x4 data
    glm::mat4 result = glm::make_mat4(&from.a1);

    // Since Assimp's aiMatrix4x4 is row-major and GLM's glm::mat4 is column-major,
    // we need to transpose the matrix to correctly convert it.
    result = glm::transpose(result);
    return result;



}

bool isLeafJoint(const std::string& jointName, aiMesh* mesh) {
    for (int b = 0; b < mesh->mNumBones; b++) {
        auto bone = mesh->mBones[b];
        if (std::string(bone->mName.C_Str()) == jointName) {
            return true;
        }
    }

    return false;
}

bool isAnimatedJoint(const std::string& name, const aiScene* scene) {
    for (int an = 0; an < scene->mNumAnimations; an++) {
        auto animation = scene->mAnimations[an];
        for (int bn = 0; bn < animation->mNumChannels; bn++) {
            auto channel = animation->mChannels[bn];
            if (std::string(channel->mNodeName.C_Str()) == name) {
                return true;
            }
        }
    }

    return false;
}


std::vector<MeshData*> importMeshFromFile(const std::string &fileName) {
    Assimp::Importer importer;
    auto scene = importer.ReadFile(fileName, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs | aiProcess_LimitBoneWeights | aiProcess_CalcTangentSpace);
    assert(scene != nullptr);

    std::vector<MeshData*> meshImportDatas;
    for (int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        auto mesh = scene->mMeshes[meshIndex];
        auto meshImportData = new MeshData();
        meshImportDatas.push_back(meshImportData);

        // Material linkage:
        auto ai_material = scene->mMaterials[mesh->mMaterialIndex];
        meshImportData->materialName = ai_material->GetName().C_Str();
        aiString diffuseTexturePath;
        aiString normalMapPath;
        ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuseTexturePath);
        ai_material->GetTexture(aiTextureType_NORMALS, 0, &normalMapPath);
        // Check for embedded texture
        if (diffuseTexturePath.C_Str()[0] == '*') {
            auto textureIndex = std::atoi(diffuseTexturePath.C_Str() + 1);
            auto texture = scene->mTextures[textureIndex];

            // Compressed?
            if (texture->mHeight == 0) {
                const uint8_t* imageData = (const uint8_t*)texture->pcData;
                size_t imageSize = texture->mWidth;
                int w, h, channels;
                uint8_t* pixels = stbi_load_from_memory(imageData, imageSize, &w, &h, &channels, 4);
                if (!pixels) {
                    std::cerr << "failed to load embedded texture" << std::endl;
                    // TODO error handling
                } else {
                    meshImportData->diffuseTexture = createTexture(w, h, pixels, 4);
                    stbi_image_free(pixels);
                }
            }


        } else {
            // External texture file, so we store the path
            // TODO should we anyway retrieve the texture right away?
            // How to handle atlases? Many meshes will use the same texture,
            // so we might want to decide this not here in the importer, what to do with this texture.
            meshImportData->diffuseTexturePath = diffuseTexturePath.C_Str();

        }
        if (normalMapPath.C_Str()[0] == '*') {
            auto normal_texture = scene->GetEmbeddedTexture(normalMapPath.C_Str());
            if (normal_texture) {
                meshImportData->normalMapPath = normal_texture->mFilename.C_Str() + std::string(".") + normal_texture->achFormatHint;
            }
        }

        meshImportData->meshName = scene->mMeshes[meshIndex]->mName.C_Str();
        meshImportData->stride = sizeof(float) * 6; // pos + normals, always.
        if (mesh->mTextureCoords[0]) {
            meshImportData->stride += sizeof(float) * 2;    // uvs, optional
        }

        for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++) {
            meshImportData->posMasterList.push_back({
                                        -mesh->mVertices[vertexIndex].x,
                                        mesh->mVertices[vertexIndex].y,
                                        -mesh->mVertices[vertexIndex].z});
            meshImportData->posFlat.push_back(-mesh->mVertices[vertexIndex].x);
            meshImportData->posFlat.push_back(mesh->mVertices[vertexIndex].y);
            meshImportData->posFlat.push_back(-mesh->mVertices[vertexIndex].z);

            if (mesh->mTangents) {
                meshImportData->tangentMasterList.push_back({
                    mesh->mTangents[vertexIndex].x,
                    mesh->mTangents[vertexIndex].y,
                    mesh->mTangents[vertexIndex].z,
                });

                meshImportData->tangentsFlat.push_back(mesh->mTangents[vertexIndex].y);
                meshImportData->tangentsFlat.push_back(mesh->mTangents[vertexIndex].x);
                meshImportData->tangentsFlat.push_back(mesh->mTangents[vertexIndex].z);


            }

            if (mesh->HasNormals()) {
                meshImportData->normalMasterList.push_back({mesh->mNormals[vertexIndex].x,
                                            mesh->mNormals[vertexIndex].y,
                                            mesh->mNormals[vertexIndex].z});

                meshImportData->normFlat.push_back(mesh->mNormals[vertexIndex].x);
                meshImportData->normFlat.push_back(mesh->mNormals[vertexIndex].y);
                meshImportData->normFlat.push_back(mesh->mNormals[vertexIndex].z);
            }

            if (mesh->mTextureCoords[0]) {
                meshImportData->uvMasterList.push_back({mesh->mTextureCoords[0][vertexIndex].x,
                                        mesh->mTextureCoords[0][vertexIndex].y});

                meshImportData->uvsFlat.push_back(mesh->mTextureCoords[0][vertexIndex].x);
                meshImportData->uvsFlat.push_back(mesh->mTextureCoords[0][vertexIndex].y);

            }

        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                meshImportData->indicesFlat.push_back(face.mIndices[j]);
                meshImportData->posIndexSortedMasterList.push_back(meshImportData->posMasterList[face.mIndices[j]]);
            }
        }





        // /////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Skeleton import:
        if (mesh->HasBones()) {
            std::map<std::string, Joint*> nameToJoint;
            meshImportData->skeleton = new Skeleton();


            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // Animation import
            if (scene->HasAnimations()) {
                for (int animIndex = 0; animIndex < scene->mNumAnimations; animIndex++) {
                    auto animation = scene->mAnimations[animIndex];
                    auto myAnimation = new Animation();
                    meshImportData->skeleton->animations.push_back(myAnimation);
                    myAnimation->name = scene->mAnimations[animIndex]->mName.C_Str();
                    myAnimation->duration = scene->mAnimations[animIndex]->mDuration / scene->mAnimations[animIndex]->mTicksPerSecond;
                    myAnimation->ticksPerSecond = scene->mAnimations[animIndex]->mTicksPerSecond;

                    for (int channelIndex = 0; channelIndex < animation->mNumChannels; channelIndex++) {
                        auto channel = animation->mChannels[channelIndex];
                        auto node = scene->mRootNode->FindNode(channel->mNodeName.C_Str());
                        JointAnimationTrack jointAnimTrack;
                        jointAnimTrack.positionKeys = std::vector<aiVectorKey>(channel->mPositionKeys, channel->mPositionKeys + channel->mNumPositionKeys);
                        jointAnimTrack.rotationKeys = std::vector<aiQuatKey>(channel->mRotationKeys, channel->mRotationKeys + channel->mNumRotationKeys);
                        jointAnimTrack.scaleKeys = std::vector<aiVectorKey>(channel->mScalingKeys, channel->mScalingKeys + channel->mNumScalingKeys);
                        myAnimation->jointAnimationTracks[channel->mNodeName.C_Str()] = jointAnimTrack;
                    }

                }

            }


            // Gather all relevant bones, these are either
            // - leaf bones
            // - part of an animation
            // - be the parent of a node in either category
            std::stack<aiNode*> stack;
            stack.push(scene->mRootNode);
            aiNode* lastParent = nullptr;
            while (!stack.empty()) {
                aiNode* node = stack.top();
                lastParent = node;
                stack.pop();


                // Collect all leaf and animation joints:
                if (isLeafJoint(node->mName.C_Str(), mesh) || isAnimatedJoint(node->mName.C_Str(), scene)) {
                    auto joint = new Joint();
                    joint->name = node->mName.C_Str();
                    nameToJoint[joint->name] = joint;

                }

                for (int i = static_cast<int>(node->mNumChildren) - 1; i >= 0; --i) {
                    auto child = node->mChildren[i];
                    std::string name = child->mName.C_Str();
                    // if (!name.contains("DEF")) {
                    //     continue;
                    // }
                    stack.push(child);
                    if (isLeafJoint(child->mName.C_Str(), mesh) || isAnimatedJoint(child->mName.C_Str(), scene)) {
                        // Now lets check if this nodes parent was so far not represented as a joint:
                        // BUT: ignore IK bones! "ik" "IK" etc.

                        if (nameToJoint[lastParent->mName.C_Str()] == nullptr) {
                            auto parentJoint = new Joint();
                            parentJoint->name = lastParent->mName.C_Str();
                            nameToJoint[parentJoint->name] = parentJoint;

                        }
                    }
                }
            }

            std::map<int, std::vector<float>> vertexJointWeights;
            std::map<int, std::vector<uint32_t>> vertexJointIndices;

            // Fill basic information per joint:
            uint32_t jointIndex = 0;
            for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                auto bone = mesh->mBones[b];
                auto joint = new Joint();
                meshImportData->skeleton->joints.push_back(joint);
                auto node = scene->mRootNode->FindNode(bone->mName.C_Str());
                joint->name = bone->mName.C_Str();
                nameToJoint[joint->name] = joint;
                joint->inverseBindMatrix = matrixAssimpToGLM(bone->mOffsetMatrix);
                joint->localTransform = matrixAssimpToGLM(node->mTransformation);

                for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                    auto weightData = bone->mWeights[w];
                    auto vertexIndex = weightData.mVertexId;
                    auto weight = weightData.mWeight;

                    if (vertexJointIndices.find(vertexIndex) == vertexJointIndices.end()) {
                        vertexJointIndices[vertexIndex] = {};
                        vertexJointIndices[vertexIndex].push_back(jointIndex);
                        vertexJointWeights[vertexIndex] = {};
                        vertexJointWeights[vertexIndex].push_back(weight);
                    } else {
                        // Check if we are already at 4 entries per vertex.
                        // If yes, stop filling more data:
                        auto num_indices = vertexJointIndices[vertexIndex].size();
                        if (num_indices < 4) {
                            std::cout << "vertex " << vertexIndex << "has  " << num_indices << " indices" << std::endl;
                            vertexJointIndices[vertexIndex].push_back(jointIndex);
                            vertexJointWeights[vertexIndex].push_back(weight);
                        }
                    }

                }
                jointIndex++;
            }

            // Build per vertex joint weights/indices as vec4s:
            std::vector<glm::vec4> finalJointIndices;
            std::vector<glm::vec4> finalJointWeights;
            for (int vi = 0; vi < meshImportData->posMasterList.size(); vi++) {
                glm::vec4 indices(0.0);
                glm::vec4 weights(0.0);
                float checkSum = 0;
                for (int x = 0; x < vertexJointIndices[vi].size() && x < 4; x++) {
                    indices[x] = vertexJointIndices[vi][x];
                    weights[x] = vertexJointWeights[vi][x];
                    checkSum += weights[x];
                }
                if (checkSum <= 0.999 || checkSum >= 1.001) {
                    exit(1);
                }
                finalJointIndices.push_back(indices);
                finalJointWeights.push_back(weights);
            }

            meshImportData->jointIndices = finalJointIndices;
            meshImportData->jointWeights = finalJointWeights;

            for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                auto bone= mesh->mBones[b];
                auto node = scene->mRootNode->FindNode(bone->mName.C_Str());
                auto joint = nameToJoint[bone->mName.C_Str()];
                if (node->mParent && std::string(node->mParent->mName.C_Str()) != "Armature") {
                    auto parentJoint = nameToJoint[node->mParent->mName.C_Str()];
                    if (parentJoint) {
                        joint->parent = parentJoint;
                    }

                }

            }
        }
    }
    return meshImportDatas;

}

