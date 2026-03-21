#ifndef VULKAN_MESH_H
#define VULKAN_MESH_H

#include "vk/vk_buffer.h"
#include "vk/vk_types.h"

typedef struct {
    VulkanBuffer vertex_buffer;
    VulkanBuffer index_buffer;
    u32 vertex_count;
    u32 index_count;
    VkIndexType index_type;
} Mesh;

bool mesh_create(VulkanContext* context, const Vertex* vertices,
                 uint32_t vertex_count, const void* indices,
                 uint32_t index_count, VkIndexType index_type, Mesh* out_mesh);

void mesh_destroy(VulkanContext* context, Mesh* mesh);
void mesh_draw(VkCommandBuffer cmd, const Mesh* mesh);

#endif  //  VULKAN_MESH_H
