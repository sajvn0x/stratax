#include "camera.h"

#include <cglm/vec3.h>
#include <math.h>

#include "core/memory.h"

void camera_init(Camera* cam) {
    // Position
    float pos[3] = {0.0f, 0.0f, 2.0f};
    glm_vec3_copy(pos, cam->position);
    // Front
    float front[3] = {0.0f, 0.0f, -1.0f};
    glm_vec3_copy(front, cam->front);
    // World up
    float world_up[3] = {0.0f, 1.0f, 0.0f};
    glm_vec3_copy(world_up, cam->world_up);
    // Up
    glm_vec3_copy(cam->world_up, cam->up);

    cam->yaw = -90.0f;
    cam->pitch = 0.0f;
    cam->movement_speed = 5.0f;
    cam->mouse_sensitivity = 0.05f;
    cam->first_mouse = true;
    memory_set(cam->keys, 0, sizeof(cam->keys));

    camera_update_vectors(cam);
}

void camera_update_vectors(Camera* cam) {
    vec3 front;
    front[0] = cos(glm_rad(cam->yaw)) * cos(glm_rad(cam->pitch));
    front[1] = sin(glm_rad(cam->pitch));
    front[2] = sin(glm_rad(cam->yaw)) * cos(glm_rad(cam->pitch));
    glm_vec3_normalize(front);
    glm_vec3_copy(front, cam->front);

    // right = front × world_up
    glm_vec3_cross(cam->front, cam->world_up, cam->right);
    glm_vec3_normalize(cam->right);

    // up = right × front
    glm_vec3_cross(cam->right, cam->front, cam->up);
    glm_vec3_normalize(cam->up);
}
