#include "vk_image.h"

#include "core/logger.h"
#include "core/memory.h"
#include "vulkan/vulkan_core.h"

bool vulkan_image_create(VulkanContext* context, VkImageType image_type,
                         VkFormat format, VkExtent3D extent,
                         uint32_t mip_levels, uint32_t array_layers,
                         VkSampleCountFlagBits samples, VkImageTiling tiling,
                         VkImageUsageFlags usage,
                         VkMemoryPropertyFlags memory_properties,
                         VulkanImage* out_image) {
    memory_set(out_image, 0, sizeof(VulkanImage));

    VkImageCreateInfo image_ci = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                  .imageType = image_type,
                                  .format = format,
                                  .extent = extent,
                                  .mipLevels = mip_levels,
                                  .arrayLayers = array_layers,
                                  .samples = samples,
                                  .tiling = tiling,
                                  .usage = usage,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

    VkResult result = vkCreateImage(context->device.device, &image_ci,
                                    context->allocator, &out_image->handle);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create image: %d", result);
        return false;
    };

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(context->device.device, out_image->handle,
                                 &mem_req);

    int32_t mem_type = context->find_memory_index(
        context, mem_req.memoryTypeBits, memory_properties);
    if (mem_type == -1) {
        LOG_ERROR("Failed to find suitable memory type for image");
        vkDestroyImage(context->device.device, out_image->handle,
                       context->allocator);
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = (uint32_t)mem_type};

    result = vkAllocateMemory(context->device.device, &alloc_info,
                              context->allocator, &out_image->memory);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate image memory: %d", result);
        vkDestroyImage(context->device.device, out_image->handle,
                       context->allocator);
        return false;
    }

    result = vkBindImageMemory(context->device.device, out_image->handle,
                               out_image->memory, 0);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to bind image memory: %d", result);
        vkDestroyImage(context->device.device, out_image->handle,
                       context->allocator);
        vkFreeMemory(context->device.device, out_image->memory,
                     context->allocator);
        return false;
    }

    // Store metadata for later use
    out_image->format = format;
    out_image->extent = extent;
    out_image->mip_levels = mip_levels;
    out_image->array_layers = array_layers;
    out_image->usage = usage;
    out_image->memory_properties = memory_properties;

    return true;
}

bool vulkan_image_view_create(VulkanContext* context, const VulkanImage* image,
                              VkImageViewType view_type,
                              VkImageAspectFlags aspect_flags,
                              uint32_t base_mip_level, uint32_t level_count,
                              uint32_t base_array_layer, uint32_t layer_count,
                              VkImageView* out_view) {
    VkImageViewCreateInfo view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image->handle,
        .viewType = view_type,
        .format = image->format,
        .subresourceRange = {.aspectMask = aspect_flags,
                             .baseMipLevel = base_mip_level,
                             .levelCount = level_count,
                             .baseArrayLayer = base_array_layer,
                             .layerCount = layer_count}};

    VkResult result = vkCreateImageView(context->device.device, &view_ci,
                                        context->allocator, out_view);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create image view: %d", result);
        return false;
    }
    return true;
}

void vulkan_image_destroy(VulkanContext* context, VulkanImage* image) {
    if (!image) return;

    if (image->view != VK_NULL_HANDLE) {
        vkDestroyImageView(context->device.device, image->view,
                           context->allocator);
        image->view = VK_NULL_HANDLE;
    }
    if (image->handle != VK_NULL_HANDLE) {
        vkDestroyImage(context->device.device, image->handle,
                       context->allocator);
        image->handle = VK_NULL_HANDLE;
    }
    if (image->memory != VK_NULL_HANDLE) {
        vkFreeMemory(context->device.device, image->memory, context->allocator);
        image->memory = VK_NULL_HANDLE;
    }
}
