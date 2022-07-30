#include "alias_instance_buffer.glsl"
#include "noise_functions.glsl"

layout(binding=0) uniform sampler2D Tex;
layout(binding=1) uniform sampler2D FullbrightTex;

@if MODE == ALIASSHADER_NOPERSP
	layout(location=0) noperspective in vec2 in_texcoord;
@else
	layout(location=0) in vec2 in_texcoord;
@endif
layout(location=1) in vec4 in_color;
layout(location=2) in float in_fogdist;

layout(location=0) out vec4 out_fragcolor;

void main()
{
	vec2 uv = in_texcoord;
@if MODE == ALIASSHADER_NOPERSP
	uv -= 0.5 / vec2(textureSize(Tex, 0).xy);
	vec4 result = textureLod(Tex, uv, 0.);
@else
	vec4 result = texture(Tex, uv);
@endif
@if ALPHATEST
	if (result.a < 0.666)
		discard;
	result.rgb *= in_color.rgb;
@else
	result.rgb = mix(result.rgb, result.rgb * in_color.rgb, result.a);
@endif
	result.a = in_color.a; // FIXME: This will make almost transparent things cut holes though heavy fog
@if MODE == ALIASSHADER_NOPERSP
	result.rgb += textureLod(FullbrightTex, uv, 0.).rgb;
@else
	result.rgb += texture(FullbrightTex, uv).rgb;
@endif
	result.rgb = clamp(result.rgb, 0.0, 1.0);
	float fog = exp2(-(Fog.w * in_fogdist) * (Fog.w * in_fogdist));
	fog = clamp(fog, 0.0, 1.0);
	result.rgb = mix(Fog.rgb, result.rgb, fog);
	out_fragcolor = result;
@if MODE == ALIASSHADER_DITHER
	// Note: sign bit is used as overbright flag
	if (abs(Fog.w) > 0.)
	{
		out_fragcolor.rgb = sqrt(out_fragcolor.rgb);
		out_fragcolor.rgb += SCREEN_SPACE_NOISE() * PAL_NOISESCALE;
		out_fragcolor.rgb *= out_fragcolor.rgb;
	}
@endif
}
