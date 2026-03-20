#include "vk_buffer.h"

#include "core/logger.h"
#include "core/memory.h"

static bool create_staging_buffer(VulkanContext* context, VkDeviceSize size,
                                  void* data, VulkanBuffer* staging_buffer) {
    VulkanBufferCreateInfo staging_info = {
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        .initial_data = data,
        .persistent_mapped = false};

    return vulkan_buffer_create(context, &staging_info, staging_buffer);
}

bool vulkan_buffer_create(VulkanContext* context,
                          const VulkanBufferCreateInfo* create_info,
                          VulkanBuffer* out_buffer) {
    if (create_info->size == 0) {
        LOG_ERROR("Buffer size cannot be zero");
        return false;
    }

    VkDevice device = context->device.device;
    memory_set(out_buffer, 0, sizeof(VulkanBuffer));

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = create_info->size,
        .usage = create_info->usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

    VkResult result = vkCreateBuffer(device, &buffer_info, context->allocator,
                                     &out_buffer->handle);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create buffer: %d", result);
        return false;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, out_buffer->handle,
                                  &mem_requirements);

    i32 memory_type =
        context->find_memory_index(context, mem_requirements.memoryTypeBits,
                                   create_info->memory_properties);
    if (memory_type == -1) {
        vkDestroyBuffer(device, out_buffer->handle, context->allocator);
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type};

    result = vkAllocateMemory(device, &alloc_info, context->allocator,
                              &out_buffer->memory);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate buffer memory: %d", result);
        vkDestroyBuffer(device, out_buffer->handle, context->allocator);
        return false;
    }

    result =
        vkBindBufferMemory(device, out_buffer->handle, out_buffer->memory, 0);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to bind buffer memory: %d", result);
        vkFreeMemory(device, out_buffer->memory, context->allocator);
        vkDestroyBuffer(device, out_buffer->handle, context->allocator);
        return false;
    }

    out_buffer->size = create_info->size;
    out_buffer->usage = create_info->usage;
    out_buffer->memory_properties = create_info->memory_properties;

    if (create_info->initial_data) {
        // Check if we can map directly
        if (create_info->memory_properties &
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            // Direct mapping for host-visible memory
            void* data;
            result = vkMapMemory(device, out_buffer->memory, 0,
                                 create_info->size, 0, &data);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to map memory for initial data: %d", result);
                vulkan_buffer_destroy(context, out_buffer);
                return false;
            }

            memory_copy(data, create_info->initial_data, create_info->size);

            if (!(create_info->memory_properties &
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                // Flush if not coherent
                VkMappedMemoryRange range = {
                    .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                    .memory = out_buffer->memory,
                    .offset = 0,
                    .size = create_info->size};
                vkFlushMappedMemoryRanges(device, 1, &range);
            }

            if (!create_info->persistent_mapped) {
                vkUnmapMemory(device, out_buffer->memory);
            } else {
                out_buffer->mapped = data;
            }
        } else {
            VulkanBuffer staging_buffer;
            if (!create_staging_buffer(context, create_info->size,
                                       create_info->initial_data,
                                       &staging_buffer)) {
                LOG_ERROR("Failed to create staging buffer for initial data");
                vulkan_buffer_destroy(context, out_buffer);
                return false;
            }

            vulkan_buffer_copy(context, &staging_buffer, out_buffer,
                               create_info->size, 0, 0);

            vulkan_buffer_destroy(context, &staging_buffer);
        }
    }

    if (create_info->persistent_mapped &&
        (create_info->memory_properties &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        !out_buffer->mapped) {
        result = vkMapMemory(device, out_buffer->memory, 0, create_info->size,
                             0, &out_buffer->mapped);
        if (result != VK_SUCCESS) {
            LOG_WARN("Failed to persistently map buffer: %d", result);
            // Non-fatal, continue without mapping
            out_buffer->mapped = NULL;
        }
    }

    LOG_INFO("Buffer created: size=%llu, usage=%u, memory=%u",
             (unsigned long long)create_info->size, create_info->usage,
             create_info->memory_properties);

    return true;
}

void vulkan_buffer_destroy(VulkanContext* context, VulkanBuffer* buffer) {
    if (!context || !buffer) return;

    VkDevice device = context->device.device;

    if (buffer->mapped) {
        vkUnmapMemory(device, buffer->memory);
        buffer->mapped = NULL;
    }

    if (buffer->memory) {
        vkFreeMemory(device, buffer->memory, context->allocator);
        buffer->memory = VK_NULL_HANDLE;
    }

    if (buffer->handle) {
        vkDestroyBuffer(device, buffer->handle, context->allocator);
        buffer->handle = VK_NULL_HANDLE;
    }

    LOG_INFO("Buffer destroyed");
}

bool vulkan_buffer_upload_data(VulkanContext* context, VulkanBuffer* buffer,
                               void* data, VkDeviceSize size,
                               VkDeviceSize offset) {
    if (!context || !buffer || !data) return false;

    // Validate range
    if (offset + size > buffer->size) {
        LOG_ERROR("Upload range exceeds buffer size");
        return false;
    }

    VkDevice device = context->device.device;

    // If buffer is host-visible, map and copy directly
    if (buffer->memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        void* mapped_data;
        VkResult result;

        if (buffer->mapped) {
            // Use persistent mapping
            mapped_data = (char*)buffer->mapped + offset;
        } else {
            // Map temporarily
            result = vkMapMemory(device, buffer->memory, offset, size, 0,
                                 &mapped_data);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to map memory for upload: %d", result);
                return false;
            }
        }

        memory_copy(mapped_data, data, size);

        if (!(buffer->memory_properties &
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            // Flush if not coherent
            VkMappedMemoryRange range = {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = buffer->memory,
                .offset = offset,
                .size = size};
            vkFlushMappedMemoryRanges(device, 1, &range);
        }

        if (!buffer->mapped) {
            vkUnmapMemory(device, buffer->memory);
        }

        return true;
    }
    // For device-local buffers, need a staging buffer
    else {
        // Create temporary staging buffer
        VulkanBuffer staging;
        VulkanBufferCreateInfo staging_info = {
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            .initial_data = data,
            .persistent_mapped = false};

        if (!vulkan_buffer_create(context, &staging_info, &staging)) {
            LOG_ERROR("Failed to create staging buffer for upload");
            return false;
        }

        vulkan_buffer_copy(context, &staging, buffer, size, 0, offset);

        vulkan_buffer_destroy(context, &staging);

        return true;
    }
}

void vulkan_buffer_copy(VulkanContext* context, VulkanBuffer* src,
                        VulkanBuffer* dst, VkDeviceSize size,
                        VkDeviceSize src_offset, VkDeviceSize dst_offset) {
    if (!context || !src || !dst) return;

    // Validate that both buffers have transfer usage
    if (!(src->usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) {
        LOG_ERROR("Source buffer missing TRANSFER_SRC usage");
        return;
    }
    if (!(dst->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
        LOG_ERROR("Destination buffer missing TRANSFER_DST usage");
        return;
    }

    // Create a one-time command buffer for the copy
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context->device.graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(context->device.device, &alloc_info,
                             &command_buffer);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy_region = {
        .srcOffset = src_offset, .dstOffset = dst_offset, .size = size};

    vkCmdCopyBuffer(command_buffer, src->handle, dst->handle, 1, &copy_region);

    vkEndCommandBuffer(command_buffer);

    // Submit and wait
    VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .commandBufferCount = 1,
                                .pCommandBuffers = &command_buffer};

    vkQueueSubmit(context->device.graphics_queue, 1, &submit_info,
                  VK_NULL_HANDLE);
    vkQueueWaitIdle(context->device.graphics_queue);

    vkFreeCommandBuffers(context->device.device,
                         context->device.graphics_command_pool, 1,
                         &command_buffer);
}
