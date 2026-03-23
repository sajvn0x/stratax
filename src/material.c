#include "material.h"

#include "core/logger.h"
#include "core/memory.h"
#include "vk/vk_image.h"
#include "vk/vk_pipeline.h"

bool material_create(VulkanContext* context, const char* vert_path,
                     const char* frag_path, const VulkanImage* texture,
                     VkSampler sampler, VkDescriptorSetLayout global_ubo_layout,
                     Material* out_mat) {
    // resource descriptors
    VkDescriptorSetLayoutBinding tex_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

    VkDescriptorSetLayoutCreateInfo tex_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &tex_binding};

    VkResult result = vkCreateDescriptorSetLayout(
        context->device.device, &tex_layout_info, context->allocator,
        &out_mat->texture_layout);

    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create texture descriptor set layout");
        return false;
    }

    VkDescriptorSetLayout descriptor_layouts[] = {global_ubo_layout,
                                                  out_mat->texture_layout};

    // graphics pipeline
    PipelineConfig config = pipeline_config_default();
    config.stages =
        memory_allocate(sizeof(VulkanShaderStageInfo) * 2, MEMORY_TAG_VULKAN);
    config.stages[0].path = vert_path;
    config.stages[0].entry_point = "main";
    config.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    config.stages[1].path = frag_path;
    config.stages[1].entry_point = "main";
    config.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    config.stage_count = 2;

    // vertex input
    config.binding_count = 1;
    config.vertex_bindings =
        memory_allocate(sizeof(VulkanVertexBinding) * 1, MEMORY_TAG_VULKAN);
    config.vertex_bindings[0].binding = 0;
    config.vertex_bindings[0].stride = sizeof(Vertex);
    config.vertex_bindings[0].input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
    config.vertex_bindings[0].attributes =
        memory_allocate(sizeof(VulkanVertexAttribute) * 3, MEMORY_TAG_VULKAN);
    config.vertex_bindings[0].attributes[0].location = 0;
    config.vertex_bindings[0].attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    config.vertex_bindings[0].attributes[0].offset = offsetof(Vertex, pos);
    config.vertex_bindings[0].attributes[1].location = 1;
    config.vertex_bindings[0].attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    config.vertex_bindings[0].attributes[1].offset = offsetof(Vertex, color);
    config.vertex_bindings[0].attributes[2].location = 2;
    config.vertex_bindings[0].attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    config.vertex_bindings[0].attributes[2].offset =
        offsetof(Vertex, tex_coord);
    config.vertex_bindings[0].attribute_count = 3;

    // dynamic states
    config.dynamic_states =
        memory_allocate(sizeof(VkDynamicState) * 2, MEMORY_TAG_VULKAN);
    config.dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    config.dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    config.dynamic_state_count = 2;

    // resource descriptors
    config.descriptor_set_layout_count = 2;
    config.descriptor_set_layouts =
        memory_allocate(sizeof(VkDescriptorSetLayout) * 2, MEMORY_TAG_VULKAN);
    config.descriptor_set_layouts = descriptor_layouts;
    // config.descriptor_set_layouts[0] = descriptor_set_layout;
    // config.descriptor_set_layouts[1] = descriptor_set_layout_tex;

    if (!vulkan_pipeline_create(context, &config, &out_mat->pipeline)) {
        LOG_ERROR("Failed to create graphics pipeline");
        vulkan_pipeline_config_cleanup(&config);
        return false;
    }

    // vulkan_pipeline_config_cleanup(&config);

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context->device.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &out_mat->texture_layout};

    VkDescriptorSetAllocateInfo tex_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context->device.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &out_mat->texture_layout};
    result = vkAllocateDescriptorSets(context->device.device, &tex_alloc,
                                      &out_mat->texture_set);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate texture descriptor set: %d", result);
        return false;
    }

    VkDescriptorImageInfo image_info = {
        .sampler = sampler,
        .imageView = texture->view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = out_mat->texture_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info};
    vkUpdateDescriptorSets(context->device.device, 1, &write, 0, NULL);

    return true;
}

void material_destroy(VulkanContext* context, Material* mat) {
    vulkan_pipeline_destroy(context, &mat->pipeline);

    if (mat->texture_layout)
        vkDestroyDescriptorSetLayout(context->device.device,
                                     mat->texture_layout, context->allocator);
}
