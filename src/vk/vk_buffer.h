#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include "vk_types.h"

typedef struct {
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memory_properties;
    void* initial_data;
    bool persistent_mapped;
} VulkanBufferCreateInfo;

typedef struct {
    VkBuffer handle;
    VkDeviceMemory memory;
    void* mapped;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memory_properties;
} VulkanBuffer;

bool vulkan_buffer_create(VulkanContext* context,
                          const VulkanBufferCreateInfo* create_info,
                          VulkanBuffer* out_buffer);

void vulkan_buffer_destroy(VulkanContext* context, VulkanBuffer* buffer);

bool vulkan_buffer_upload_data(VulkanContext* context, VulkanBuffer* buffer,
                               void* data, VkDeviceSize size,
                               VkDeviceSize offset);

void vulkan_buffer_copy(VulkanContext* context, VulkanBuffer* src,
                        VulkanBuffer* dst, VkDeviceSize size,
                        VkDeviceSize src_offset, VkDeviceSize dst_offset);

#endif  // VULKAN_BUFFER_H
