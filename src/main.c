#include <cglm/cam.h>

#include "camera.h"
#include "core/memory.h"
#include "vk/vk_buffer.h"
#include "vk/vk_mesh.h"
#include "vulkan/vulkan_core.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/mat4.h>

#include "app.h"
#include "core/logger.h"
#include "material.h"
#include "vk/vk_device.h"
#include "vk/vk_image.h"
#include "vk/vk_swapchain.h"
#include "vk/vk_types.h"

static App app;

static VulkanContext vulkan_context = {0};
static GLFWwindow* window = 0;
static VkCommandBuffer* command_buffers = 0;

// synchrnonization objects
VkSemaphore image_available_semaphore = 0;
VkSemaphore* render_finished_semaphores = 0;
VkFence in_flight_fence = 0;

// resources descriptors
static VkDescriptorSetLayout global_ubo_descriptor_layout = 0;
static VkDescriptorSet global_ubo_descriptor_set = 0;

i32 find_memory_index(VulkanContext* context, u32 type_filter,
                      u32 property_flags);
bool load_texture(VulkanContext* ctx, const char* path, VulkanImage* out_tex);

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

bool resource_descriptor_create(VkBuffer ub) {
    // UBO descriptor set layout
    VkDescriptorSetLayoutBinding ubo_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};
    VkDescriptorSetLayoutCreateInfo ubo_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &ubo_binding};
    VkResult result = vkCreateDescriptorSetLayout(
        vulkan_context.device.device, &ubo_layout_info,
        vulkan_context.allocator, &global_ubo_descriptor_layout);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create UBO descriptor set layout: %d", result);
        return false;
    }

    VkDescriptorSetAllocateInfo ubo_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vulkan_context.device.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &global_ubo_descriptor_layout};
    result = vkAllocateDescriptorSets(vulkan_context.device.device, &ubo_alloc,
                                      &global_ubo_descriptor_set);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate UBO descriptor set: %d", result);
        return false;
    }

    VkDescriptorBufferInfo buffer_info = {
        .buffer = ub, .offset = 0, .range = sizeof(UBO)};
    VkWriteDescriptorSet ubo_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = global_ubo_descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &buffer_info};
    vkUpdateDescriptorSets(vulkan_context.device.device, 1, &ubo_write, 0,
                           NULL);

    return true;
}

void resource_descriptor_destroy() {
    vkDestroyDescriptorSetLayout(vulkan_context.device.device,
                                 global_ubo_descriptor_layout, NULL);
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
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        app.state = APP_STATE_SHUTDOWN;
    }

    if (key == GLFW_KEY_W && action == GLFW_PRESS)
        app.camera.keys[GLFW_KEY_W] = true;
    if (key == GLFW_KEY_W && action == GLFW_RELEASE)
        app.camera.keys[GLFW_KEY_W] = false;
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static Camera* cam = &app.camera;

    if (cam->first_mouse) {
        cam->last_x = xpos;
        cam->last_y = ypos;
        cam->first_mouse = false;
    }

    float xoffset = xpos - cam->last_x;
    float yoffset = cam->last_y - ypos;  // reversed: y ranges bottom to top
    cam->last_x = xpos;
    cam->last_y = ypos;

    xoffset *= cam->mouse_sensitivity;
    yoffset *= cam->mouse_sensitivity;

    cam->yaw += xoffset;
    cam->pitch += yoffset;

    // constrain pitch to avoid gimbal lock
    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    camera_update_vectors(cam);
}

int main() {
    app.state = APP_STATE_INITIALIZING;

    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WIDTH, HEIGHT, APP_NAME, NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    camera_init(&app.camera);
    glfwSetCursorPosCallback(window, mouse_callback);
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

    // uniform buffers
    VulkanBuffer uniform_buffer;

    VulkanBufferCreateInfo ub_info = {
        .size = sizeof(UBO),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        .persistent_mapped = VK_TRUE};

    if (!vulkan_buffer_create(&vulkan_context, &ub_info, &uniform_buffer)) {
        LOG_ERROR("Failed to create uniform buffer");
        return -1;
    }

    if (!resource_descriptor_create(uniform_buffer.handle)) {
        LOG_ERROR("Failed to create global UBO resource descriptor");
        return -1;
    }

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f};

    VkSampler texture_sampler;
    if (vkCreateSampler(vulkan_context.device.device, &sampler_info,
                        vulkan_context.allocator, &texture_sampler)) {
        {
            LOG_ERROR("Failed to create the image sample");
            return -1;
        }
    }

    VulkanImage texture_image;
    if (!load_texture(&vulkan_context, "../assets/Glass_125L.jpg",
                      &texture_image)) {
        LOG_ERROR("Failed to load the texture image");
        return -1;
    }

    Material material;
    material_create(&vulkan_context, "shader.vert.spv", "shader.frag.spv",
                    &texture_image, texture_sampler,
                    global_ubo_descriptor_layout, &material);

    Mesh triangle_mesh;
    if (!mesh_create(&vulkan_context, triangle_vertices,
                     sizeof(triangle_vertices) / sizeof(Vertex),
                     triangle_indices, sizeof(triangle_indices) / sizeof(u16),
                     VK_INDEX_TYPE_UINT16, &triangle_mesh)) {
        LOG_ERROR("Failed to create mesh");
        return -1;
    }

    VkDevice device = vulkan_context.device.device;
    app.state = APP_STATE_RUNNING;
    while (!glfwWindowShouldClose(window)) {
        if (app.state != APP_STATE_RUNNING) break;

        glfwPollEvents();

        vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &in_flight_fence);

        uint32_t image_index;
        vkAcquireNextImageKHR(device, vulkan_context.swapchain.handle,
                              UINT64_MAX, image_available_semaphore,
                              VK_NULL_HANDLE, &image_index);

        float current_time = glfwGetTime();
        float deltaTime = current_time - app.last_time;
        app.last_time = current_time;

        float speed = app.camera.movement_speed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            glm_vec3_muladds(app.camera.front, speed, app.camera.position);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            glm_vec3_muladds(app.camera.front, -speed, app.camera.position);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            glm_vec3_muladds(app.camera.right, -speed, app.camera.position);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            glm_vec3_muladds(app.camera.right, speed, app.camera.position);
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            glm_vec3_muladds(app.camera.up, speed, app.camera.position);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            glm_vec3_muladds(app.camera.up, -speed, app.camera.position);

        mat4 view;
        vec3 center;
        glm_vec3_add(app.camera.position, app.camera.front, center);
        glm_lookat(app.camera.position, center, app.camera.up, view);

        UBO ubo;
        glm_mat4_copy(view, ubo.view);
        glm_ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 10.0f, ubo.proj);

        glm_perspective(glm_rad(45.0f),
                        (float)vulkan_context.swapchain.extent.width /
                            (float)vulkan_context.swapchain.extent.height,
                        0.1f, 10.0f, ubo.proj);

        memory_copy(uniform_buffer.mapped, &ubo, sizeof(ubo));

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
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          material.pipeline.handle);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                material.pipeline.layout, 0, 1,
                                &global_ubo_descriptor_set, 0, NULL);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                material.pipeline.layout, 1, 1,
                                &material.texture_set, 0, NULL);

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

    // uniform buffer
    vulkan_buffer_destroy(&vulkan_context, &uniform_buffer);

    vulkan_image_destroy(&vulkan_context, &texture_image);
    vkDestroySampler(device, texture_sampler, vulkan_context.allocator);

    resource_descriptor_destroy();
    material_destroy(&vulkan_context, &material);

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
