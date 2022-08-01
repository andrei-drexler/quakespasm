/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include "shaders/shaders.h"

glprogs_t glprogs;
static GLuint gl_programs[64];
static GLuint gl_current_program;
static int gl_num_programs;

/*
=============
AppendString
=============
*/
static qboolean AppendString (char **dst, const char *dstend, const char *str, int len)
{
	int avail = dstend - *dst;
	if (len < 0)
		len = Q_strlen (str);
	if (len + 1 > avail)
		return false;
	memcpy (*dst, str, len);
	(*dst)[len] = 0;
	*dst += len;
	return true;
}

/*
=============
GL_CreateShader
=============
*/
static GLuint GL_CreateShader (GLenum type, const char *source, const char *extradefs, const char *name)
{
	const char *strings[16];
	const char *typestr = NULL;
	char header[256];
	int numstrings = 0;
	GLint status;
	GLuint shader;

	switch (type)
	{
		case GL_VERTEX_SHADER:
			typestr = "vertex";
			break;
		case GL_FRAGMENT_SHADER:
			typestr = "fragment";
			break;
		case GL_COMPUTE_SHADER:
			typestr = "compute";
			break;
		default:
			Sys_Error ("GL_CreateShader: unknown type 0x%X for %s", type, name);
			break;
	}

	q_snprintf (header, sizeof (header),
		"#version 430\n"
		"\n"
		"#define BINDLESS %d\n"
		"#define REVERSED_Z %d\n",
		gl_bindless_able,
		gl_clipcontrol_able
	);
	strings[numstrings++] = header;

	if (extradefs && *extradefs)
		strings[numstrings++] = extradefs;
	strings[numstrings++] = source;

	shader = GL_CreateShaderFunc (type);
	GL_ObjectLabelFunc (GL_SHADER, shader, -1, name);
	GL_ShaderSourceFunc (shader, numstrings, strings, NULL);
	GL_CompileShaderFunc (shader);
	GL_GetShaderivFunc (shader, GL_COMPILE_STATUS, &status);

	if (status != GL_TRUE)
	{
		char infolog[1024];
		memset(infolog, 0, sizeof(infolog));
		GL_GetShaderInfoLogFunc (shader, sizeof(infolog), NULL, infolog);
		Sys_Error ("Error compiling %s %s shader :\n%s", name, typestr, infolog);
	}

	return shader;
}

/*
=============
GL_CreateProgramFromShaders
=============
*/
static GLuint GL_CreateProgramFromShaders (const GLuint *shaders, int numshaders, const char *name)
{
	GLuint program;
	GLint status;

	program = GL_CreateProgramFunc ();
	GL_ObjectLabelFunc (GL_PROGRAM, program, -1, name);

	while (numshaders-- > 0)
	{
		GL_AttachShaderFunc (program, *shaders);
		GL_DeleteShaderFunc (*shaders);
		++shaders;
	}

	GL_LinkProgramFunc (program);
	GL_GetProgramivFunc (program, GL_LINK_STATUS, &status);

	if (status != GL_TRUE)
	{
		char infolog[1024];
		memset(infolog, 0, sizeof(infolog));
		GL_GetProgramInfoLogFunc (program, sizeof(infolog), NULL, infolog);
		Sys_Error ("Error linking %s program: %s", name, infolog);
	}

	if (gl_num_programs == countof(gl_programs))
		Sys_Error ("gl_programs overflow");
	gl_programs[gl_num_programs] = program;
	gl_num_programs++;

	return program;
}

/*
====================
GL_CreateProgramFromSources
====================
*/
static GLuint GL_CreateProgramFromSources (int count, const GLchar **sources, const GLenum *types, const char *name, va_list argptr)
{
	char macros[1024];
	char eval[256];
	char *pipe;
	int i, realcount;
	GLuint shaders[2];

	if (count <= 0 || count > 2)
		Sys_Error ("GL_CreateProgramFromSources: invalid source count (%d)", count);

	q_vsnprintf (eval, sizeof (eval), name, argptr);
	macros[0] = 0;

	pipe = strchr (name, '|');
	if (pipe) // parse symbol list and generate #defines
	{
		char *dst = macros;
		char *dstend = macros + sizeof (macros);
		char *src = eval + 1 + (pipe - name);

		while (*src == ' ')
			src++;

		while (*src)
		{
			char *srcend = src + 1;
			while (*srcend && *srcend != ';')
				srcend++;

			if (!AppendString (&dst, dstend, "#define ", 8) ||
				!AppendString (&dst, dstend, src, srcend - src) ||
				!AppendString (&dst, dstend, "\n", 1))
				Sys_Error ("GL_CreateProgram: symbol overflow for %s", eval);

			src = srcend;
			while (*src == ';' || *src == ' ')
				src++;
		}

		AppendString (&dst, dstend, "\n", 1);
	}

	name = eval;

	realcount = 0;
	for (i = 0; i < count; i++)
		if (sources[i])
			shaders[realcount++] = GL_CreateShader (types[i], sources[i], macros, name);

	return GL_CreateProgramFromShaders (shaders, realcount, name);
}

/*
====================
GL_CreateProgram

Compiles and returns GLSL program.
====================
*/
static FUNC_PRINTF(3,4) GLuint GL_CreateProgram (const GLchar *vertSource, const GLchar *fragSource, const char *name, ...)
{
	const GLchar *sources[2] = {vertSource, fragSource};
	GLenum types[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
	va_list argptr;
	GLuint program;

	va_start (argptr, name);
	program = GL_CreateProgramFromSources (2, sources, types, name, argptr);
	va_end (argptr);

	return program;
}

/*
====================
GL_CreateComputeProgram

Compiles and returns GLSL program.
====================
*/
static FUNC_PRINTF(2,3) GLuint GL_CreateComputeProgram (const GLchar *source, const char *name, ...)
{
	GLenum type = GL_COMPUTE_SHADER;
	va_list argptr;
	GLuint program;

	va_start (argptr, name);
	program = GL_CreateProgramFromSources (1, &source, &type, name, argptr);
	va_end (argptr);

	return program;
}

/*
====================
GL_UseProgram
====================
*/
void GL_UseProgram (GLuint program)
{
	if (program == gl_current_program)
		return;
	gl_current_program = program;
	GL_UseProgramFunc (program);
}

/*
====================
GL_ClearCachedProgram

This must be called if you do anything that could make the cached program
invalid (e.g. manually binding, destroying the context).
====================
*/
void GL_ClearCachedProgram (void)
{
	gl_current_program = 0;
	GL_UseProgramFunc (0);
}

/*
=============
GL_CreateShaders
=============
*/
void GL_CreateShaders (void)
{
	int palettize, dither, mode, alphatest, warp;

	glprogs.gui = GL_CreateProgram (glsl_gui_vert, glsl_gui_frag, "gui");
	glprogs.viewblend = GL_CreateProgram (glsl_viewblend_vert, glsl_viewblend_frag, "viewblend");
	for (warp = 0; warp < 2; warp++)
		glprogs.warpscale[warp] = GL_CreateProgram (glsl_warpscale_vert, glsl_warpscale_frag, "view warp/scale|WARP %d", warp);
	for (palettize = 0; palettize < 3; palettize++)
		glprogs.postprocess[palettize] = GL_CreateProgram (glsl_postprocess_vert, glsl_postprocess_frag, "postprocess|PALETTIZE %d", palettize);

	for (dither = 0; dither < 3; dither++)
		for (mode = 0; mode < 3; mode++)
			glprogs.world[dither][mode] = GL_CreateProgram (glsl_world_vert, glsl_world_frag, "world|DITHER %d; MODE %d", dither, mode);

	for (dither = 0; dither < 2; dither++)
	{
		glprogs.water[dither] = GL_CreateProgram (glsl_water_vert, glsl_water_frag, "water|DITHER %d", dither);
		glprogs.skylayers[dither] = GL_CreateProgram (glsl_sky_layers_vert, glsl_sky_layers_frag, "sky layers|DITHER %d", dither);
		glprogs.skycubemap[dither] = GL_CreateProgram (glsl_sky_cubemap_vert, glsl_sky_cubemap_frag, "sky cubemap|DITHER %d", dither);
		glprogs.skyboxside[dither] = GL_CreateProgram (glsl_sky_boxside_vert, glsl_sky_boxside_frag, "skybox side|DITHER %d", dither);
		glprogs.sprites[dither] = GL_CreateProgram (glsl_sprites_vert, glsl_sprites_frag, "sprites|DITHER %d", dither);
		glprogs.particles[dither] = GL_CreateProgram (glsl_particles_vert, glsl_particles_frag, "particles|DITHER %d", dither);
	}
	glprogs.skystencil = GL_CreateProgram (glsl_skystencil_vert, NULL, "sky stencil");

	for (mode = 0; mode < 3; mode++)
		for (alphatest = 0; alphatest < 2; alphatest++)
			glprogs.alias[mode][alphatest] = GL_CreateProgram (glsl_alias_vert, glsl_alias_frag, "alias|MODE %d; ALPHATEST %d", mode, alphatest);

	glprogs.debug3d = GL_CreateProgram (glsl_debug3d_vert, glsl_debug3d_frag, "debug3d");

	glprogs.clear_indirect = GL_CreateComputeProgram (glsl_clear_indirect_compute, "clear indirect draw params");
	glprogs.gather_indirect = GL_CreateComputeProgram (glsl_gather_indirect_compute, "indirect draw gather");
	glprogs.cull_mark = GL_CreateComputeProgram (glsl_cull_mark_compute, "cull/mark");
	glprogs.cluster_lights = GL_CreateComputeProgram (glsl_cluster_lights_compute, "light cluster");
	glprogs.palette_init = GL_CreateComputeProgram (glsl_palette_init_compute, "palette init");
	glprogs.palette_postprocess = GL_CreateComputeProgram (glsl_palette_postprocess_compute, "palette postprocess");
}

/*
=============
GL_DeleteShaders
=============
*/
void GL_DeleteShaders (void)
{
	int i;
	for (i = 0; i < gl_num_programs; i++)
	{
		GL_DeleteProgramFunc (gl_programs[i]);
		gl_programs[i] = 0;
	}
	gl_num_programs = 0;

	GL_UseProgramFunc (0);
	gl_current_program = 0;

	memset (&glprogs, 0, sizeof(glprogs));
}
