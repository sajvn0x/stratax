#ifndef MATERIAL_H
#define MATERIAL_H

#include <vulkan/vulkan_core.h>

#include "vk/vk_image.h"
#include "vk/vk_pipeline.h"
#include "vk/vk_types.h"

typedef struct {
    VulkanPipeline pipeline;
    VkDescriptorSetLayout texture_layout;
    VkDescriptorSet texture_set;
} Material;

bool material_create(VulkanContext* context, const char* vert_path,
                     const char* frag_path, const VulkanImage* texture,
                     VkSampler sampler, VkDescriptorSetLayout global_ubo_layout,
                     Material* out_mat);

void material_destroy(VulkanContext* context, Material* mat);

#endif  // MATERIAL_H
