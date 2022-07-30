#include "framedata_buffer.glsl"
#include "noise_functions.glsl"

layout(binding=2) uniform samplerCube Skybox;

layout(location=0) in vec3 in_dir;

layout(location=0) out vec4 out_fragcolor;

void main()
{
	out_fragcolor = texture(Skybox, in_dir);
	out_fragcolor.rgb = mix(out_fragcolor.rgb, SkyFog.rgb, SkyFog.a);
@if DITHER
	out_fragcolor.rgb = sqrt(out_fragcolor.rgb);
	out_fragcolor.rgb += SCREEN_SPACE_NOISE() * PAL_NOISESCALE;
	out_fragcolor.rgb *= out_fragcolor.rgb;
@endif
}
