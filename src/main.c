#include <cglm/affine-pre.h>
#include <cglm/cam.h>

#include "core/memory.h"
#include "vk/vk_mesh.h"
#include "vulkan/vulkan_core.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/mat4.h>

#include "app.h"
#include "core/logger.h"
#include "vk/vk_device.h"
#include "vk/vk_pipeline.h"
#include "vk/vk_swapchain.h"
#include "vk/vk_types.h"

static VulkanContext vulkan_context = {0};
static GLFWwindow* window = 0;
static VkCommandBuffer* command_buffers = 0;
static bool is_pipeline_line = true;

static VulkanPipeline pipeline_fill = {0};
static VulkanPipeline pipeline_line = {0};

// synchrnonization objects
VkSemaphore image_available_semaphore = 0;
VkSemaphore* render_finished_semaphores = 0;
VkFence in_flight_fence = 0;

i32 find_memory_index(VulkanContext* context, u32 type_filter,
                      u32 property_flags);

bool vulkan_renderer_initialize() {
    vulkan_context.allocator = 0;
    vulkan_context.find_memory_index = find_memory_index;

    if (!vulkan_device_create(&vulkan_context, window)) {
        return false;
    }

    if (!vulkan_swapchain_initialize(&vulkan_context, window)) {
        return false;
    }

    return true;
}

void vulkan_renderer_destroy() {
    vulkan_swapchain_destroy(&vulkan_context);
    vulkan_device_destroy(&vulkan_context);
}

static bool command_buffers_create() {
    command_buffers =
        malloc(vulkan_context.swapchain.image_count * sizeof(VkCommandBuffer));
    VkCommandBufferAllocateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vulkan_context.device.graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = vulkan_context.swapchain.image_count};
    if (vkAllocateCommandBuffers(vulkan_context.device.device, &ci,
                                 command_buffers) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffers");
        return false;
    }

    return true;
}

static bool graphics_pipeline_create_with_mode(VkPolygonMode polygon_mode,
                                               VulkanPipeline* out_pipeline) {
    PipelineConfig config = pipeline_config_default();
    config.stages =
        memory_allocate(sizeof(VulkanShaderStageInfo) * 2, MEMORY_TAG_VULKAN);
    config.stages[0].path = "triangle.vert.spv";
    config.stages[0].entry_point = "main";
    config.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    config.stages[1].path = "triangle.frag.spv";
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
        memory_allocate(sizeof(VulkanVertexAttribute) * 2, MEMORY_TAG_VULKAN);
    config.vertex_bindings[0].attributes[0].location = 0;
    config.vertex_bindings[0].attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    config.vertex_bindings[0].attributes[0].offset = offsetof(Vertex, pos);
    config.vertex_bindings[0].attributes[1].location = 1;
    config.vertex_bindings[0].attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    config.vertex_bindings[0].attributes[1].offset = offsetof(Vertex, color);
    config.vertex_bindings[0].attribute_count = 2;

    // dynamic states
    config.dynamic_states =
        memory_allocate(sizeof(VkDynamicState) * 2, MEMORY_TAG_VULKAN);
    config.dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    config.dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    config.dynamic_state_count = 2;

    // rasterization state
    config.polygon_mode = polygon_mode;

    if (!vulkan_pipeline_create(&vulkan_context, &config, out_pipeline)) {
        LOG_ERROR("Failed to create graphics pipeline for polygon mode %d",
                  polygon_mode);
        vulkan_pipeline_config_cleanup(&config);
        return false;
    }

    vulkan_pipeline_config_cleanup(&config);
    return true;
}

static bool graphics_pipelines_create() {
    if (!graphics_pipeline_create_with_mode(VK_POLYGON_MODE_FILL,
                                            &pipeline_fill)) {
        return false;
    }
    if (!graphics_pipeline_create_with_mode(VK_POLYGON_MODE_LINE,
                                            &pipeline_line)) {
        return false;
    }
    return true;
}

void graphics_pipelines_destroy() {
    vulkan_pipeline_destroy(&vulkan_context, &pipeline_fill);
    vulkan_pipeline_destroy(&vulkan_context, &pipeline_line);
}

bool sync_objects(VulkanContext* context) {
    VkSemaphoreCreateInfo sem_ci = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_ci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                  .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    VkResult res =
        vkCreateSemaphore(context->device.device, &sem_ci, context->allocator,
                          &image_available_semaphore);

    if (res != VK_SUCCESS) {
        LOG_ERROR("failed to create image available semaphore: %d", res);
        return false;
    }

    for (u32 i = 0; i < context->swapchain.image_count; ++i) {
        res = vkCreateSemaphore(context->device.device, &sem_ci,
                                context->allocator,
                                &render_finished_semaphores[i]);

        if (res != VK_SUCCESS) {
            LOG_ERROR("failed to create render finished semaphore: %d", res);
            return false;
        }
    }

    res = vkCreateFence(context->device.device, &fence_ci, context->allocator,
                        &in_flight_fence);

    if (res != VK_SUCCESS) {
        LOG_ERROR("failed to create in flight fence: %d", res);
        return false;
    }

    return true;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action,
                         int mods) {
    if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        is_pipeline_line = !is_pipeline_line;
        LOG_INFO("Switched to %s mode", is_pipeline_line ? "line" : "fill");
    }
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WIDTH, HEIGHT, APP_NAME, NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, key_callback);

    if (!vulkan_renderer_initialize()) {
        LOG_ERROR("Failed to load the renderer");
        return -1;
    }

    if (!command_buffers_create()) {
        LOG_ERROR("Failed to create command buffers");
        return -1;
    }

    render_finished_semaphores = memory_allocate(
        sizeof(VkSemaphore) * vulkan_context.swapchain.image_count,
        MEMORY_TAG_VULKAN);

    if (!sync_objects(&vulkan_context)) {
        return -1;
    }

    // graphics pipeline
    if (!graphics_pipelines_create()) {
        LOG_ERROR("Failed to create graphics pipelines");
        return -1;
    }

    Mesh triangle_mesh;
    if (!mesh_create(&vulkan_context, triangle_vertices,
                     sizeof(triangle_vertices) / sizeof(Vertex),
                     triangle_indices, sizeof(triangle_indices) / sizeof(u16),
                     VK_INDEX_TYPE_UINT16, &triangle_mesh)) {
        LOG_ERROR("Failed to create mesh");
        return -1;
    }

    VkDevice device = vulkan_context.device.device;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &in_flight_fence);

        uint32_t image_index;
        vkAcquireNextImageKHR(device, vulkan_context.swapchain.handle,
                              UINT64_MAX, image_available_semaphore,
                              VK_NULL_HANDLE, &image_index);

        VkCommandBuffer cmd = command_buffers[image_index];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkClearValue clear_color = {.color = {0.f, 0.f, 0.022f, 1.0f}};
        VkRenderPassBeginInfo rp_begin = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin.renderPass = vulkan_context.device.render_pass;
        rp_begin.framebuffer =
            vulkan_context.swapchain.framebuffers[image_index];
        rp_begin.renderArea.offset = (VkOffset2D){0, 0};
        rp_begin.renderArea.extent = vulkan_context.swapchain.extent;
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues = &clear_color;

        vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

        // bind graphics pipeline
        if (is_pipeline_line) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline_line.handle);
        } else {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline_fill.handle);
        }

        VkViewport viewport = {0,
                               0,
                               (float)vulkan_context.swapchain.extent.width,
                               (float)vulkan_context.swapchain.extent.height,
                               0.0f,
                               1.0f};
        VkRect2D scissor = {{0, 0}, vulkan_context.swapchain.extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // draw triangle
        mesh_draw(cmd, &triangle_mesh);

        vkCmdDrawIndexed(
            cmd, sizeof(triangle_vertices) / sizeof(triangle_vertices[0]), 1, 0,
            0, 0);

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        // submit command buffer
        VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &image_available_semaphore,
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &render_finished_semaphores[image_index],

        vkQueueSubmit(vulkan_context.device.graphics_queue, 1, &submitInfo,
                      in_flight_fence);

        // present
        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &render_finished_semaphores[image_index],
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &vulkan_context.swapchain.handle;
        presentInfo.pImageIndices = &image_index;

        vkQueuePresentKHR(vulkan_context.device.present_queue, &presentInfo);
    }

    vkDeviceWaitIdle(device);

    mesh_destroy(&vulkan_context, &triangle_mesh);

    graphics_pipelines_destroy();

    if (image_available_semaphore)
        vkDestroySemaphore(vulkan_context.device.device,
                           image_available_semaphore, vulkan_context.allocator);

    for (u32 i = 0; i < vulkan_context.swapchain.image_count; ++i) {
        if (render_finished_semaphores[i])
            vkDestroySemaphore(vulkan_context.device.device,
                               render_finished_semaphores[i],
                               vulkan_context.allocator);
    }
    memory_free(render_finished_semaphores,
                sizeof(VkSemaphore) * vulkan_context.swapchain.image_count,
                MEMORY_TAG_VULKAN);

    if (in_flight_fence)
        vkDestroyFence(vulkan_context.device.device, in_flight_fence,
                       vulkan_context.allocator);

    vulkan_renderer_destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

i32 find_memory_index(VulkanContext* context, u32 type_filter,
                      u32 property_flags) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(context->device.gpu, &mem_props);

    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (type_filter & (1 << i) && (mem_props.memoryTypes[i].propertyFlags &
                                       property_flags) == property_flags) {
            return i;
        }
    }

    LOG_WARN("Unable to find suitable memory type");
    return -1;
}
