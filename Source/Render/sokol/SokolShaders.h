#ifndef PERIMETER_SOKOLSHADERS_H
#define PERIMETER_SOKOLSHADERS_H

#include "SokolTypes.h"

struct shader_funcs {
    SOKOL_SHADER_ID (*get_id)() = nullptr;
    const sg_shader_desc* (*shader_desc)(sg_backend) = nullptr;
    int (*attr_slot)(const char*) = nullptr;
    int (*image_slot)(sg_shader_stage, const char*) = nullptr;
    int (*sampler_slot)(sg_shader_stage, const char*) = nullptr;
    int (*uniformblock_slot)(sg_shader_stage, const char*) = nullptr;
    size_t (*uniformblock_size)(sg_shader_stage, const char*) = nullptr;
};

#include "sokol/shaders/color_tex1.h"
#include "sokol/shaders/color_tex2.h"
#include "sokol/shaders/normal.h"
#include "sokol/shaders/object_shadow.h"
#include "sokol/shaders/only_texture.h"
#include "sokol/shaders/tile_map.h"

#define SOKOL_SHADER(MODULE_NAME) \
extern shader_funcs shader_##MODULE_NAME;

#define SOKOL_SHADER_IMPL(MODULE_NAME) \
SOKOL_SHADER_ID MODULE_NAME##_get_shader_id() { \
    return SOKOL_SHADER_ID_##MODULE_NAME;    \
}; \
shader_funcs shader_##MODULE_NAME = { \
    MODULE_NAME##_get_shader_id, \
    MODULE_NAME##_program_shader_desc, \
    MODULE_NAME##_program_attr_slot, \
    MODULE_NAME##_program_image_slot, \
    MODULE_NAME##_program_sampler_slot, \
    MODULE_NAME##_program_uniformblock_slot, \
    MODULE_NAME##_program_uniformblock_size, \
};

SOKOL_SHADER(tex1);
SOKOL_SHADER(color_tex1);
SOKOL_SHADER(color_tex2);
SOKOL_SHADER(normal);
SOKOL_SHADER(tile_map);
SOKOL_SHADER(object_shadow);
SOKOL_SHADER(only_texture);

using normal_texture_vs_params_t = normal_normal_texture_vs_params_t;
using normal_texture_fs_params_t = normal_normal_texture_fs_params_t;
//color_tex1 and color_tex2 share the params struct, so we pick tex2
using color_texture_vs_params_t = color_tex2_color_texture_vs_params_t;
using color_texture_fs_params_t = color_tex2_color_texture_fs_params_t;
using tile_map_vs_params_t = tile_map_tile_map_vs_params_t;
using tile_map_fs_params_t = tile_map_tile_map_fs_params_t;
using object_shadow_vs_params_t = object_shadow_object_shadow_vs_params_t;
using only_texture_vs_params_t = only_texture_only_texture_vs_params_t;

#endif //PERIMETER_SOKOLSHADERS_H
