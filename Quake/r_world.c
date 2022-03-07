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
// r_world.c: world model rendering

#include "quakedef.h"

extern cvar_t gl_fullbrights, r_oldskyleaf, r_showtris; //johnfitz
extern cvar_t gl_zfix; // QuakeSpasm z-fighting fix

extern gltexture_t *lightmap_texture;
extern gltexture_t *skybox_cubemap;

extern GLuint gl_bmodel_vbo;
extern size_t gl_bmodel_vbo_size;
extern GLuint gl_bmodel_ibo;
extern size_t gl_bmodel_ibo_size;
extern GLuint gl_bmodel_indirect_buffer;
extern size_t gl_bmodel_indirect_buffer_size;
extern GLuint gl_bmodel_leaf_buffer;
extern GLuint gl_bmodel_surf_buffer;
extern GLuint gl_bmodel_marksurf_buffer;

static GLuint gl_bmodel_cmdbuf;
static size_t gl_bmodel_cmdbuf_size;
static size_t gl_bmodel_cmdbuf_offset;

typedef struct gpumark_frame_s {
	vec4_t		frustum[4];
	vec3_t		vieworg;
	GLuint		oldskyleaf;
	GLuint		framecount;
	GLuint		padding[3];
} gpumark_frame_t;

byte *SV_FatPVS (vec3_t org, qmodel_t *worldmodel);

#ifdef USE_SSE2
/*
===============
R_CullBoxSIMD

Performs frustum culling for 8 bounding boxes
===============
*/
int R_CullBoxSIMD (soa_aabb_t box, int activelanes)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		mplane_t *p;
		byte signbits;
		int ofs;

		if (activelanes == 0)
			break;

		p = frustum + i;
		signbits = p->signbits;

		__m128 vplane = _mm_loadu_ps(p->normal);

		ofs = signbits & 1 ? 0 : 8; // x min/max
		__m128 px = _mm_shuffle_ps(vplane, vplane, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 v0 = _mm_mul_ps(_mm_loadu_ps(box + ofs), px);
		__m128 v1 = _mm_mul_ps(_mm_loadu_ps(box + ofs + 4), px);

		ofs = signbits & 2 ? 16 : 24; // y min/max
		__m128 py = _mm_shuffle_ps(vplane, vplane, _MM_SHUFFLE(1, 1, 1, 1));
		v0 = _mm_add_ps(v0, _mm_mul_ps(_mm_loadu_ps(box + ofs), py));
		v1 = _mm_add_ps(v1, _mm_mul_ps(_mm_loadu_ps(box + ofs + 4), py));

		ofs = signbits & 4 ? 32 : 40; // z min/max
		__m128 pz = _mm_shuffle_ps(vplane, vplane, _MM_SHUFFLE(2, 2, 2, 2));
		v0 = _mm_add_ps(v0, _mm_mul_ps(_mm_loadu_ps(box + ofs), pz));
		v1 = _mm_add_ps(v1, _mm_mul_ps(_mm_loadu_ps(box + ofs + 4), pz));

		__m128 pd = _mm_shuffle_ps(vplane, vplane, _MM_SHUFFLE(3, 3, 3, 3));
		activelanes &= _mm_movemask_ps(_mm_cmplt_ps(pd, v0)) | (_mm_movemask_ps(_mm_cmplt_ps(pd, v1)) << 4);
	}

	return activelanes;
}
#endif // defined(USE_SSE2)

/*
===============
R_MarkVisSurfaces
===============
*/
static void R_MarkVisSurfaces (byte* vis)
{
	int			i;
	GLuint		buf;
	GLbyte*		ofs;
	size_t		vissize = (cl.worldmodel->numleafs + 7) >> 3;
	gpumark_frame_t frame;

	GL_BeginGroup ("Mark surfaces");

	for (i = 0; i < 4; i++)
	{
		frame.frustum[i][0] = frustum[i].normal[0];
		frame.frustum[i][1] = frustum[i].normal[1];
		frame.frustum[i][2] = frustum[i].normal[2];
		frame.frustum[i][3] = frustum[i].dist;
	}
	frame.vieworg[0] = r_refdef.vieworg[0];
	frame.vieworg[1] = r_refdef.vieworg[1];
	frame.vieworg[2] = r_refdef.vieworg[2];
	frame.oldskyleaf = r_oldskyleaf.value != 0.f;
	frame.framecount = r_framecount;

	vissize = (vissize + 3) & ~3; // round up to uint

	GL_UseProgram (glprogs.clear_indirect);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, gl_bmodel_indirect_buffer, 0, cl.worldmodel->texofs[TEXTYPE_COUNT] * sizeof(bmodel_draw_indirect_t));
	GL_DispatchComputeFunc ((cl.worldmodel->texofs[TEXTYPE_COUNT] + 63) / 64, 1, 1);
	GL_MemoryBarrierFunc (GL_SHADER_STORAGE_BARRIER_BIT);

	GL_UseProgram (glprogs.cull_mark);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 2, gl_bmodel_ibo, 0, gl_bmodel_ibo_size);
	GL_Upload (GL_SHADER_STORAGE_BUFFER, vis, vissize, &buf, &ofs);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 3, buf, (GLintptr)ofs, vissize);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 4, gl_bmodel_leaf_buffer, 0, cl.worldmodel->numleafs * sizeof(bmodel_gpu_leaf_t));
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 5, gl_bmodel_marksurf_buffer, 0, cl.worldmodel->nummarksurfaces * sizeof(cl.worldmodel->marksurfaces[0]));
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 6, gl_bmodel_surf_buffer, 0, cl.worldmodel->numsurfaces * sizeof(bmodel_gpu_surf_t));
	GL_Upload (GL_UNIFORM_BUFFER, &frame, sizeof(frame), &buf, &ofs);
	GL_BindBufferRange (GL_UNIFORM_BUFFER, 1, buf, (GLintptr)ofs, sizeof(frame));

	GL_DispatchComputeFunc ((cl.worldmodel->numleafs + 63) / 64, 1, 1);
	GL_MemoryBarrierFunc (GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

	GL_EndGroup ();
}

#if defined(USE_SIMD)
/*
===============
R_AddStaticModelsSIMD
===============
*/
void R_AddStaticModelsSIMD (const byte *vis)
{
	int			i, j;
	int			numleafs = cl.worldmodel->numleafs;
	soa_aabb_t	*leafbounds = cl.worldmodel->soa_leafbounds;

	for (i = 0; i < numleafs; i += 8)
	{
		efrag_t **efrags;
		int mask = vis[i>>3];
		if (mask == 0)
			continue;

		mask = R_CullBoxSIMD (leafbounds[i>>3], mask);
		if (mask == 0)
			continue;

		for (j = 0, efrags = &cl.worldmodel->leaf_efrags[1 + i]; j < 8 && 1 + i + j < numleafs; j++, efrags++)
			if ((mask & (1 << j)) && *efrags)
				R_StoreEfrags (efrags);
	}
}
#endif // defined(USE_SIMD)

/*
===============
R_AddStaticModels
===============
*/
void R_AddStaticModels (const byte* vis)
{
	int			i;
	mleaf_t		*leaf;
	efrag_t		**efrags;

	for (i = 0, leaf = &cl.worldmodel->leafs[1], efrags = &cl.worldmodel->leaf_efrags[1]; 1 + i < cl.worldmodel->numleafs; i++, leaf++, efrags++)
		if (vis[i>>3] & (1<<(i&7)) && *efrags && !R_CullBox(leaf->minmaxs, leaf->minmaxs + 3))
			R_StoreEfrags (efrags);
}

/*
===============
R_MarkSurfaces
===============
*/
void R_MarkSurfaces (void)
{
	byte		*vis;
	int			i;
	qboolean	nearwaterportal;

	// check this leaf for water portals
	// TODO: loop through all water surfs and use distance to leaf cullbox
	nearwaterportal = false;
	for (i=0; i < r_viewleaf->nummarksurfaces; i++)
		if (cl.worldmodel->surfaces[r_viewleaf->firstmarksurface[i]].flags & SURF_DRAWTURB)
			nearwaterportal = true;

	// choose vis data
	if (r_novis.value || r_viewleaf->contents == CONTENTS_SOLID || r_viewleaf->contents == CONTENTS_SKY)
		vis = Mod_NoVisPVS (cl.worldmodel);
	else if (nearwaterportal)
		vis = SV_FatPVS (r_origin, cl.worldmodel);
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	r_visframecount++;

	R_MarkVisSurfaces (vis);

#if defined(USE_SIMD)
	if (use_simd)
		R_AddStaticModelsSIMD (vis);
	else
#endif
		R_AddStaticModels (vis);
}

/*
================
GL_WaterAlphaForEntityTextureType
 
Returns the water alpha to use for the entity and texture type combination.
================
*/
float GL_WaterAlphaForEntityTextureType (entity_t *ent, textype_t type)
{
	float entalpha;
	if (ent == NULL || ent->alpha == ENTALPHA_DEFAULT)
		entalpha = GL_WaterAlphaForTextureType(type);
	else
		entalpha = ENTALPHA_DECODE(ent->alpha);
	return entalpha;
}

/*
=============
GLWorld_AllocIndirectBuffer
=============
*/
static void GLWorld_AllocIndirectBuffer (void)
{
	if (gl_bmodel_cmdbuf)
		GL_AddGarbageBuffer (gl_bmodel_cmdbuf);
	GL_GenBuffersFunc (1, &gl_bmodel_cmdbuf);
	GL_BindBuffer (GL_DRAW_INDIRECT_BUFFER, gl_bmodel_cmdbuf);
	GL_BufferDataFunc (GL_DRAW_INDIRECT_BUFFER, gl_bmodel_cmdbuf_size, NULL, GL_DYNAMIC_DRAW);
	GL_BindBuffer (GL_DRAW_INDIRECT_BUFFER, 0);
	gl_bmodel_cmdbuf_offset = 0;
}

/*
=============
GLWorld_CreateResources
=============
*/
void GLWorld_CreateResources (void)
{
	gl_bmodel_cmdbuf_size = 256 * 1024;
	GLWorld_AllocIndirectBuffer ();
}

typedef struct bmodel_gpu_instance_s {
	float		world[12];	// world matrix (transposed mat4x3)
	float		alpha;
	float		padding[3];
} bmodel_gpu_instance_t;

typedef struct bmodel_bindless_gpu_call_s {
	GLuint		flags;
	GLfloat		alpha;
	GLuint64	texture;
	GLuint64	fullbright;
} bmodel_bindless_gpu_call_t;

typedef struct bmodel_bound_gpu_call_s {
	GLuint		flags;
	GLfloat		alpha;
	GLint		baseinstance;
	GLint		padding;
} bmodel_bound_gpu_call_t;

typedef struct bmodel_gpu_call_remap_s {
	GLuint		src;
	GLuint		inst;
} bmodel_gpu_call_remap_t;

static bmodel_gpu_instance_t		bmodel_instances[MAX_VISEDICTS + 1]; // +1 for worldspawn
static union {
	struct {
		bmodel_bindless_gpu_call_t	params[MAX_BMODEL_DRAWS];
	} bindless;
	struct {
		bmodel_bound_gpu_call_t		params[MAX_BMODEL_DRAWS];
		gltexture_t					*textures[MAX_BMODEL_DRAWS][2];
	} bound;
} bmodel_calls;
static bmodel_gpu_call_remap_t		bmodel_call_remap[MAX_BMODEL_DRAWS];
static int							num_bmodel_calls;
static GLuint						bmodel_batch_program;
static int							bmodel_framecount;

/*
=============
R_InitBModelInstance
=============
*/
static void R_InitBModelInstance (bmodel_gpu_instance_t *inst, entity_t *ent)
{
	vec3_t angles;
	float mat[16];

	angles[0] = -ent->angles[0];
	angles[1] =  ent->angles[1];
	angles[2] =  ent->angles[2];
	R_EntityMatrix (mat, ent->origin, angles);

	#define COPY_ROW(row)					\
		inst->world[row*4+0] = mat[row+0],	\
		inst->world[row*4+1] = mat[row+4],	\
		inst->world[row*4+2] = mat[row+8],	\
		inst->world[row*4+3] = mat[row+12]

	COPY_ROW (0);
	COPY_ROW (1);
	COPY_ROW (2);

	#undef COPY_ROW

	inst->alpha = ent->alpha == ENTALPHA_DEFAULT ? -1.f : ENTALPHA_DECODE (ent->alpha);
	memset (&inst->padding, 0, sizeof(inst->padding));
}

/*
=============
R_ResetBModelCalls
=============
*/
static void R_ResetBModelCalls (GLuint program)
{
	bmodel_batch_program = program;
	num_bmodel_calls = 0;
}

/*
=============
R_FlushBModelCalls
=============
*/
static void R_FlushBModelCalls (void)
{
	GLuint	buf;
	GLbyte	*ofs;
	size_t	dstcmdofs;

	if (!num_bmodel_calls)
		return;

	if (bmodel_framecount != r_framecount)
	{
		bmodel_framecount = r_framecount;
		gl_bmodel_cmdbuf_offset = 0;
	}

	if (gl_bmodel_cmdbuf_offset + sizeof (bmodel_draw_indirect_t) * num_bmodel_calls > gl_bmodel_cmdbuf_size)
	{
		gl_bmodel_cmdbuf_size = gl_bmodel_cmdbuf_offset + sizeof (bmodel_draw_indirect_t) * num_bmodel_calls;
		gl_bmodel_cmdbuf_size += gl_bmodel_cmdbuf_size >> 1;
		GLWorld_AllocIndirectBuffer ();
	}

	dstcmdofs = gl_bmodel_cmdbuf_offset;
	gl_bmodel_cmdbuf_offset +=
		(sizeof (bmodel_draw_indirect_t) * num_bmodel_calls + ssbo_align) & ~ssbo_align;

	GL_UseProgram (glprogs.gather_indirect);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 5, gl_bmodel_indirect_buffer, 0, gl_bmodel_indirect_buffer_size);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 6, gl_bmodel_cmdbuf, dstcmdofs, sizeof (bmodel_draw_indirect_t) * num_bmodel_calls);
	GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_call_remap, sizeof (bmodel_call_remap[0]) * num_bmodel_calls, &buf, &ofs);
	GL_BindBufferRange  (GL_SHADER_STORAGE_BUFFER, 7, buf, (GLintptr)ofs, sizeof (bmodel_call_remap[0]) * num_bmodel_calls);
	GL_DispatchComputeFunc ((num_bmodel_calls + 63) / 64, 1, 1);
	GL_MemoryBarrierFunc (GL_COMMAND_BARRIER_BIT);

	GL_UseProgram (bmodel_batch_program);
	GL_BindBuffer (GL_ELEMENT_ARRAY_BUFFER, gl_bmodel_ibo);
	GL_BindBuffer (GL_ARRAY_BUFFER, gl_bmodel_vbo);
	GL_BindBuffer (GL_DRAW_INDIRECT_BUFFER, gl_bmodel_cmdbuf);
	GL_VertexAttribPointerFunc (0, 3, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, pos));
	GL_VertexAttribPointerFunc (1, 4, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, st));
	GL_VertexAttribPointerFunc (2, 1, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, lmofs));
	GL_VertexAttribIPointerFunc (3, 4, GL_UNSIGNED_BYTE, sizeof (glvert_t), (void *) offsetof (glvert_t, styles));

	if (gl_bindless_able)
	{
		GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_calls.bindless.params, sizeof (bmodel_calls.bindless.params[0]) * num_bmodel_calls, &buf, &ofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, buf, (GLintptr)ofs, sizeof (bmodel_calls.bindless.params[0]) * num_bmodel_calls);
		GL_MultiDrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const void *)dstcmdofs, num_bmodel_calls, sizeof (bmodel_draw_indirect_t));
	}
	else
	{
		int i;

		GL_Upload (GL_SHADER_STORAGE_BUFFER, &bmodel_calls.bound.params, sizeof (bmodel_calls.bound.params[0]) * num_bmodel_calls, &buf, &ofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, buf, (GLintptr)ofs, sizeof (bmodel_calls.bound.params[0]) * num_bmodel_calls);

		for (i = 0; i < num_bmodel_calls; i++)
		{
			GL_Uniform1iFunc (0, i);
			GL_BindTextures (0, 2, bmodel_calls.bound.textures[i]);
			GL_DrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const byte *)(dstcmdofs + i * sizeof (bmodel_draw_indirect_t)));
		}
	}

	num_bmodel_calls = 0;
}

/*
=============
R_AddBModelCall
=============
*/
static void R_AddBModelCall (int index, int first_instance, int num_instances, texture_t *t, qboolean zfix)
{
	GLuint		flags;
	float		alpha;
	gltexture_t	*tx, *fb;

	if (num_bmodel_calls == MAX_BMODEL_DRAWS)
		R_FlushBModelCalls ();

	if (t)
	{
		tx = t->gltexture;
		fb = t->fullbright;
		if (r_lightmap_cheatsafe)
			tx = fb = NULL;
		if (!gl_fullbrights.value && t->type != TEXTYPE_SKY)
			fb = NULL;
	}
	else
	{
		tx = fb = whitetexture;
	}

	if (!gl_zfix.value)
		zfix = 0;

	flags = zfix | ((fb != NULL) << 1) | ((r_fullbright_cheatsafe != false) << 2);
	alpha = t ? GL_WaterAlphaForTextureType (t->type) : 1.f;

	if (gl_bindless_able)
	{
		bmodel_bindless_gpu_call_t *call = &bmodel_calls.bindless.params[num_bmodel_calls];
		call->flags = flags;
		call->alpha = alpha;
		call->texture = tx ? tx->bindless_handle : greytexture->bindless_handle;
		call->fullbright = fb ? fb->bindless_handle : blacktexture->bindless_handle;
	}
	else
	{
		bmodel_bound_gpu_call_t *call = &bmodel_calls.bound.params[num_bmodel_calls];
		gltexture_t **textures = bmodel_calls.bound.textures[num_bmodel_calls];
		call->flags = flags;
		call->alpha = alpha;
		call->baseinstance = first_instance;
		call->padding = 0;
		textures[0] = tx ? tx : greytexture;
		textures[1] = fb ? fb : blacktexture;
	}

	SDL_assert (num_instances > 0);
	SDL_assert (num_instances <= MAX_BMODEL_INSTANCES);
	bmodel_call_remap[num_bmodel_calls].src = index;
	bmodel_call_remap[num_bmodel_calls].inst = first_instance * MAX_BMODEL_INSTANCES + (num_instances - 1);

	++num_bmodel_calls;
}

typedef enum {
	BP_SOLID,
	BP_ALPHATEST,
	BP_SKYLAYERS,
	BP_SKYCUBEMAP,
	BP_SKYSTENCIL,
	BP_SHOWTRIS,
} brushpass_t;

/*
=============
R_DrawBrushModels_Real
=============
*/
static void R_DrawBrushModels_Real (entity_t **ents, int count, brushpass_t pass)
{
	int i, j;
	int totalinst, baseinst;
	unsigned state;
	GLuint program;
	GLuint buf;
	GLbyte *ofs;
	textype_t texbegin, texend;

	if (!count)
		return;

	if (count > countof(bmodel_instances))
	{
		Con_DWarning ("bmodel instance overflow: %d > %d\n", count, (int)countof(bmodel_instances));
		count = countof(bmodel_instances);
	}

	switch (pass)
	{
	default:
	case BP_SOLID:
		texbegin = 0;
		texend = TEXTYPE_CUTOUT;
		program = glprogs.world[q_max(0, (int)softemu - 1)][WORLDSHADER_SOLID];
		break;
	case BP_ALPHATEST:
		texbegin = TEXTYPE_CUTOUT;
		texend = TEXTYPE_CUTOUT + 1;
		program = glprogs.world[q_max(0, (int)softemu - 1)][WORLDSHADER_ALPHATEST];
		break;
	case BP_SKYLAYERS:
		texbegin = TEXTYPE_SKY;
		texend = TEXTYPE_SKY + 1;
		program = glprogs.skylayers[softemu == SOFTEMU_COARSE];
		break;
	case BP_SKYCUBEMAP:
		texbegin = TEXTYPE_SKY;
		texend = TEXTYPE_SKY + 1;
		program = glprogs.skycubemap[softemu == SOFTEMU_COARSE];
		break;
	case BP_SKYSTENCIL:
		texbegin = TEXTYPE_SKY;
		texend = TEXTYPE_SKY + 1;
		program = glprogs.skystencil;
		break;
	case BP_SHOWTRIS:
		texbegin = 0;
		texend = TEXTYPE_COUNT;
		program = glprogs.world[0][0];
		break;
	}

	// fill instance data
	for (i = 0, totalinst = 0; i < count; i++)
		if (ents[i]->model->texofs[texend] - ents[i]->model->texofs[texbegin] > 0)
			R_InitBModelInstance (&bmodel_instances[totalinst++], ents[i]);

	if (!totalinst)
		return;

	// setup state
	state = GLS_CULL_BACK | GLS_ATTRIBS(4);
	if (ents[0] == cl_entities || ENTALPHA_OPAQUE (ents[0]->alpha))
		state |= GLS_BLEND_OPAQUE;
	else
		state |= GLS_BLEND_ALPHA | GLS_NO_ZWRITE;
	
	R_ResetBModelCalls (program);
	GL_SetState (state);
	if (pass <= BP_ALPHATEST)
		GL_Bind (GL_TEXTURE2, r_fullbright_cheatsafe ? greytexture : lightmap_texture);
	else if (pass == BP_SKYCUBEMAP)
		GL_Bind (GL_TEXTURE2, skybox_cubemap);

	GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_instances, sizeof(bmodel_instances[0]) * count, &buf, &ofs);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 2, buf, (GLintptr)ofs, sizeof(bmodel_instances[0]) * count);

	// generate drawcalls
	for (i = 0, baseinst = 0; i < count; /**/)
	{
		int numinst;
		entity_t *e = ents[i++];
		qmodel_t *model = e->model;
		qboolean isworld = (e == &cl_entities[0]);
		int frame = isworld ? 0 : e->frame;
		int numtex = model->texofs[texend] - model->texofs[texbegin];

		if (!numtex)
			continue;

		for (numinst = 1; i < count && ents[i]->model == model && numinst < MAX_BMODEL_INSTANCES; i++)
			numinst += (ents[i]->model->texofs[texend] - ents[i]->model->texofs[texbegin]) > 0;

		for (j = model->texofs[texbegin]; j < model->texofs[texend]; j++)
		{
			texture_t *t = model->textures[model->usedtextures[j]];
			R_AddBModelCall (model->firstcmd + j, baseinst, numinst, pass != BP_SHOWTRIS ? R_TextureAnimation (t, frame) : 0, !isworld);
		}

		baseinst += numinst;
	}

	R_FlushBModelCalls ();
}

/*
=============
R_EntHasWater
=============
*/
static qboolean R_EntHasWater (entity_t *ent, qboolean translucent)
{
	int i;
	for (i = TEXTYPE_FIRSTLIQUID; i < TEXTYPE_LASTLIQUID+1; i++)
	{
		int numtex = ent->model->texofs[i+1] - ent->model->texofs[i];
		if (numtex && (GL_WaterAlphaForEntityTextureType (ent, (textype_t)i) < 1.f) == translucent)
			return true;
	}
	return false;
}

/*
=============
R_DrawBrushModels_Water
=============
*/
void R_DrawBrushModels_Water (entity_t **ents, int count, qboolean translucent)
{
	int i, j;
	int totalinst, baseinst;
	unsigned state;
	GLuint buf, program;
	GLbyte *ofs;

	if (count > countof(bmodel_instances))
	{
		Con_DWarning ("bmodel instance overflow: %d > %d\n", count, (int)countof(bmodel_instances));
		count = countof(bmodel_instances);
	}

	// fill instance data
	for (i = 0, totalinst = 0; i < count; i++)
		if (R_EntHasWater (ents[i], translucent))
			R_InitBModelInstance (&bmodel_instances[totalinst++], ents[i]);

	if (!totalinst)
		return;

	GL_BeginGroup (translucent ? "Water (translucent)" : "Water (opaque)");

	// setup state
	state = GLS_CULL_BACK | GLS_ATTRIBS(4);
	if (translucent)
		state |= GLS_BLEND_ALPHA | GLS_NO_ZWRITE;
	else
		state |= GLS_BLEND_OPAQUE;

	if (cl.worldmodel->haslitwater && r_litwater.value)
		program = glprogs.world[q_max(0, (int)softemu - 1)][WORLDSHADER_WATER];
	else
		program = glprogs.water[softemu == SOFTEMU_COARSE];

	R_ResetBModelCalls (program);
	GL_SetState (state);
	GL_Bind (GL_TEXTURE2, r_fullbright_cheatsafe ? greytexture : lightmap_texture);

	GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_instances, sizeof(bmodel_instances[0]) * totalinst, &buf, &ofs);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 2, buf, (GLintptr)ofs, sizeof(bmodel_instances[0]) * count);

	// generate drawcalls
	for (i = 0, baseinst = 0; i < count; /**/)
	{
		int numinst;
		entity_t *e = ents[i++];
		qmodel_t *model = e->model;
		qboolean isworld = (e == &cl_entities[0]);
		int frame = isworld ? 0 : e->frame;

		if (!R_EntHasWater (e, translucent))
			continue;

		for (numinst = 1; i < count && ents[i]->model == model && numinst < MAX_BMODEL_INSTANCES; i++)
			numinst += R_EntHasWater (ents[i], translucent);

		for (j = model->texofs[TEXTYPE_FIRSTLIQUID]; j < model->texofs[TEXTYPE_LASTLIQUID+1]; j++)
		{
			texture_t *t = model->textures[model->usedtextures[j]];
			if ((GL_WaterAlphaForEntityTextureType (e, t->type) < 1.f) != translucent)
				continue;
			R_AddBModelCall (model->firstcmd + j, baseinst, numinst, R_TextureAnimation (t, frame), !isworld);
		}

		baseinst += numinst;
	}

	R_FlushBModelCalls ();

	GL_EndGroup ();
}

/*
=============
R_DrawBrushModels
=============
*/
void R_DrawBrushModels (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SOLID);
	R_DrawBrushModels_Real (ents, count, BP_ALPHATEST);
}

/*
=============
R_DrawBrushModels_SkyLayers
=============
*/
void R_DrawBrushModels_SkyLayers (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SKYLAYERS);
}

/*
=============
R_DrawBrushModels_SkyCubemap
=============
*/
void R_DrawBrushModels_SkyCubemap (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SKYCUBEMAP);
}

/*
=============
R_DrawBrushModels_SkyStencil
=============
*/
void R_DrawBrushModels_SkyStencil (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SKYSTENCIL);
}

/*
=============
R_DrawBrushModels_ShowTris
=============
*/
void R_DrawBrushModels_ShowTris (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SHOWTRIS);
}
