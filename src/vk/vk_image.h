#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include "vk/vk_types.h"

typedef struct {
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;
    VkFormat format;
    VkExtent3D extent;
    uint32_t mip_levels;
    uint32_t array_layers;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags memory_properties;
} VulkanImage;

bool vulkan_image_create(VulkanContext* context, VkImageType image_type,
                         VkFormat format, VkExtent3D extent,
                         uint32_t mip_levels, uint32_t array_layers,
                         VkSampleCountFlagBits samples, VkImageTiling tiling,
                         VkImageUsageFlags usage,
                         VkMemoryPropertyFlags memory_properties,
                         VulkanImage* out_image);

bool vulkan_image_view_create(VulkanContext* context, const VulkanImage* image,
                              VkImageViewType view_type,
                              VkImageAspectFlags aspect_flags,
                              uint32_t base_mip_level, uint32_t level_count,
                              uint32_t base_array_layer, uint32_t layer_count,
                              VkImageView* out_view);

void vulkan_image_destroy(VulkanContext* context, VulkanImage* image);

#endif  // VULKAN_IMAGE_H
