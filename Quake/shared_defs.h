#ifndef SHARED_DEFS_
#define SHARED_DEFS_

/*
 * Shared defines between C and GLSL code
*/

// client.h:
#define	MAX_DLIGHTS		64 //johnfitz -- was 32

// quakedef.h:
#define	MAX_LIGHTSTYLES	64

// glquake.h:
#define LIGHT_TILES_X			32
#define LIGHT_TILES_Y			16
#define LIGHT_TILES_Z			32
#define WORLDSHADER_SOLID		0
#define WORLDSHADER_ALPHATEST	1
#define WORLDSHADER_WATER		2
#define ALIASSHADER_STANDARD	0
#define ALIASSHADER_DITHER		1
#define ALIASSHADER_NOPERSP		2

#endif
