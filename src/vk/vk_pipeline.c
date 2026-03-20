#include "vk_pipeline.h"

#include "core/fs.h"
#include "core/logger.h"
#include "core/memory.h"

bool shader_module_create(VulkanContext* context, const char* spv_file,
                          VkShaderModule* out_shader_module);

bool vulkan_pipeline_create(VulkanContext* context,
                            const PipelineConfig* config,
                            VulkanPipeline* out_pipeline) {
    VkDevice device = context->device.device;
    memory_set(out_pipeline, 0, sizeof(VulkanPipeline));

    // shader stages
    VkPipelineShaderStageCreateInfo* stage_infos = 0;
    out_pipeline->shader_modules = memory_allocate(
        sizeof(VkShaderModule) * config->stage_count, MEMORY_TAG_VULKAN);
    out_pipeline->shader_module_count = config->stage_count;

    stage_infos = memory_allocate(
        sizeof(VkPipelineShaderStageCreateInfo) * config->stage_count,
        MEMORY_TAG_VULKAN);

    for (u32 i = 0; i < config->stage_count; ++i) {
        const VulkanShaderStageInfo* stage_info = &config->stages[i];
        if (!shader_module_create(context, stage_info->path,
                                  &out_pipeline->shader_modules[i])) {
            LOG_ERROR("Failed to create shader module");
            return false;
        }

        stage_infos[i] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage_info->stage,
            .module = out_pipeline->shader_modules[i],
            .pName =
                stage_info->entry_point ? stage_info->entry_point : "main"};
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkVertexInputBindingDescription* binding_descs = NULL;
    VkVertexInputAttributeDescription* attr_descs = NULL;

    // vertex input
    if (config->binding_count > 0) {
        u32 total_attrs = 0;
        for (u32 i = 0; i < config->binding_count; ++i) {
            total_attrs += config->vertex_bindings[i].attribute_count;
        }

        // allocate descriptors
        binding_descs = memory_allocate(
            sizeof(VkVertexInputBindingDescription) * config->binding_count,
            MEMORY_TAG_VULKAN);
        attr_descs = memory_allocate(
            sizeof(VkVertexInputAttributeDescription) * total_attrs,
            MEMORY_TAG_VULKAN);

        u32 attr_index = 0;
        for (u32 i = 0; i < config->binding_count; ++i) {
            const VulkanVertexBinding* binding = &config->vertex_bindings[i];

            binding_descs[i] = (VkVertexInputBindingDescription){
                .binding = binding->binding,
                .stride = binding->stride,
                .inputRate = binding->input_rate};

            for (uint32_t j = 0; j < binding->attribute_count; j++) {
                attr_descs[attr_index++] = (VkVertexInputAttributeDescription){
                    .location = binding->attributes[j].location,
                    .binding = binding->binding,
                    .format = binding->attributes[j].format,
                    .offset = binding->attributes[j].offset};
            }
        }

        vertex_input_info.vertexBindingDescriptionCount = config->binding_count;
        vertex_input_info.pVertexBindingDescriptions = binding_descs;
        vertex_input_info.vertexAttributeDescriptionCount = total_attrs;
        vertex_input_info.pVertexAttributeDescriptions = attr_descs;
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    // input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .primitiveRestartEnable = config->primitive_restart_enable,
        .topology = config->topology};

    // viewport state
    VkPipelineViewportStateCreateInfo viewport_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        // TODO:
        // .viewportCount = 1,
        // .scissorCount = 1
    };

    // rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = config->polygon_mode,
        .cullMode = config->cull_mode,
        .frontFace = config->front_face,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = config->line_width};

    // multisampling
    VkPipelineMultisampleStateCreateInfo multisample_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = config->samples,
        .sampleShadingEnable = VK_FALSE};

    // depth/stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = config->depth_test_enable,
        .depthWriteEnable = config->depth_write_enable,
        .depthCompareOp = config->depth_compare_op,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE};

    // color blending
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = config->blend_enable,
        .srcColorBlendFactor = config->src_color_blend_factor,
        .dstColorBlendFactor = config->dst_color_blend_factor,
        .colorBlendOp = config->color_blend_op,
        .srcAlphaBlendFactor = config->src_alpha_blend_factor,
        .dstAlphaBlendFactor = config->dst_alpha_blend_factor,
        .alphaBlendOp = config->alpha_blend_op,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    VkPipelineColorBlendStateCreateInfo color_blending_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment};

    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = config->dynamic_state_count,
        .pDynamicStates = config->dynamic_states};

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = config->descriptor_set_layout_count,
        .pSetLayouts = config->descriptor_set_layouts,
        .pushConstantRangeCount = config->push_constant_range_count,
        .pPushConstantRanges = config->push_constant_ranges};

    VkResult result = vkCreatePipelineLayout(
        device, &layout_info, context->allocator, &out_pipeline->layout);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create pipeline layout");
        return false;
    }

    // graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = config->stage_count,
        .pStages = stage_infos,
        .pVertexInputState = &vertex_input_ci,
        .pInputAssemblyState = &input_assembly_ci,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterizer_ci,
        .pMultisampleState = &multisample_ci,
        .pDepthStencilState = &depth_stencil_ci,
        .pColorBlendState = &color_blending_ci,
        .pDynamicState = &dynamic_state_ci,
        .layout = out_pipeline->layout,
        .renderPass = context->device.render_pass,
        .subpass = 0};

    result =
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
                                  context->allocator, &out_pipeline->handle);

    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create graphics pipeline");
        return false;
    }

    return true;
}

void vulkan_pipeline_destroy(VulkanContext* context, VulkanPipeline* pipeline) {
    VkDevice device = context->device.device;

    if (pipeline->handle) {
        vkDestroyPipeline(device, pipeline->handle, context->allocator);
        pipeline->handle = VK_NULL_HANDLE;
    }

    if (pipeline->layout) {
        vkDestroyPipelineLayout(device, pipeline->layout, context->allocator);
        pipeline->layout = VK_NULL_HANDLE;
    }

    if (pipeline->shader_modules) {
        for (uint32_t i = 0; i < pipeline->shader_module_count; i++) {
            if (pipeline->shader_modules[i])
                vkDestroyShaderModule(device, pipeline->shader_modules[i],
                                      context->allocator);
        }
        memory_free(pipeline->shader_modules,
                    sizeof(VkShaderModule) * pipeline->shader_module_count,
                    MEMORY_TAG_VULKAN);
        pipeline->shader_modules = 0;
    }
}

// ===== Pipeline Config =====
PipelineConfig pipeline_config_default() {
    PipelineConfig config = {0};

    config.stages = 0;
    config.stage_count = 0;

    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.primitive_restart_enable = VK_FALSE;

    config.polygon_mode = VK_POLYGON_MODE_FILL;
    config.cull_mode = VK_CULL_MODE_BACK_BIT;
    config.front_face = VK_FRONT_FACE_CLOCKWISE;
    config.line_width = 1.0f;

    config.samples = VK_SAMPLE_COUNT_1_BIT;

    config.depth_test_enable = VK_FALSE;
    config.depth_write_enable = VK_FALSE;
    config.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

    config.blend_enable = VK_FALSE;
    config.src_color_blend_factor = VK_BLEND_FACTOR_ONE;
    config.dst_color_blend_factor = VK_BLEND_FACTOR_ZERO;
    config.color_blend_op = VK_BLEND_OP_ADD;
    config.src_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
    config.dst_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
    config.alpha_blend_op = VK_BLEND_OP_ADD;

    config.dynamic_states = 0;
    config.dynamic_state_count = 0;

    config.descriptor_set_layouts = 0;
    config.descriptor_set_layout_count = 0;

    config.push_constant_ranges = 0;
    config.push_constant_range_count = 0;

    return config;
}

void vulkan_pipeline_config_cleanup(PipelineConfig* config) {
    if (config->stages)
        memory_free(
            config->stages,
            sizeof(VkPipelineShaderStageCreateInfo) * config->stage_count,
            MEMORY_TAG_VULKAN);
    if (config->vertex_bindings) {
        for (u32 i = 0; i < config->binding_count; ++i) {
            if (config->vertex_bindings[i].attributes)
                memory_free(config->vertex_bindings[i].attributes,
                            sizeof(VulkanVertexAttribute) *
                                config->vertex_bindings[i].attribute_count,
                            MEMORY_TAG_VULKAN);
        }
        memory_free(config->vertex_bindings,
                    sizeof(VulkanVertexBinding) * config->binding_count,
                    MEMORY_TAG_VULKAN);
    }
    if (config->dynamic_states)
        memory_free(config->dynamic_states,
                    sizeof(VkDynamicState) * config->dynamic_state_count,
                    MEMORY_TAG_VULKAN);
    if (config->descriptor_set_layouts)
        memory_free(
            config->descriptor_set_layouts,
            sizeof(VkDescriptorSetLayout) * config->descriptor_set_layout_count,
            MEMORY_TAG_VULKAN);
    if (config->push_constant_ranges)
        memory_free(
            config->descriptor_set_layouts,
            sizeof(VkPushConstantRange) * config->push_constant_range_count,
            MEMORY_TAG_VULKAN);
}

bool shader_module_create(VulkanContext* context, const char* spv_file,
                          VkShaderModule* out_shader_module) {
    u64 size = 0;
    char* data;
    if (!read_file(spv_file, &size, &data) || size == 0) {
        LOG_ERROR("Failed to load the file");
        return false;
    }

    VkShaderModuleCreateInfo shader_module_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (uint32_t*)data};

    VkResult result =
        vkCreateShaderModule(context->device.device, &shader_module_ci,
                             context->allocator, out_shader_module);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create shader module: %u", result);
        return false;
    }

    return true;
}
