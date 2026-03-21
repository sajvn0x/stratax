#include "vk_mesh.h"

#include "core/memory.h"
#include "vk/vk_buffer.h"
#include "vk/vk_types.h"

bool mesh_create(VulkanContext* context, const Vertex* vertices,
                 uint32_t vertex_count, const void* indices,
                 uint32_t index_count, VkIndexType index_type, Mesh* out_mesh) {
    memory_zero(out_mesh, sizeof(Mesh));

    VkDeviceSize vertex_buffer_size = vertex_count * sizeof(Vertex);
    VkDeviceSize index_buffer_size =
        index_count * (index_type == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t)
                                                          : sizeof(uint32_t));

    // vertex buffer
    VulkanBufferCreateInfo vb_info = {
        .size = vertex_buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .initial_data = (void*)vertices};
    if (!vulkan_buffer_create(context, &vb_info, &out_mesh->vertex_buffer)) {
        return false;
    }

    // index buffer
    VulkanBufferCreateInfo ib_info = {
        .size = index_buffer_size,
        .usage =
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .initial_data = (void*)indices};
    if (!vulkan_buffer_create(context, &ib_info, &out_mesh->index_buffer)) {
        // Clean up vertex buffer if index buffer creation fails
        vulkan_buffer_destroy(context, &out_mesh->vertex_buffer);
        return false;
    }

    out_mesh->vertex_count = vertex_count;
    out_mesh->index_count = index_count;
    out_mesh->index_type = index_type;

    return true;
}

void mesh_destroy(VulkanContext* context, Mesh* mesh) {
    if (!mesh) return;
    vulkan_buffer_destroy(context, &mesh->vertex_buffer);
    vulkan_buffer_destroy(context, &mesh->index_buffer);
    memory_zero(mesh, sizeof(Mesh));
}

void mesh_draw(VkCommandBuffer cmd, const Mesh* mesh) {
    if (!mesh || mesh->vertex_count == 0 || mesh->index_count == 0) return;

    VkBuffer vertex_buffers[] = {mesh->vertex_buffer.handle};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(cmd, mesh->index_buffer.handle, 0, mesh->index_type);

    vkCmdDrawIndexed(cmd, mesh->index_count, 1, 0, 0, 0);
}
