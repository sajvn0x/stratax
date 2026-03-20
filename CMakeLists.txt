cmake_minimum_required(VERSION 3.25)
project(stratax)

set(CMAKE_C_STANDARD 11)

find_package(glfw3 REQUIRED)
find_package(Threads REQUIRED)

set(VULKAN_SDK $ENV{VULKAN_SDK})
set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/include")
set(Vulkan_LIBRARY "${VULKAN_SDK}/lib/libvulkan.so")

find_package(Vulkan REQUIRED)

find_program(GLSLC glslc REQUIRED)

add_executable(stratax
    src/main.c

	# core
	src/core/memory.c
	src/core/logger.c
	src/core/fs.c

	# renderer
	src/vk/vk_device.c
	src/vk/vk_swapchain.c
	src/vk/pipeline/pipeline.c
)

target_include_directories(stratax PRIVATE
    src
    ${Vulkan_INCLUDE_DIR}
)

target_link_libraries(stratax PRIVATE
    glfw
    ${Vulkan_LIBRARY}
    cglm
    m
)

