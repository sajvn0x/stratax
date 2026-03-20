#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "app.h"
#include "core/logger.h"
#include "vk/vk_device.h"
#include "vk/vk_swapchain.h"
#include "vk/vk_types.h"

static VulkanContext vulkan_context = {0};
static GLFWwindow* window = 0;

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

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WIDTH, HEIGHT, APP_NAME, NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    if (!vulkan_renderer_initialize()) {
        LOG_ERROR("Failed to load the renderer");
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

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
