#ifndef MODEL_H
#define MODEL_H

#include "vk/vk_mesh.h"
#include "vk/vk_types.h"

bool model_load(VulkanContext* context, const char* filepath, Mesh** out_mesh,
                u32* out_mesh_count);

#endif  // MODEL_H
