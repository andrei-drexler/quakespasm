#include "shader_defs.glsl"
@if BINDLESS
	@extension GL_ARB_bindless_texture : require
@else
	layout(binding=0) uniform sampler2D SolidLayer;
	layout(binding=1) uniform sampler2D AlphaLayer;
@endif

#include "framedata_buffer.glsl"

layout(location=0) in vec3 in_dir;
@if BINDLESS
	layout(location=1) flat in uvec4 in_samplers;
@endif

layout(location=0) out vec4 out_fragcolor;

void main()
{
@if BINDLESS
	sampler2D SolidLayer = sampler2D(in_samplers.xy);
	sampler2D AlphaLayer = sampler2D(in_samplers.zw);
@endif
	vec2 uv = normalize(in_dir).xy * (189.0 / 64.0);
	vec4 result = texture(SolidLayer, uv + Time / 16.0);
	vec4 layer = texture(AlphaLayer, uv + Time / 8.0);
	result.rgb = mix(result.rgb, layer.rgb, layer.a);
	result.rgb = mix(result.rgb, SkyFog.rgb, SkyFog.a);
	out_fragcolor = result;
}
