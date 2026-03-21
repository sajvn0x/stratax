#ifndef APP_H
#define APP_H

#include "core/defines.h"
#include "vk/vk_types.h"

static const char* APP_NAME = "StrataX";
static const char* APP_ENGINE = "StrataZ";
static const int WIDTH = 800;
static const int HEIGHT = 600;

static const Vertex triangle_vertices[] = {
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // bottom
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},   // right
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}   // left
};

static const u16 triangle_indices[] = {0, 1, 2};

#endif  // APP_H
