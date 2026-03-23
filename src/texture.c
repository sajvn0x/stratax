#include "core/defines.h"
#include "core/logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "vk/vk_buffer.h"
#include "vk/vk_image.h"
#include "vk/vk_types.h"

static VkCommandBuffer begin_single_time_commands(VulkanContext* context) {
    VkCommandBufferAllocateInfo command_buffer_ai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context->device.graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(context->device.device, &command_buffer_ai, &cmd);

    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

static void end_single_time_commands(VulkanContext* ctx, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &cmd};
    vkQueueSubmit(ctx->device.graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->device.graphics_queue);
    vkFreeCommandBuffers(ctx->device.device, ctx->device.graphics_command_pool,
                         1, &cmd);
}

static void transition_image_layout(VkCommandBuffer cmd, VkImage image,
                                    VkImageLayout old_layout,
                                    VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    VkPipelineStageFlags src_stage, dst_stage;
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        return;
    }
    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1,
                         &barrier);
}

bool load_texture(VulkanContext* ctx, const char* path, VulkanImage* out_tex) {
    int width, height, channels;
    stbi_uc* pixels =
        stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR("Failed to load texture %s", path);
        return false;
    }

    VkDeviceSize image_size = width * height * 4;
    VkExtent3D extent = {(uint32_t)width, (uint32_t)height, 1};
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

    // Create Vulkan image (device local)
    if (!vulkan_image_create(
            ctx, VK_IMAGE_TYPE_2D, format, extent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, out_tex)) {
        stbi_image_free(pixels);
        return false;
    }

    // Create staging buffer
    VulkanBuffer staging;
    VulkanBufferCreateInfo staging_info = {
        .size = image_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        .persistent_mapped = VK_TRUE,
        .initial_data = pixels};
    if (!vulkan_buffer_create(ctx, &staging_info, &staging)) {
        vulkan_image_destroy(ctx, out_tex);
        stbi_image_free(pixels);
        return false;
    }

    // Copy data using a temporary command buffer
    VkCommandBuffer cmd = begin_single_time_commands(ctx);

    // Transition image to transfer destination layout
    transition_image_layout(cmd, out_tex->handle, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy buffer to image
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = extent};
    vkCmdCopyBufferToImage(cmd, staging.handle, out_tex->handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read only layout
    transition_image_layout(cmd, out_tex->handle,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    end_single_time_commands(ctx, cmd);

    // Clean up staging
    vulkan_buffer_destroy(ctx, &staging);
    stbi_image_free(pixels);

    // Create image view
    if (!vulkan_image_view_create(ctx, out_tex, VK_IMAGE_VIEW_TYPE_2D,
                                  VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
                                  &out_tex->view)) {
        vulkan_image_destroy(ctx, out_tex);
        return false;
    }

    return true;
}
