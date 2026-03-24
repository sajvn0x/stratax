#include "model.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stdlib.h>

#include "core/logger.h"
#include "core/memory.h"

static void process_mesh(VulkanContext* ctx, const struct aiMesh* assimp_mesh,
                         Mesh* out_mesh) {
    // count total vertices
    u32 vertex_count = assimp_mesh->mNumVertices;
    Vertex* vertices = (Vertex*)memory_allocate(vertex_count * sizeof(Vertex),
                                                MEMORY_TAG_RENDERER);

    // extract positions, texture coordinates, normals, etc.
    for (uint32_t i = 0; i < vertex_count; ++i) {
        // Position
        vertices[i].pos[0] = assimp_mesh->mVertices[i].x;
        vertices[i].pos[1] = assimp_mesh->mVertices[i].y;
        vertices[i].pos[2] = assimp_mesh->mVertices[i].z;

        // TODO: need to change this later
        vertices[i].color[0] = 1.0f;
        vertices[i].color[1] = 1.0f;
        vertices[i].color[2] = 1.0f;

        // texture coordinates (if present)
        if (assimp_mesh->mTextureCoords[0]) {
            vertices[i].tex_coord[0] = assimp_mesh->mTextureCoords[0][i].x;
            vertices[i].tex_coord[1] = assimp_mesh->mTextureCoords[0][i].y;
        } else {
            vertices[i].tex_coord[0] = 0.0f;
            vertices[i].tex_coord[1] = 0.0f;
        }
    }

    // count total indices
    uint32_t index_count = 0;
    for (uint32_t f = 0; f < assimp_mesh->mNumFaces; ++f) {
        index_count += assimp_mesh->mFaces[f].mNumIndices;
    }
    uint32_t* indices = (u32*)memory_allocate(index_count * sizeof(uint32_t),
                                              MEMORY_TAG_RENDERER);

    // fill index buffer
    uint32_t idx_offset = 0;
    for (uint32_t f = 0; f < assimp_mesh->mNumFaces; ++f) {
        const struct aiFace* face = &assimp_mesh->mFaces[f];
        for (uint32_t i = 0; i < face->mNumIndices; ++i) {
            indices[idx_offset++] = face->mIndices[i];
        }
    }

    bool success = mesh_create(ctx, vertices, vertex_count, indices,
                               index_count, VK_INDEX_TYPE_UINT32, out_mesh);

    memory_free(vertices, sizeof(Vertex) * vertex_count, MEMORY_TAG_RENDERER);
    memory_free(indices, sizeof(uint32_t), MEMORY_TAG_RENDERER);

    if (!success) {
        LOG_ERROR("Failed to create mesh from Assimp data");
    }
}

static void process_node(VulkanContext* ctx, const struct aiNode* node,
                         const struct aiScene* scene, Mesh** out_meshes,
                         uint32_t* mesh_counter, uint32_t max_meshes) {
    // process all meshes at this node
    for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        if (*mesh_counter >= max_meshes) {
            LOG_WARN("Maximum mesh count reached, ignoring extra meshes");
            return;
        }
        const struct aiMesh* assimp_mesh = scene->mMeshes[node->mMeshes[i]];
        process_mesh(ctx, assimp_mesh, &(*out_meshes)[*mesh_counter]);
        (*mesh_counter)++;
    }
    // recurse into children
    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
        process_node(ctx, node->mChildren[i], scene, out_meshes, mesh_counter,
                     max_meshes);
    }
}

bool model_load(VulkanContext* context, const char* filepath, Mesh** out_meshes,
                u32* out_mesh_count) {
    const struct aiScene* scene =
        aiImportFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs |
                                   aiProcess_GenNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode) {
        LOG_ERROR("Assimp error: %s", aiGetErrorString());
        return false;
    }

    u32 total_meshes = 0;
    for (u32 i = 0; i < scene->mNumMeshes; ++i) {
        total_meshes++;
    }

    if (total_meshes == 0) {
        LOG_ERROR("No meshes found in the model");
        aiReleaseImport(scene);
        return false;
    }

    Mesh* meshes = (Mesh*)memory_allocate(total_meshes * sizeof(Mesh),
                                          MEMORY_TAG_RENDERER);

    u32 mesh_counter = 0;
    process_node(context, scene->mRootNode, scene, &meshes, &mesh_counter,
                 total_meshes);

    aiReleaseImport(scene);

    *out_meshes = meshes;
    *out_mesh_count = mesh_counter;

    LOG_INFO("Loaded model with %u meshes", mesh_counter);

    return true;
}
