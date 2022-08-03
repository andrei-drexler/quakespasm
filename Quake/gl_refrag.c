/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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
// r_efrag.c

#include "quakedef.h"

//===========================================================================

/*
===============================================================================

					ENTITY FRAGMENT FUNCTIONS

ericw -- GLQuake only uses efrags for static entities, and they're never
removed, so I trimmed out unused functionality and fields in efrag_t.

This is inspired by MH's tutorial, and code from RMQEngine.
http://forums.insideqc.com/viewtopic.php?t=1930
 
===============================================================================
*/

// leaf count followed by leaf indices, for each static ent with a non-NULL model
int			*cl_efrags;

vec3_t		r_emins, r_emaxs;

/*
===================
R_SplitEntityOnNode
===================
*/
static void R_SplitEntityOnNode (mnode_t *node)
{
	mplane_t	*splitplane;
	int			idx, sides;

	if (node->contents == CONTENTS_SOLID)
	{
		return;
	}

// add an efrag if the node is a leaf

	if (node->contents < 0)
	{
		idx = (mleaf_t *)node - cl.worldmodel->leafs;
		if (idx >= 1)
			VEC_PUSH (cl_efrags, idx - 1);
		return;
	}

// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);

// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNode (node->children[0]);

	if (sides & 2)
		R_SplitEntityOnNode (node->children[1]);
}

/*
===========
R_CheckEfrags -- johnfitz -- check for excessive efrag count
===========
*/
void R_CheckEfrags (void)
{
	if (cls.signon < 2)
		return; //don't spam when still parsing signon packet full of static ents

	if (cl.num_efrags > 640 && dev_peakstats.efrags <= 640)
		Con_DWarning ("%i efrags exceeds standard limit of 640.\n", cl.num_efrags);

	dev_stats.efrags = cl.num_efrags;
	dev_peakstats.efrags = q_max(cl.num_efrags, dev_peakstats.efrags);
}

/*
===========
R_ClearEfrags
===========
*/
void R_ClearEfrags (void)
{
	VEC_CLEAR (cl_efrags);
}

/*
===========
R_AddEfrags
===========
*/
void R_AddEfrags (entity_t *ent)
{
	qmodel_t	*entmodel;
	int			i;
	vec3_t boundVec, scaledVec;
	vec_t scalefactor;

	if (!ent->model)
		return;

	entmodel = ent->model;

	scalefactor = ENTSCALE_DECODE(ent->scale);
	if (scalefactor != 1.0f)
	{
		VectorCopy (entmodel->mins, boundVec);
		VectorScale (boundVec, scalefactor, scaledVec);
		VectorAdd (ent->origin, scaledVec, r_emins);

		VectorCopy (entmodel->maxs, boundVec);
		VectorScale (boundVec, scalefactor, scaledVec);
		VectorAdd (ent->origin, scaledVec, r_emaxs);
	}
	else
	{
		VectorAdd (ent->origin, entmodel->mins, r_emins);
		VectorAdd (ent->origin, entmodel->maxs, r_emaxs);
	}

	i = VEC_SIZE (cl_efrags);
	VEC_PUSH (cl_efrags, 0); // write dummy count

	R_SplitEntityOnNode (cl.worldmodel->nodes);
	cl_efrags[i] = VEC_SIZE (cl_efrags) - i - 1; // write actual count
	cl.num_efrags += cl_efrags[i];

	R_CheckEfrags (); //johnfitz
}

/*
===============
R_AddStaticModels
===============
*/
void R_AddStaticModels (const byte *vis)
{
	int			i, j, leafidx, numleafs, *efrags;
	entity_t	*ent;

	for (i = 0, ent = cl_static_entities, efrags = cl_efrags; i < cl.num_statics; i++, ent++)
	{
		if (!ent->model)
			continue;
		for (j = 0, numleafs = *efrags++; j < numleafs; j++)
		{
			leafidx = efrags[j];
			if ((vis[leafidx >> 3] & (1 << (leafidx & 7))))
			{
				if (cl_numvisedicts >= MAX_VISEDICTS)
					return;
				cl_visedicts[cl_numvisedicts++] = ent;
				break;
			}
		}
		efrags += numleafs;
	}
}
