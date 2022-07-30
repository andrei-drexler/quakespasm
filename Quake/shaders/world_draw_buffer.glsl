#include "draw_elements_indirect_command.glsl"

layout(std430, binding=1) buffer DrawIndirectBuffer
{
	DrawElementsIndirectCommand cmds[];
};
