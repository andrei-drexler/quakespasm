struct Call
{
	uint	flags;
	float	wateralpha;
@if BINDLESS
	uvec2	txhandle;
	uvec2	fbhandle;
@else
	int		baseinstance;
	int		padding;
@endif // BINDLESS
};
const uint
	CF_USE_POLYGON_OFFSET = 1u,
	CF_USE_FULLBRIGHT = 2u,
	CF_NOLIGHTMAP = 4u
;

layout(std430, binding=1) restrict readonly buffer CallBuffer
{
	Call call_data[];
};

@if BINDLESS
	@define GET_INSTANCE_ID(call) (gl_BaseInstanceARB + gl_InstanceID)
@else
	@define GET_INSTANCE_ID(call) (call.baseinstance + gl_InstanceID)
@endif
