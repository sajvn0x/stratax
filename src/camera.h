#ifndef CAMERA_H
#define CAMERA_H

#include <GLFW/glfw3.h>
#include <cglm/types.h>

#include "core/defines.h"

typedef struct {
    vec3 position;
    vec3 front;
    vec3 up;
    vec3 right;
    vec3 world_up;

    f32 yaw;
    f32 pitch;

    float movement_speed;
    float mouse_sensitivity;
    float last_x;
    float last_y;
    bool first_mouse;

    bool keys[GLFW_KEY_LAST];
} Camera;

void camera_init(Camera* cam);
void camera_update_vectors(Camera* cam);

#endif  //  CAMERA_H
