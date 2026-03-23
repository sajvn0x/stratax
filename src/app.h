#ifndef APP_H
#define APP_H

#include "camera.h"
#include "core/defines.h"
#include "vk/vk_types.h"

static const char* APP_NAME = "StrataX";
static const char* APP_ENGINE = "StrataZ";
static const int WIDTH = 800;
static const int HEIGHT = 600;

typedef enum {
    APP_STATE_UNINITIALIZED,
    APP_STATE_INITIALIZING,
    APP_STATE_RUNNING,
    APP_STATE_PAUSED,
    APP_STATE_SHUTDOWN
} AppState;

typedef struct {
    AppState state;
    Camera camera;
    bool resize_occured;

    // timing
    f64 last_time;
    f64 delta_time;
    u32 frame_count;
} App;

static const Vertex triangle_vertices[] = {
    // position (x, y, z)   color (r, g, b)   texCoord (u, v)
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 1.0f}}};

static const u16 triangle_indices[] = {0, 1, 2};

#endif  // APP_H
