//
// Created by mgrus on 27.06.2025.
//

#include <cassert>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


void importMeshFromFile(const std::string &fileName) {
    Assimp::Importer importer;
    auto scene = importer.ReadFile(fileName, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_CalcTangentSpace);
    assert(scene != nullptr);

}

