#ifndef GRAPHICS_PIPELINE_H
#define GRAPHICS_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "core/defines.h"
#include "vk/vk_types.h"

typedef struct {
    VkShaderStageFlagBits stage;
    const char* path;
    const char* entry_point;
} VulkanShaderStageInfo;

typedef struct {
    u32 location;
    VkFormat format;
    u32 offset;
} VulkanVertexAttribute;

typedef struct {
    u32 binding;
    u32 stride;
    VkVertexInputRate input_rate;
    VulkanVertexAttribute* attributes;
    u32 attribute_count;
} VulkanVertexBinding;

typedef struct {
    // shaders
    VulkanShaderStageInfo* stages;
    u32 stage_count;

    // vertex input
    VulkanVertexBinding* vertex_bindings;
    uint32_t binding_count;

    // input assembly
    VkPrimitiveTopology topology;
    VkBool32 primitive_restart_enable;

    // rasterization
    VkPolygonMode polygon_mode;
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;
    f32 line_width;

    // multisampling
    VkSampleCountFlagBits samples;

    // depth/stencil
    VkBool32 depth_test_enable;
    VkBool32 depth_write_enable;
    VkCompareOp depth_compare_op;

    // color blend
    VkBool32 blend_enable;
    VkBlendFactor src_color_blend_factor;
    VkBlendFactor dst_color_blend_factor;
    VkBlendOp color_blend_op;
    VkBlendFactor src_alpha_blend_factor;
    VkBlendFactor dst_alpha_blend_factor;
    VkBlendOp alpha_blend_op;

    // dynamic states
    VkDynamicState* dynamic_states;
    u32 dynamic_state_count;

    // descriptor set layouts
    VkDescriptorSetLayout* descriptor_set_layouts;
    u32 descriptor_set_layout_count;

    // Push constant range
    VkPushConstantRange* push_constant_ranges;
    u32 push_constant_range_count;
} PipelineConfig;

typedef struct {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkShaderModule* shader_modules;
    u32 shader_module_count;
} VulkanPipeline;

bool vulkan_pipeline_create(VulkanContext* context,
                            const PipelineConfig* config,
                            VulkanPipeline* out_pipeline);
void vulkan_pipeline_destroy(VulkanContext* context, VulkanPipeline* pipeline);
void vulkan_pipeline_draw(VulkanContext* context, VulkanPipeline* pipeline,
                          VkCommandBuffer cmd_buffer);

// pipeline config
PipelineConfig pipeline_config_default();
void vulkan_pipeline_config_cleanup(PipelineConfig* config);

#endif  // GRAPHICS_PIPELINE_H
