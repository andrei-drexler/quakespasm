layout(binding=0) uniform sampler2D GammaTexture;
layout(binding=1) uniform usampler3D PaletteLUT;

#include "palette_buffer.glsl"
#include "noise_functions.glsl"

layout(location=0) uniform vec3 Params;

layout(location=0) out vec4 out_fragcolor;

void main()
{
	float gamma = Params.x;
	float contrast = Params.y;
	float scale = Params.z;
	out_fragcolor = texelFetch(GammaTexture, ivec2(gl_FragCoord), 0);
@if PALETTIZE == 1
	vec2 noiseuv = floor(gl_FragCoord.xy * scale) + 0.5;
	out_fragcolor.rgb = sqrt(out_fragcolor.rgb);
	out_fragcolor.rgb += DITHER_NOISE(noiseuv) * PAL_NOISESCALE;
	out_fragcolor.rgb *= out_fragcolor.rgb;
@endif // PALETTIZE == 1
@if PALETTIZE
	ivec3 clr = ivec3(clamp(out_fragcolor.rgb, 0., 1.) * 127. + 0.5);
	uint remap = Palette[texelFetch(PaletteLUT, clr, 0).x];
	out_fragcolor.rgb = vec3(UnpackRGB8(remap)) * (1./255.);
@else
	out_fragcolor.rgb *= contrast;
	out_fragcolor = vec4(pow(out_fragcolor.rgb, vec3(gamma)), 1.0);
@endif // PALETTIZE
}
