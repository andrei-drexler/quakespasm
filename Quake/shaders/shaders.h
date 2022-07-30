#ifndef SHADERS_H_
#define SHADERS_H_

#include "../quakedef.h"

/*
 * ideally we'd just cherry pick the following defines:

#define LIGHT_TILES_X
#define LIGHT_TILES_Y 
#define LIGHT_TILES_Z
#define MAX_LIGHTS
#define MAX_LIGHTSTYLES
#define WORLDSHADER_ALPHATEST
#define WORLDSHADER_WATER
#define ALIASSHADER_NOPERSP
*/

// safety first!

#define SHOW_ACTIVE_LIGHT_CLUSTERS 0

extern char glsl_gui_vert[];
extern char glsl_gui_frag[];

extern char glsl_viewblend_vert[];
extern char glsl_viewblend_frag[];

extern char glsl_postprocess_vert[];
extern char glsl_postprocess_frag[];

extern char glsl_warpscale_vert[];
extern char glsl_warpscale_frag[];

extern char glsl_world_vert[];
extern char glsl_world_frag[];

extern char glsl_water_vert[];
extern char glsl_water_frag[];

extern char glsl_skystencil_vert[];

extern char glsl_sky_layers_vert[];
extern char glsl_sky_layers_frag[];

extern char glsl_sky_cubemap_vert[];
extern char glsl_sky_cubemap_frag[];

extern char glsl_sky_boxside_vert[];
extern char glsl_sky_boxside_frag[];

extern char glsl_alias_vert[];
extern char glsl_alias_frag[];

extern char glsl_sprites_vert[];
extern char glsl_sprites_frag[];

extern char glsl_particles_vert[];
extern char glsl_particles_frag[];

extern char glsl_gui_vert[];
extern char glsl_gui_frag[];

extern char glsl_debug3d_vert[];
extern char glsl_debug3d_frag[];

extern char glsl_clear_indirect_compute[];
extern char glsl_cluster_lights_compute[];
extern char glsl_cull_mark_compute[];
extern char glsl_gather_indirect_compute[];
extern char glsl_palette_init_compute[];
extern char glsl_palette_postprocess_compute[];

#endif
