#include "shader_defs.glsl"
#include "light_cluster_image.glsl"

@if BINDLESS
	@extension GL_ARB_bindless_texture : require
@else
	layout(binding=0) uniform sampler2D Tex;
	layout(binding=1) uniform sampler2D FullbrightTex;
@endif
layout(binding=2) uniform sampler2D LMTex;

#include "framedata_buffer.glsl"
#include "light_buffer.glsl"
LIGHT_CLUSTER_IMAGE(readonly)
#include "world_calldata_buffer.glsl"
#include "world_instancedata_buffer.glsl"
#include "noise_functions.glsl"

layout(location=0) flat in uint in_flags;
layout(location=1) flat in float in_alpha;
layout(location=2) in vec3 in_pos;
@if MODE == WORLDSHADER_ALPHATEST
	layout(location=3) centroid in vec2 in_uv;
@else
	layout(location=3) in vec2 in_uv;
@endif
layout(location=4) centroid in vec2 in_lmuv;
layout(location=5) in float in_depth;
layout(location=6) noperspective in vec2 in_coord;
layout(location=7) flat in vec4 in_styles;
layout(location=8) flat in float in_lmofs;
@if BINDLESS
	layout(location=9) flat in uvec4 in_samplers;
@endif

layout(location=0) out vec4 out_fragcolor;

void main()
{
	vec3 fullbright = vec3(0.);
	vec2 uv = in_uv;
@if MODE == WORLDSHADER_WATER
	uv = uv * 2.0 + 0.125 * sin(uv.yx * (3.14159265 * 2.0) + Time);
@endif
@if BINDLESS
	sampler2D Tex = sampler2D(in_samplers.xy);
	sampler2D FullbrightTex;
	if ((in_flags & CF_USE_FULLBRIGHT) != 0u)
	{
		FullbrightTex = sampler2D(in_samplers.zw);
		fullbright = texture(FullbrightTex, uv).rgb;
	}
@else
	if ((in_flags & CF_USE_FULLBRIGHT) != 0u)
		fullbright = texture(FullbrightTex, uv).rgb;
@endif
@if DITHER >= 2
	vec4 result = texture(Tex, uv, -1.0);
@elif DITHER
	vec4 result = texture(Tex, uv, -0.5);
@else
	vec4 result = texture(Tex, uv);
@endif
@if MODE == WORLDSHADER_ALPHATEST
	if (result.a < 0.666)
		discard;
@endif

	vec2 lmuv = in_lmuv;
@if DITHER
	vec2 lmsize = vec2(textureSize(LMTex, 0).xy) * 16.;
	lmuv = (floor(lmuv * lmsize) + 0.5) / lmsize;
@endif // DITHER
	vec4 lm0 = textureLod(LMTex, lmuv, 0.);
	vec3 total_light;
	if (in_styles.y < 0.) // single style fast path
		total_light = in_styles.x * lm0.xyz;
	else
	{
		vec4 lm1 = textureLod(LMTex, vec2(lmuv.x + in_lmofs, lmuv.y), 0.);
		if (in_styles.z < 0.) // 2 styles
		{
			total_light =
				in_styles.x * lm0.xyz +
				in_styles.y * lm1.xyz;
		}
		else // 3 or 4 lightstyles
		{
			vec4 lm2 = textureLod(LMTex, vec2(lmuv.x + in_lmofs * 2., lmuv.y), 0.);
			total_light = vec3
			(
				dot(in_styles, lm0),
				dot(in_styles, lm1),
				dot(in_styles, lm2)
			);
		}
	}

	if (NumLights > 0u)
	{
		uint i, ofs;
		ivec3 cluster_coord;
		cluster_coord.x = int(floor(in_coord.x));
		cluster_coord.y = int(floor(in_coord.y));
		cluster_coord.z = int(floor(log2(in_depth) * ZLogScale + ZLogBias));
		uvec2 clusterdata = imageLoad(LightClusters, cluster_coord).xy;
		if ((clusterdata.x | clusterdata.y) != 0u)
		{
#if SHOW_ACTIVE_LIGHT_CLUSTERS
			int cluster_idx = cluster_coord.x + cluster_coord.y * LIGHT_TILES_X + cluster_coord.z * LIGHT_TILES_X * LIGHT_TILES_Y;
			total_light = vec3(ivec3((cluster_idx + 1) * 0x45d9f3b) >> ivec3(0, 8, 16) & 255) / 255.0;
#endif // SHOW_ACTIVE_LIGHT_CLUSTERS
			vec3 dynamic_light = vec3(0.);
			vec4 plane;
			plane.xyz = normalize(cross(dFdx(in_pos), dFdy(in_pos)));
			plane.w = dot(in_pos, plane.xyz);
			for (i = 0u, ofs = 0u; i < 2u; i++, ofs += 32u)
			{
				uint mask = clusterdata[i];
				while (mask != 0u)
				{
					int j = findLSB(mask);
					mask ^= 1u << j;
					Light l = Lights[ofs + j];
					// mimics R_AddDynamicLights, up to a point
					float rad = l.radius;
					float dist = dot(l.origin, plane.xyz) - plane.w;
					rad -= abs(dist);
					float minlight = l.minlight;
					if (rad < minlight)
						continue;
					vec3 local_pos = l.origin - plane.xyz * dist;
					minlight = rad - minlight;
					dist = length(in_pos - local_pos);
					dynamic_light += clamp((minlight - dist) / 16.0, 0.0, 1.0) * max(0., rad - dist) / 256. * l.color;
				}
			}
			total_light += max(min(dynamic_light, 1. - total_light), 0.);
		}
	}
@if DITHER >= 2
	total_light = floor(total_light * 64. + 0.5) * (1./32.);
@else
	total_light *= 2.0;
@endif
@if MODE != WORLDSHADER_ALPHATEST
	result.rgb = mix(result.rgb, result.rgb * total_light, result.a);
@else
	result.rgb *= total_light;
@endif
	result.rgb += fullbright;
	result = clamp(result, 0.0, 1.0);
	result.rgb = ApplyFog(result.rgb, in_depth);

	result.a = in_alpha; // FIXME: This will make almost transparent things cut holes though heavy fog
	out_fragcolor = result;
@if DITHER == 1
	vec3 dpos = fwidth(in_pos);
	float farblend = clamp(max(dpos.x, max(dpos.y, dpos.z)) * 0.5 - 0.125, 0., 1.);
	farblend *= farblend;
	out_fragcolor.rgb = sqrt(out_fragcolor.rgb);
	float luma = dot(out_fragcolor.rgb, vec3(.25, .625, .125));
	float nearnoise = tri(whitenoise01(lmuv * lmsize)) * luma;
	float farnoise = Fog.w > 0. ? SCREEN_SPACE_NOISE() : 0.;
	out_fragcolor.rgb += mix(nearnoise, farnoise, farblend) * PAL_NOISESCALE;
	out_fragcolor.rgb *= out_fragcolor.rgb;
@endif // DITHER == 1
};

