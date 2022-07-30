@if BINDLESS
	@extension GL_ARB_shader_draw_parameters : require
	@define DRAW_ID			gl_DrawIDARB
@else
	layout(location=0) uniform int DrawID;
	@define DRAW_ID			DrawID
@endif

