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

/*
========================================================================================================================================================================

NEW PARTICLE SYSTEM

Features:

- Framerate-independent particle physics (accurately replicates classic Quake particle physics but in a framerate-independent manner).
- Unlimited (except by memory) particles dynamically allocated at runtime (useful for pointfiles, but even the ID1 timedemos overflow the default max of 2048).
- Correctly handles cases of particles that last forever.
- Correctly handles cases of particles that only last one frame.
- Restores r_particles 3 option.
- Performs particle scaling and billboarding entirely on the GPU for added performance.
- Reduced upload bandwidth requirement.
- Reduced particle fillrate overheads by drawing as quads (tristrips, actually) instead of triangles.

Implementation.

- This file (replaces existing r_part.c).
- New particles vertex shader in gl_shaders.h, reproduced below.
- Move call to R_ClearParticles from R_NewMap to Host_ClearMemory, *AFTER* the Hunk_FreeToLowMark call.
- Remove ptype_t and particle_t from glquake.h because they're in r_part.c now.
- Move call to CL_RunParticles in _Host_frame from after SCR_UpdateScreen to *BEFORE* it.
- Add definitions for DrawArraysInstanced and VertexAttribDivisor to QGL_CORE_FUNCTIONS:
	x(void,			DrawArraysInstanced, (GLenum mode, GLint first, GLsizei count, GLsizei primcount))\
	x(void,			VertexAttribDivisor, (GLuint index, GLuint divisor))\

======== NEW PARTICLES VERTEX SHADER ===================================================================================================================================

	static const char particles_vertex_shader[] =
	FRAMEDATA_BUFFER
	"\n"
	"layout(location=0) in vec3 in_pos;\n"
	"layout(location=1) in vec4 in_color;\n"
	"\n"
	"layout(location=0) out vec2 out_uv;\n"
	"layout(location=1) out vec4 out_color;\n"
	"layout(location=2) out float out_fogdist;\n"
	"\n"
	"uniform vec3 r_origin;\n"
	"uniform vec3 vpn;\n"
	"uniform vec3 vup;\n"
	"uniform vec3 vright;\n"
	"uniform float texturescalefactor;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	// figure the current corner\n"
	"	int vtx = gl_VertexID % 3;\n"
	"	vec2 corner = vec2 (vtx == 1, vtx == 2);\n"
	"\n"
	"	// hack a scale up to keep particles from disappearing\n"
	"	float scale = max (1.0 + dot (in_pos - r_origin, vpn) * 0.004, 1.08) * texturescalefactor;\n"
	"\n"
	"	// perform the billboarding\n"
	"	vec3 out_pos = in_pos + (vup * corner.x + vright * corner.y) * scale;\n"
	"\n"
	"	gl_Position = ViewProj * vec4 (out_pos, 1.0);\n"
	"	out_fogdist = gl_Position.w;\n"
	"	out_uv = corner;\n"
	"	out_color = in_color;\n"
	"}\n";

========================================================================================================================================================================
*/

#include "quakedef.h"

// default # of particles per batch
// the ID1 timedemos require this number of particles
#define MAX_PARTICLES			4096

// additional number of particles per allocation
#define EXTRA_PARTICLES			128


#define PF_ONEFRAME		(1 << 0)		// particle is removed after one frame irrespective of die times
#define PF_ETERNAL		(1 << 1)		// particle is never removed irrespective of die times

// ramps are made larger to allow for overshoot, using full alpha as the dead particle; anything exceeding ramp[8] is automatically dead
int ramp1[] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
int ramp2[] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
int ramp3[] = {0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


typedef struct ptypedef_s {
	vec3_t dvel;
	vec3_t grav;
	int *ramp;
	float ramptime;
	vec3_t accel;
} ptypedef_t;


typedef enum {
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2, pt_entparticles, MAX_PARTICLE_TYPES
} ptype_t;


// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
	// driver-usable fields
	vec3_t		org;
	vec3_t		move;
	int			color;

	// drivers never touch the following fields
	vec3_t		vel;
	float		ramp;
	float		die;
	float		time;
	int			flags;
	ptype_t		type;

	struct particle_s *next;
} particle_t;


particle_t *active_particles, *free_particles;


gltexture_t *particletexture, *particletexture1, *particletexture2, *particletexture3; //johnfitz
float texturescalefactor; //johnfitz -- compensate for apparent size of different particle textures

// mh - blob particles
cvar_t	r_particles = {"r_particles", "3", CVAR_ARCHIVE}; //johnfitz


typedef struct particlevert_t {
	vec3_t		pos;
	unsigned	color;
} particlevert_t;


/*
===============
R_ParticleTextureLookup -- johnfitz -- generate nice antialiased 32x32 circle for particles
===============
*/
int R_ParticleTextureLookup (int x, int y, int sharpness)
{
	int r; //distance from point x,y to circle origin, squared
	int a; //alpha value to return

	x -= 16;
	y -= 16;
	r = x * x + y * y;
	r = r > 255 ? 255 : r;
	a = sharpness * (255 - r);
	a = q_min (a, 255);
	return a;
}


/*
===============
R_InitParticleTextures -- johnfitz -- rewritten
===============
*/
void R_InitParticleTextures (void)
{
	int			x, y;
	static byte	particle1_data[64 * 64 * 4];
	static byte	particle2_data[2 * 2 * 4];
	static byte	particle3_data[64 * 64 * 4];
	byte *dst;

	// particle texture 1 -- circle
	dst = particle1_data;
	for (x = 0; x < 64; x++)
		for (y = 0; y < 64; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = R_ParticleTextureLookup (x, y, 8);
		}
	particletexture1 = TexMgr_LoadImage (NULL, "particle1", 64, 64, SRC_RGBA, particle1_data, "", (src_offset_t) particle1_data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_LINEAR);

	// particle texture 2 -- square
	dst = particle2_data;
	for (x = 0; x < 2; x++)
		for (y = 0; y < 2; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = x || y ? 0 : 255;
		}
	particletexture2 = TexMgr_LoadImage (NULL, "particle2", 2, 2, SRC_RGBA, particle2_data, "", (src_offset_t) particle2_data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_NEAREST);

	// particle texture 3 -- blob
	dst = particle3_data;
	for (x = 0; x < 64; x++)
		for (y = 0; y < 64; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = R_ParticleTextureLookup (x, y, 2);
		}
	particletexture3 = TexMgr_LoadImage (NULL, "particle3", 64, 64, SRC_RGBA, particle3_data, "", (src_offset_t) particle3_data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_LINEAR);
}


/*
===============
R_SetParticleTexture_f -- johnfitz
===============
*/
static void R_SetParticleTexture_f (cvar_t *var)
{
	switch ((int) (r_particles.value))
	{
	case 1:
		particletexture = particletexture1;
		texturescalefactor = 1.27;
		break;
	case 2:
		particletexture = particletexture2;
		texturescalefactor = 1.0;
		break;
	case 3:
		particletexture = particletexture3;
		texturescalefactor = 1.5;
		break;
	}
}


/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
	R_InitParticleTextures (); //johnfitz

	Cvar_RegisterVariable (&r_particles); //johnfitz
	Cvar_SetCallback (&r_particles, R_SetParticleTexture_f);
	R_SetParticleTexture_f (&r_particles); // set default
}


/*
===============
R_MoreParticles

expand the free particle list by the given count; may be called at any time, even if the free list is not NULL
===============
*/
void R_MoreParticles (int count)
{
	particle_t *particles = (particle_t *) Hunk_Alloc (count * sizeof (particle_t));

	for (int i = 1; i < count; i++)
		particles[i - 1].next = &particles[i];

	// link it into the free list (which may be NULL)
	particles[count - 1].next = free_particles;
	free_particles = particles;
}


/*
===============
R_AllocParticle
===============
*/
particle_t *R_AllocParticle (void)
{
	if (!free_particles)
	{
		// alloc a smaller batch
		R_MoreParticles (EXTRA_PARTICLES);

		// call recursively to use the new free particles
		return R_AllocParticle ();
	}

	// take the first free particle
	particle_t *p = free_particles;
	free_particles = p->next;

	// and move it to the active list
	p->next = active_particles;
	active_particles = p;

	// save off time the particle was spawned at for time-based effects
	p->time = cl.time;

	// initially no flags
	p->flags = 0;

	// and return what we got
	return p;
}


/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles (void)
{
	free_particles = NULL;
	active_particles = NULL;

	// alloc an initial batch to prevent the hunk being hit first time particles are seen
	R_MoreParticles (MAX_PARTICLES);
}


/*
===============
R_EntityParticles
===============
*/
#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;
vec3_t	avelocity = {23, 7, 3};
float	partstep = 0.01;
float	timescale = 0.01;

void R_EntityParticles (entity_t *ent)
{
	int		i;
	particle_t *p;
	float		angle;
	float		sp, sy, cp, cy;
	//	float		sr, cr;
	//	int		count;
	vec3_t		forward;
	float		dist;

	dist = 64;
	//	count = 50;

	if (!avelocities[0][0])
	{
		for (i = 0; i < NUMVERTEXNORMALS; i++)
		{
			avelocities[i][0] = (rand () & 255) * 0.01;
			avelocities[i][1] = (rand () & 255) * 0.01;
			avelocities[i][2] = (rand () & 255) * 0.01;
		}
	}

	for (i = 0; i < NUMVERTEXNORMALS; i++)
	{
		angle = cl.time * avelocities[i][0];
		sy = sin (angle);
		cy = cos (angle);
		angle = cl.time * avelocities[i][1];
		sp = sin (angle);
		cp = cos (angle);
		angle = cl.time * avelocities[i][2];
		//	sr = sin(angle);
		//	cr = cos(angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if ((p = R_AllocParticle ()) == NULL) break;

		// this just needs to be higher than cl.time and PF_ONEFRAME will manage the rest
		p->die = cl.time + 1;
		p->flags |= PF_ONEFRAME;

		p->color = 0x6f;
		p->type = pt_entparticles;

		p->org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist + forward[0] * beamlength;
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist + forward[1] * beamlength;
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist + forward[2] * beamlength;
	}
}


/*
===============
R_ReadPointFile_f
===============
*/
void R_ReadPointFile_f (void)
{
	FILE *f;
	vec3_t	org;
	int		r;
	int		c;
	particle_t *p;
	char	name[MAX_QPATH];

	if (cls.state != ca_connected)
		return;			// need an active map.

	q_snprintf (name, sizeof (name), "maps/%s.pts", cl.mapname);

	COM_FOpenFile (name, &f, NULL);
	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}

	Con_Printf ("Reading %s...\n", name);
	c = 0;
	org[0] = org[1] = org[2] = 0; // silence pesky compiler warnings
	for (;; )
	{
		r = fscanf (f, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if ((p = R_AllocParticle ()) == NULL)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}

		// this just needs to be higher than cl.time and PF_ETERNAL will manage the rest
		p->die = cl.time + 1;
		p->flags |= PF_ETERNAL;

		p->color = (-c) & 15;
		p->type = pt_static;

		VectorCopy (vec3_origin, p->vel);
		VectorCopy (org, p->org);
	}

	fclose (f);
	Con_Printf ("%i points read\n", c);
}

/*
===============
R_ParticleExplosion
===============
*/
void R_ParticleExplosion (vec3_t org)
{
	int			i, j;
	particle_t *p;

	for (i = 0; i < 1024; i++)
	{
		if ((p = R_AllocParticle ()) == NULL) break;

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand () & 3;
		if (i & 1)
		{
			p->type = pt_explode;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand () % 32) - 16);
				p->vel[j] = (rand () % 512) - 256;
			}
		}
		else
		{
			p->type = pt_explode2;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand () % 32) - 16);
				p->vel[j] = (rand () % 512) - 256;
			}
		}
	}
}


/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, color;

	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord (cl.protocolflags);
	for (i = 0; i < 3; i++)
		dir[i] = MSG_ReadChar () * (1.0 / 16);
	count = MSG_ReadByte ();
	color = MSG_ReadByte ();

	if (count == 255)
		R_ParticleExplosion (org);
	else R_RunParticleEffect (org, dir, color, count);
}


/*
===============
R_ParticleExplosion2
===============
*/
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int			i, j;
	particle_t *p;
	int			colorMod = 0;

	for (i = 0; i < 512; i++)
	{
		if ((p = R_AllocParticle ()) == NULL) break;

		p->die = cl.time + 0.3;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->type = pt_blob;
		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () % 32) - 16);
			p->vel[j] = (rand () % 512) - 256;
		}
	}
}

/*
===============
R_BlobExplosion
===============
*/
void R_BlobExplosion (vec3_t org)
{
	int			i, j;
	particle_t *p;

	for (i = 0; i < 1024; i++)
	{
		if ((p = R_AllocParticle ()) == NULL) break;

		p->die = cl.time + 1 + (rand () & 8) * 0.05;

		if (i & 1)
		{
			p->type = pt_blob;
			p->color = 66 + rand () % 6;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand () % 32) - 16);
				p->vel[j] = (rand () % 512) - 256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand () % 6;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand () % 32) - 16);
				p->vel[j] = (rand () % 512) - 256;
			}
		}
	}
}

/*
===============
R_RunParticleEffect
===============
*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	particle_t *p;

	for (i = 0; i < count; i++)
	{
		if ((p = R_AllocParticle ()) == NULL) break;

		p->die = cl.time + 0.1 * (rand () % 5);
		p->color = (color & ~7) + (rand () & 7);
		p->type = pt_slowgrav;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 15) - 8);
			p->vel[j] = dir[j] * 15;// + (rand()%300)-150;
		}
	}
}

/*
===============
R_LavaSplash
===============
*/
void R_LavaSplash (vec3_t org)
{
	int			i, j, k;
	particle_t *p;
	float		vel;
	vec3_t		dir;

	for (i = -16; i < 16; i++)
		for (j = -16; j < 16; j++)
			for (k = 0; k < 1; k++)
			{
				if ((p = R_AllocParticle ()) == NULL) return;

				p->die = cl.time + 2 + (rand () & 31) * 0.02;
				p->color = 224 + (rand () & 7);
				p->type = pt_slowgrav;

				dir[0] = j * 8 + (rand () & 7);
				dir[1] = i * 8 + (rand () & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand () & 63);

				VectorNormalize (dir);
				vel = 50 + (rand () & 63);
				VectorScale (dir, vel, p->vel);
			}
}

/*
===============
R_TeleportSplash
===============
*/
void R_TeleportSplash (vec3_t org)
{
	int			i, j, k;
	particle_t *p;
	float		vel;
	vec3_t		dir;

	for (i = -16; i < 16; i += 4)
		for (j = -16; j < 16; j += 4)
			for (k = -24; k < 32; k += 4)
			{
				if ((p = R_AllocParticle ()) == NULL) return;

				p->die = cl.time + 0.2 + (rand () & 7) * 0.02;
				p->color = 7 + (rand () & 7);
				p->type = pt_slowgrav;

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (rand () & 3);
				p->org[1] = org[1] + j + (rand () & 3);
				p->org[2] = org[2] + k + (rand () & 3);

				VectorNormalize (dir);
				vel = 50 + (rand () & 63);
				VectorScale (dir, vel, p->vel);
			}
}

/*
===============
R_RocketTrail

FIXME -- rename function and use #defined types instead of numbers
===============
*/
void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		vec;
	float		len;
	int			j;
	particle_t *p;
	int			dec;
	static int	tracercount;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	if (type < 128)
		dec = 3;
	else
	{
		dec = 1;
		type -= 128;
	}

	while (len > 0)
	{
		len -= dec;

		if ((p = R_AllocParticle ()) == NULL) return;

		VectorCopy (vec3_origin, p->vel);
		p->die = cl.time + 2;

		switch (type)
		{
		case 0:	// rocket trail
			p->ramp = (rand () & 3);
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand () % 6) - 3);
			break;

		case 1:	// smoke smoke
			p->ramp = (rand () & 3) + 2;
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand () % 6) - 3);
			break;

		case 2:	// blood
			p->type = pt_grav;
			p->color = 67 + (rand () & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand () % 6) - 3);
			break;

		case 3:
		case 5:	// tracer
			p->die = cl.time + 0.5;
			p->type = pt_static;
			if (type == 3)
				p->color = 52 + ((tracercount & 4) << 1);
			else
				p->color = 230 + ((tracercount & 4) << 1);

			tracercount++;

			VectorCopy (start, p->org);
			if (tracercount & 1)
			{
				p->vel[0] = 30 * vec[1];
				p->vel[1] = 30 * -vec[0];
			}
			else
			{
				p->vel[0] = 30 * -vec[1];
				p->vel[1] = 30 * vec[0];
			}
			break;

		case 4:	// slight blood
			p->type = pt_grav;
			p->color = 67 + (rand () & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand () % 6) - 3);
			len -= 3;
			break;

		case 6:	// voor trail
			p->color = 9 * 16 + 8 + (rand () & 3);
			p->type = pt_static;
			p->die = cl.time + 0.3;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand () & 15) - 8);
			break;
		}

		VectorAdd (start, vec, start);
	}
}

/*
===============
CL_RunParticles -- johnfitz -- all the particle behavior, separated from R_DrawParticles
===============
*/
void CL_RunParticles (void)
{
	static ptypedef_t p_typedefs[MAX_PARTICLE_TYPES] = {
		{{0, 0, 0}, {0, 0, 0}, NULL, 0},		// pt_static
		{{0, 0, 0}, {0, 0, -1}, NULL, 0},		// pt_grav
		{{0, 0, 0}, {0, 0, -0.5}, NULL, 0},		// pt_slowgrav
		{{0, 0, 0}, {0, 0, 1}, ramp3, 5},		// pt_fire
		{{4, 4, 4}, {0, 0, -1}, ramp1, 10},		// pt_explode
		{{-1, -1, -1}, {0, 0, -1}, ramp2, 15},	// pt_explode2
		{{4, 4, 4}, {0, 0, -1}, NULL, 0},		// pt_blob
		{{-4, -4, 0}, {0, 0, -1}, NULL, 0},		// pt_blob2
		{{4, 4, 4}, {0, 0, -1}, NULL, 0},		// pt_entparticles replicates pt_explode but with no ramp
	};

	// update particle accelerations
	for (int i = 0; i < MAX_PARTICLE_TYPES; i++)
	{
		extern cvar_t sv_gravity;
		ptypedef_t *pt = &p_typedefs[i];
		float grav = sv_gravity.value * 0.05f;

		// in theory this could be calced once and never again, but in practice mods may change sv_gravity from frame-to-frame
		// so we need to recalc it each frame too.... (correcting the acceleration formula here too)
		pt->accel[0] = (pt->dvel[0] + (pt->grav[0] * grav)) * 0.5f;
		pt->accel[1] = (pt->dvel[1] + (pt->grav[1] * grav)) * 0.5f;
		pt->accel[2] = (pt->dvel[2] + (pt->grav[2] * grav)) * 0.5f;
	}

	for (;; )
	{
		particle_t *kill = active_particles;

		if (kill && kill->die < cl.time)
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}

		break;
	}

	for (particle_t *p = active_particles; p; p = p->next)
	{
		for (;; )
		{
			particle_t *kill = p->next;

			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}

			break;
		}

		// get the emitter properties for this particle
		ptypedef_t *pt = &p_typedefs[p->type];
		float etime = cl.time - p->time;

		// move the particle in a framerate-independent manner
		p->move[0] = (p->vel[0] + (pt->accel[0] * etime)) * etime;
		p->move[1] = (p->vel[1] + (pt->accel[1] * etime)) * etime;
		p->move[2] = (p->vel[2] + (pt->accel[2] * etime)) * etime;

		// update color ramps
		if (pt->ramp)
		{
			int ramp = (int) (p->ramp + (etime * pt->ramptime));

			// kill particles that are full alpha or that have expired ramps
			if (ramp > 8)
				p->die = -1;
			else if ((p->color = pt->ramp[ramp]) == 0xff)
				p->die = -1;
		}
	}
}


/*
===============
R_FlushParticleBatch
===============
*/
static void R_FlushParticleBatch (const particlevert_t *partverts, int numpartverts)
{
	GLuint buf;
	GLbyte *ofs;

	GL_Upload (GL_ARRAY_BUFFER, partverts, sizeof (partverts[0]) * numpartverts, &buf, &ofs);
	GL_BindBuffer (GL_ARRAY_BUFFER, buf);

	// the buffer stores color as a single unsigned int but we're going to set our pointer as if it were 4 bytes; this is legal because buffers are untyped
	GL_VertexAttribPointerFunc (0, 3, GL_FLOAT, GL_FALSE, sizeof (partverts[0]), ofs + offsetof (particlevert_t, pos));
	GL_VertexAttribPointerFunc (1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof (partverts[0]), ofs + offsetof (particlevert_t, color));

	// draw numpartverts instances of a single triangle strip; this saves on fillrate compared to triangle particles (with this setup we expect to be bottlenecked on fillrate, not vertices)
	GL_DrawArraysInstancedFunc (GL_TRIANGLE_STRIP, 0, 4, numpartverts);
}


/*
===============
R_DrawParticles_Real -- johnfitz -- moved all non-drawing code to CL_RunParticles
===============
*/
static void R_DrawParticles_Real (qboolean showtris)
{
	qboolean dither = (softemu == SOFTEMU_COARSE && !showtris);
	GLuint prog = glprogs.particles[dither];

	if (!r_particles.value)
		return;

	if (!active_particles)
		return;

	GL_BeginGroup ("Particles");
	GL_UseProgram (prog);

	// set up uniforms so we can billboard and scale on the GPU
	GL_Uniform3fvFunc (GL_GetUniformLocationFunc (prog, "r_origin"), 1, r_origin);
	GL_Uniform3fvFunc (GL_GetUniformLocationFunc (prog, "vpn"), 1, vpn);
	GL_Uniform3fvFunc (GL_GetUniformLocationFunc (prog, "vup"), 1, vup);
	GL_Uniform3fvFunc (GL_GetUniformLocationFunc (prog, "vright"), 1, vright);

	// compensate for apparent size of different particle textures; this bakes in the additional scaling of vup and vright by 1.5f for billboarding, then down by 0.25f for quad particles
	GL_Uniform1fFunc (GL_GetUniformLocationFunc (prog, "texturescalefactor"), texturescalefactor * 0.375f);

	GL_Bind (GL_TEXTURE0, showtris ? whitetexture : particletexture);

	GL_SetState (GLS_BLEND_ALPHA | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS (2));

	// set up instancing divisors
	GL_VertexAttribDivisorFunc (0, 1);
	GL_VertexAttribDivisorFunc (1, 1);

	// keep these private - they should not be accessed or modified outside of the drawing code
	static particlevert_t partverts[MAX_PARTICLES];
	int numpartverts = 0;

	for (particle_t *p = active_particles; p; p = p->next)
	{
		if (numpartverts == countof (partverts))
		{
			R_FlushParticleBatch (partverts, numpartverts);
			numpartverts = 0;
		}

		// move the particle in a framerate-independent manner
		partverts[numpartverts].pos[0] = p->org[0] + p->move[0];
		partverts[numpartverts].pos[1] = p->org[1] + p->move[1];
		partverts[numpartverts].pos[2] = p->org[2] + p->move[2];

		// if the particle is to be removed on the next frame, draw it with full alpha; otherwise draw it normally
		if (p->die < cl.time)
			partverts[numpartverts].color = 0x0;
		else if (!showtris)
			partverts[numpartverts].color = d_8to24table[p->color];
		else partverts[numpartverts].color = 0xffffffff;

		numpartverts++;
		rs_particles++;

		// update die flags - these must be done after drawing
		if (p->flags & PF_ETERNAL) p->die = cl.time + 1;
		if (p->flags & PF_ONEFRAME) p->die = -1;
	}

	if (numpartverts)
		R_FlushParticleBatch (partverts, numpartverts);

	// switch off instancing divisors
	GL_VertexAttribDivisorFunc (0, 0);
	GL_VertexAttribDivisorFunc (1, 0);

	GL_EndGroup ();
}

/*
===============
R_DrawParticles -- johnfitz -- moved all non-drawing code to CL_RunParticles
===============
*/
void R_DrawParticles (void)
{
	R_DrawParticles_Real (false);
}
/*
===============
R_DrawParticles_ShowTris -- johnfitz
===============
*/
void R_DrawParticles_ShowTris (void)
{
	R_DrawParticles_Real (true);
}

