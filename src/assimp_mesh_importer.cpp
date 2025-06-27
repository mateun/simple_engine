//
// Created by mgrus on 27.06.2025.
//

#include <cassert>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include "engine.h"

std::vector<MeshImportData*> importMeshFromFile(const std::string &fileName) {
    Assimp::Importer importer;
    auto scene = importer.ReadFile(fileName, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_CalcTangentSpace );
    assert(scene != nullptr);

    std::vector<MeshImportData*> meshImportDatas;
    for (int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        auto mesh = scene->mMeshes[meshIndex];
        auto meshImportData = new MeshImportData();
        meshImportDatas.push_back(meshImportData);

        meshImportData->meshName = scene->mMeshes[meshIndex]->mName.C_Str();
        meshImportData->stride = sizeof(float) * 6; // pos + normals, always.
        if (mesh->mTextureCoords[0]) {
            meshImportData->stride += sizeof(float) * 2;    // uvs, optional
        }

        for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++) {
            meshImportData->posMasterList.push_back({mesh->mVertices[vertexIndex].x,
                                     mesh->mVertices[vertexIndex].y,
                                     mesh->mVertices[vertexIndex].z});
            meshImportData->posFlat.push_back(mesh->mVertices[vertexIndex].x);
            meshImportData->posFlat.push_back(mesh->mVertices[vertexIndex].y);
            meshImportData->posFlat.push_back(mesh->mVertices[vertexIndex].z);

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
    }
    return meshImportDatas;

}

