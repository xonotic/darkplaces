/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "r_shadow.h"
#include "portals.h"
#include "csprogs.h"
#include "image.h"
#include "cl_collision.h"

cvar_t r_ambient = {CF_CLIENT, "r_ambient", "0", "brightens map, value is 0-128"};
cvar_t r_lockpvs = {CF_CLIENT, "r_lockpvs", "0", "disables pvs switching, allows you to walk around and inspect what is visible from a given location in the map (anything not visible from your current location will not be drawn)"};
cvar_t r_lockvisibility = {CF_CLIENT, "r_lockvisibility", "0", "disables visibility updates, allows you to walk around and inspect what is visible from a given viewpoint in the map (anything offscreen at the moment this is enabled will not be drawn)"};
cvar_t r_useportalculling = {CF_CLIENT, "r_useportalculling", "1", "improve framerate with r_novis 1 by using portal culling - still not as good as compiled visibility data in the map, but it helps (a value of 2 forces use of this even with vis data, which improves framerates in maps without too much complexity, but hurts in extremely complex maps, which is why 2 is not the default mode)"};
cvar_t r_usesurfaceculling = {CF_CLIENT, "r_usesurfaceculling", "1", "skip off-screen surfaces (1 = cull surfaces if the map is likely to benefit, 2 = always cull surfaces)"};
cvar_t r_vis_debug = {CF_CLIENT, "r_vis_debug", "0", "show debug lines for traces and portals and other stuff"};
cvar_t r_vis_debug_linewidth = {CF_CLIENT, "r_vis_debug_linewidth", "0.125", "width of debug lines in world space"};
cvar_t r_vis_debug_nodepthtest = {CF_CLIENT, "r_vis_debug_nodepthtest", "1", "whether debug lines should be visible behind geometry"};
cvar_t r_vis_minimum_radius = {CF_CLIENT, "r_vis_minimum_radius", "16", "minimum visible distance"};
cvar_t r_vis_maximum_radius = {CF_CLIENT, "r_vis_maximum_radius", "1000000000", "maximum visible distance - should be a big number generally"};
cvar_t r_vis_scatter_radius = {CF_CLIENT, "r_vis_scatter_radius", "256", "for raytraced visibility methods, pretend the eye is this large by scattering rays around the eye location"};
cvar_t r_vis_bouncevis = {CF_CLIENT, "r_vis_bouncevis", "0", "use a stochastic raytraced method to determine the visible part of the world in realtime (supports dynamic occluders, transparent water, etc)"};
cvar_t r_vis_bouncevis_new_rays_per_frame = {CF_CLIENT, "r_vis_bouncevis_new_rays_per_frame", "1024", "number of new rays to cast each frame (in addition to repeating some retained rays from the previous frame)"};
cvar_t r_vis_bouncevis_use_frustum = {CF_CLIENT, "r_vis_bouncevis_use_frustum", "0", "generate rays within the view frustum only, not random spherical directions"};
cvar_t r_vis_bouncevis_bounces = {CF_CLIENT, "r_vis_bouncevis_bounces", "3", "number of times a ray can bounce, makes visible area more generous and could support hardware raytracing where knowing the scene bounds including reflections is important"};
cvar_t r_vis_bouncevis_pad = {CF_CLIENT, "r_vis_bouncevis_pad", "32", "expand the discovered visible area by this much so that walls do not flicker, but should be less than half the thickness of walls meant to occlude other rooms to avoid a lot of overdraw"};
cvar_t r_vis_bouncevis_clip_scattered_ray_origin = {CF_CLIENT, "r_vis_bouncevis_clip_scattered_ray_origin", "1", "for raytraced visibility methods, trace an additional ray to the intended ray starting point to avoid starting rays inside walls or on the other side of a wall" };
cvar_t r_vis_bouncevis_movement_prediction_time = {CF_CLIENT, "r_vis_bouncevis_movement_prediction_time", "0.05", "for raytraced visibility methods, fire an additional ray from a position the camera is moving towards"};
cvar_t r_vis_bouncevis_fibonacci_spiral = { CF_CLIENT, "r_vis_bouncevis_fibonacci_spiral", "1", "rather than random distribution use the fibonacci spiral on a sphere to uniformly distribute rays, rotated randomly every frame"};
cvar_t r_vis_trace = {CF_CLIENT, "r_vis_trace", "0", "test if each portal or leaf is visible using tracelines"};
cvar_t r_vis_trace_samples = {CF_CLIENT, "r_vis_trace_samples", "1", "use this many randomly positioned tracelines each frame to refresh the visible timer"};
cvar_t r_vis_trace_delay = {CF_CLIENT, "r_vis_trace_delay", "1", "keep a portal visible for this many seconds"};
cvar_t r_vis_trace_enlarge = {CF_CLIENT, "r_vis_trace_enlarge", "0", "make portal bounds bigger for tests by (1+this)*size"};
cvar_t r_vis_trace_expand = {CF_CLIENT, "r_vis_trace_expand", "0", "make portal bounds bigger for tests by this many units"};
cvar_t r_vis_trace_pad = {CF_CLIENT, "r_vis_trace_pad", "8", "accept traces that hit within this many units of the portal"};
cvar_t r_vis_trace_surfaces = {CF_CLIENT, "r_vis_trace_surfaces", "0", "also use tracelines to cull surfaces"};
cvar_t r_q3bsp_renderskydepth = {CF_CLIENT, "r_q3bsp_renderskydepth", "0", "draws sky depth masking in q3 maps (as in q1 maps), this means for example that sky polygons can hide other things"};

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (const entity_render_t *ent, msurface_t *surface, int combine)
{
	int smax, tmax, i, size, size3, maps, l;
	int *bl, scale;
	unsigned char *lightmap, *out, *stain;
	model_t *model = ent->model;
	int *intblocklights;
	unsigned char *templight;

	smax = (surface->lightmapinfo->extents[0]>>4)+1;
	tmax = (surface->lightmapinfo->extents[1]>>4)+1;
	size = smax*tmax;
	size3 = size*3;

	r_refdef.stats[r_stat_lightmapupdatepixels] += size;
	r_refdef.stats[r_stat_lightmapupdates]++;

	if (cl.buildlightmapmemorysize < size*sizeof(int[3]))
	{
		cl.buildlightmapmemorysize = size*sizeof(int[3]);
		if (cl.buildlightmapmemory)
			Mem_Free(cl.buildlightmapmemory);
		cl.buildlightmapmemory = (unsigned char *) Mem_Alloc(cls.levelmempool, cl.buildlightmapmemorysize);
	}

	// these both point at the same buffer, templight is only used for final
	// processing and can replace the intblocklights data as it goes
	intblocklights = (int *)cl.buildlightmapmemory;
	templight = (unsigned char *)cl.buildlightmapmemory;

	// update cached lighting info
	model->brushq1.lightmapupdateflags[surface - model->data_surfaces] = false;

	lightmap = surface->lightmapinfo->samples;

// set to full bright if no light data
	bl = intblocklights;
	if (!model->brushq1.lightdata)
	{
		for (i = 0;i < size3;i++)
			bl[i] = 128*256;
	}
	else
	{
// clear to no light
		memset(bl, 0, size3*sizeof(*bl));

// add all the lightmaps
		if (lightmap)
			for (maps = 0;maps < MAXLIGHTMAPS && surface->lightmapinfo->styles[maps] != 255;maps++, lightmap += size3)
				for (scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[maps]], i = 0;i < size3;i++)
					bl[i] += lightmap[i] * scale;
	}

	stain = surface->lightmapinfo->stainsamples;
	bl = intblocklights;
	out = templight;
	// the >> 16 shift adjusts down 8 bits to account for the stainmap
	// scaling, and remaps the 0-65536 (2x overbright) to 0-256, it will
	// be doubled during rendering to achieve 2x overbright
	// (0 = 0.0, 128 = 1.0, 256 = 2.0)
	if (stain)
	{
		for (i = 0;i < size;i++, bl += 3, stain += 3, out += 4)
		{
			l = (bl[0] * stain[0]) >> 16;out[2] = min(l, 255);
			l = (bl[1] * stain[1]) >> 16;out[1] = min(l, 255);
			l = (bl[2] * stain[2]) >> 16;out[0] = min(l, 255);
			out[3] = 255;
		}
	}
	else
	{
		for (i = 0;i < size;i++, bl += 3, out += 4)
		{
			l = bl[0] >> 8;out[2] = min(l, 255);
			l = bl[1] >> 8;out[1] = min(l, 255);
			l = bl[2] >> 8;out[0] = min(l, 255);
			out[3] = 255;
		}
	}

	if(vid_sRGB.integer && vid_sRGB_fallback.integer && !vid.sRGB3D)
		Image_MakesRGBColorsFromLinear_Lightmap(templight, templight, size);
	R_UpdateTexture(surface->lightmaptexture, templight, surface->lightmapinfo->lightmaporigin[0], surface->lightmapinfo->lightmaporigin[1], 0, smax, tmax, 1, combine);

	// update the surface's deluxemap if it has one
	if (surface->deluxemaptexture != r_texture_blanknormalmap)
	{
		vec3_t n;
		unsigned char *normalmap = surface->lightmapinfo->nmapsamples;
		lightmap = surface->lightmapinfo->samples;
		// clear to no normalmap
		bl = intblocklights;
		memset(bl, 0, size3*sizeof(*bl));
		// add all the normalmaps
		if (lightmap && normalmap)
		{
			for (maps = 0;maps < MAXLIGHTMAPS && surface->lightmapinfo->styles[maps] != 255;maps++, lightmap += size3, normalmap += size3)
			{
				for (scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[maps]], i = 0;i < size;i++)
				{
					// add the normalmap with weighting proportional to the style's lightmap intensity
					l = (int)(VectorLength(lightmap + i*3) * scale);
					bl[i*3+0] += ((int)normalmap[i*3+0] - 128) * l;
					bl[i*3+1] += ((int)normalmap[i*3+1] - 128) * l;
					bl[i*3+2] += ((int)normalmap[i*3+2] - 128) * l;
				}
			}
		}
		bl = intblocklights;
		out = templight;
		// we simply renormalize the weighted normals to get a valid deluxemap
		for (i = 0;i < size;i++, bl += 3, out += 4)
		{
			VectorCopy(bl, n);
			VectorNormalize(n);
			l = (int)(n[0] * 128 + 128);out[2] = bound(0, l, 255);
			l = (int)(n[1] * 128 + 128);out[1] = bound(0, l, 255);
			l = (int)(n[2] * 128 + 128);out[0] = bound(0, l, 255);
			out[3] = 255;
		}
		R_UpdateTexture(surface->deluxemaptexture, templight, surface->lightmapinfo->lightmaporigin[0], surface->lightmapinfo->lightmaporigin[1], 0, smax, tmax, 1, r_q1bsp_lightmap_updates_combine.integer);
	}
}

static void R_StainNode (mnode_t *node, model_t *model, const vec3_t origin, float radius, const float fcolor[8])
{
	float ndist, a, ratio, maxdist, maxdist2, maxdist3, invradius, sdtable[256], td, dist2;
	msurface_t *surface, *endsurface;
	int i, s, t, smax, tmax, smax3, impacts, impactt, stained;
	unsigned char *bl;
	vec3_t impact;

	maxdist = radius * radius;
	invradius = 1.0f / radius;

loc0:
	if (!node->plane)
		return;
	ndist = PlaneDiff(origin, node->plane);
	if (ndist > radius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (ndist < -radius)
	{
		node = node->children[1];
		goto loc0;
	}

	dist2 = ndist * ndist;
	maxdist3 = maxdist - dist2;

	if (node->plane->type < 3)
	{
		VectorCopy(origin, impact);
		impact[node->plane->type] -= ndist;
	}
	else
	{
		impact[0] = origin[0] - node->plane->normal[0] * ndist;
		impact[1] = origin[1] - node->plane->normal[1] * ndist;
		impact[2] = origin[2] - node->plane->normal[2] * ndist;
	}

	for (surface = model->data_surfaces + node->firstsurface, endsurface = surface + node->numsurfaces;surface < endsurface;surface++)
	{
		if (surface->lightmapinfo->stainsamples)
		{
			smax = (surface->lightmapinfo->extents[0] >> 4) + 1;
			tmax = (surface->lightmapinfo->extents[1] >> 4) + 1;

			impacts = (int)(DotProduct (impact, surface->lightmapinfo->texinfo->vecs[0]) + surface->lightmapinfo->texinfo->vecs[0][3] - surface->lightmapinfo->texturemins[0]);
			impactt = (int)(DotProduct (impact, surface->lightmapinfo->texinfo->vecs[1]) + surface->lightmapinfo->texinfo->vecs[1][3] - surface->lightmapinfo->texturemins[1]);

			s = bound(0, impacts, smax * 16) - impacts;
			t = bound(0, impactt, tmax * 16) - impactt;
			i = (int)(s * s + t * t + dist2);
			if ((i > maxdist) || (smax > (int)(sizeof(sdtable)/sizeof(sdtable[0])))) // smax overflow fix from Andreas Dehmel
				continue;

			// reduce calculations
			for (s = 0, i = impacts; s < smax; s++, i -= 16)
				sdtable[s] = i * i + dist2;

			bl = surface->lightmapinfo->stainsamples;
			smax3 = smax * 3;
			stained = false;

			i = impactt;
			for (t = 0;t < tmax;t++, i -= 16)
			{
				td = i * i;
				// make sure some part of it is visible on this line
				if (td < maxdist3)
				{
					maxdist2 = maxdist - td;
					for (s = 0;s < smax;s++)
					{
						if (sdtable[s] < maxdist2)
						{
							ratio = lhrandom(0.0f, 1.0f);
							a = (fcolor[3] + ratio * fcolor[7]) * (1.0f - sqrt(sdtable[s] + td) * invradius);
							if (a >= (1.0f / 64.0f))
							{
								if (a > 1)
									a = 1;
								bl[0] = (unsigned char) ((float) bl[0] + a * ((fcolor[0] + ratio * fcolor[4]) - (float) bl[0]));
								bl[1] = (unsigned char) ((float) bl[1] + a * ((fcolor[1] + ratio * fcolor[5]) - (float) bl[1]));
								bl[2] = (unsigned char) ((float) bl[2] + a * ((fcolor[2] + ratio * fcolor[6]) - (float) bl[2]));
								stained = true;
							}
						}
						bl += 3;
					}
				}
				else // skip line
					bl += smax3;
			}
			// force lightmap upload
			if (stained)
				model->brushq1.lightmapupdateflags[surface - model->data_surfaces] = true;
		}
	}

	if (node->children[0]->plane)
	{
		if (node->children[1]->plane)
		{
			R_StainNode(node->children[0], model, origin, radius, fcolor);
			node = node->children[1];
			goto loc0;
		}
		else
		{
			node = node->children[0];
			goto loc0;
		}
	}
	else if (node->children[1]->plane)
	{
		node = node->children[1];
		goto loc0;
	}
}

void R_Stain (const vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2)
{
	int n;
	float fcolor[8];
	entity_render_t *ent;
	model_t *model;
	vec3_t org;
	if (r_refdef.scene.worldmodel == NULL || !r_refdef.scene.worldmodel->brush.data_nodes || !r_refdef.scene.worldmodel->brushq1.lightdata)
		return;
	fcolor[0] = cr1;
	fcolor[1] = cg1;
	fcolor[2] = cb1;
	fcolor[3] = ca1 * (1.0f / 64.0f);
	fcolor[4] = cr2 - cr1;
	fcolor[5] = cg2 - cg1;
	fcolor[6] = cb2 - cb1;
	fcolor[7] = (ca2 - ca1) * (1.0f / 64.0f);

	R_StainNode(r_refdef.scene.worldmodel->brush.data_nodes + r_refdef.scene.worldmodel->brushq1.hulls[0].firstclipnode, r_refdef.scene.worldmodel, origin, radius, fcolor);

	// look for embedded bmodels
	for (n = 0;n < cl.num_brushmodel_entities;n++)
	{
		ent = &cl.entities[cl.brushmodel_entities[n]].render;
		model = ent->model;
		if (model && model->name[0] == '*')
		{
			if (model->brush.data_nodes)
			{
				Matrix4x4_Transform(&ent->inversematrix, origin, org);
				R_StainNode(model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode, model, org, radius, fcolor);
			}
		}
	}
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

static void R_DrawPortal_Callback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	// due to the hacky nature of this function's parameters, this is never
	// called with a batch, so numsurfaces is always 1, and the surfacelist
	// contains only a leaf number for coloring purposes
	const mportal_t *portal = (mportal_t *)ent;
	qbool isvis;
	int i, numpoints;
	float *v;
	float vertex3f[POLYGONELEMENTS_MAXPOINTS*3];
	CHECKGLERROR
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	GL_DepthTest(true);
	GL_CullFace(GL_NONE);
	R_EntityMatrix(&identitymatrix);

	numpoints = min(portal->numpoints, POLYGONELEMENTS_MAXPOINTS);

//	R_Mesh_ResetTextureState();

	isvis = (portal->here->clusterindex >= 0 && portal->past->clusterindex >= 0 && portal->here->clusterindex != portal->past->clusterindex);

	i = surfacelist[0] >> 1;
	GL_Color(((i & 0x0007) >> 0) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			 ((i & 0x0038) >> 3) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			 ((i & 0x01C0) >> 6) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			 isvis ? 0.125f : 0.03125f);
	for (i = 0, v = vertex3f;i < numpoints;i++, v += 3)
		VectorCopy(portal->points[i].position, v);
	R_Mesh_PrepareVertices_Generic_Arrays(numpoints, vertex3f, NULL, NULL);
	R_SetupShader_Generic_NoTexture(false, false);
	R_Mesh_Draw(0, numpoints, 0, numpoints - 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
}

// LadyHavoc: this is just a nice debugging tool, very slow
void R_DrawPortals(void)
{
	int i, leafnum;
	mportal_t *portal;
	float center[3], f;
	model_t *model = r_refdef.scene.worldmodel;
	if (model == NULL)
		return;
	for (leafnum = 0;leafnum < model->brush.num_leafs;leafnum++)
	{
		if (r_refdef.viewcache.world_leafvisible[leafnum])
		{
			//for (portalnum = 0, portal = model->brush.data_portals;portalnum < model->brush.num_portals;portalnum++, portal++)
			for (portal = model->brush.data_leafs[leafnum].portals;portal;portal = portal->next)
			{
				if (portal->numpoints <= POLYGONELEMENTS_MAXPOINTS)
				if (!R_CullFrustum(portal->mins, portal->maxs))
				{
					VectorClear(center);
					for (i = 0;i < portal->numpoints;i++)
						VectorAdd(center, portal->points[i].position, center);
					f = ixtable[portal->numpoints];
					VectorScale(center, f, center);
					R_MeshQueue_AddTransparent(TRANSPARENTSORT_DISTANCE, center, R_DrawPortal_Callback, (entity_render_t *)portal, leafnum, rsurface.rtlight);
				}
			}
		}
	}
}

static void R_View_WorldVisibility_CullSurfaces(void)
{
	int surfaceindex;
	unsigned char *surfacevisible;
	msurface_t *surfaces;
	model_t *model = r_refdef.scene.worldmodel;
	vec3_t vismins;
	vec3_t vismaxs;
	if (!model)
		return;
	if (r_trippy.integer)
		return;
	if (r_usesurfaceculling.integer < 1)
		return;
	VectorCopy(r_refdef.viewcache.vismins, vismins);
	VectorCopy(r_refdef.viewcache.vismaxs, vismaxs);
	surfaces = model->data_surfaces;
	surfacevisible = r_refdef.viewcache.world_surfacevisible;
	for (surfaceindex = model->submodelsurfaces_start; surfaceindex < model->submodelsurfaces_end; surfaceindex++)
	{
		if (surfacevisible[surfaceindex])
		{
			if (!BoxesOverlap(vismins, vismaxs, surfaces[surfaceindex].mins, surfaces[surfaceindex].maxs)
			 || R_CullFrustum(surfaces[surfaceindex].mins, surfaces[surfaceindex].maxs))
				surfacevisible[surfaceindex] = 0;
		}
	}
}

void R_View_WorldVisibility_NewFrame(void)
{
	r_refdef.viewcache.bouncevis.currentframe ^= 1;
	r_refdef.viewcache.bouncevis.frames[r_refdef.viewcache.bouncevis.currentframe].numrays = 0;
}

#define VISIBILITY_CELLS 2
void R_BounceVis(unsigned int newrays, unsigned int bounces, vec3_t actualorigin, vec_t scatter, vec_t minradius, vec_t maxradius, vec3_t scenemins, vec3_t scenemaxs, int skipmaterialflagsmask, qbool ortho, qbool frustum)
{
	bouncevis_frame_t previous = r_refdef.viewcache.bouncevis.frames[r_refdef.viewcache.bouncevis.currentframe ^ 1];
	bouncevis_frame_t current = r_refdef.viewcache.bouncevis.frames[r_refdef.viewcache.bouncevis.currentframe];
	unsigned int totalrays = previous.numrays + newrays;
	unsigned int cells;
	vec3_t startpos;
	vec3_t endpos;
	vec3_t unsnappedorigin[VISIBILITY_CELLS];
	vec3_t vieworigin[VISIBILITY_CELLS];
	vec3_t vismins[VISIBILITY_CELLS];
	vec3_t vismaxs[VISIBILITY_CELLS];
	vec3_t origin;
	vec3_t offset;
	vec_t bestdist2;
	vec_t base_y;
	unsigned int i;
	unsigned int c;
	unsigned int bestc;
	unsigned int bounce;
	unsigned int q;
	float pad = bound(0.0f, r_vis_bouncevis_pad.value, 1000000000.0f);
	qbool keep;
	randomseed_t seed = { 0 };
	Math_RandomSeed_FromInts(&seed, actualorigin[0], actualorigin[1], actualorigin[2], r_textureframe + 1);
	if (current.maxrays < current.numrays + totalrays)
	{
		current.maxrays = current.numrays + totalrays + 256;
		current.rays = Mem_Realloc(r_main_mempool, current.rays, current.maxrays * sizeof(current.rays[0]));
	}
	cells = 0;
	// We start with the view origin, this is always in slot 0
	VectorCopy(actualorigin, unsnappedorigin[cells]);
	// Snap the vieworigin to improve convergence of the AABB fit (if the view
	// origin changes too frequently, persistent rays are rejected more often)
	VectorCopy(unsnappedorigin[cells], vieworigin[cells]);
	AABBFrom(vismins[cells], vismaxs[cells], vieworigin[cells], vieworigin[cells]);
	cells++;
	// If extrapolated movement is enabled, we need to figure out the projected
	// position and whether it is reachable from the actual origin, we will be
	// updating more than one AABB if it is enabled, so that we begin retaining
	// rays and converging on a good answer before the view gets there
	if (r_vis_bouncevis_movement_prediction_time.value > 0.0f)
	{
		vec3_t e;
		trace_t trace;
		VectorMA(actualorigin, r_vis_bouncevis_movement_prediction_time.value, r_refdef.view.velocity, e);
		// Check how far along the extrapolation vector we can go without ending up in solid
		trace = CL_Cache_TraceLineSurfaces(actualorigin, e, MOVE_NORMAL, SUPERCONTENTS_SOLID, 0, skipmaterialflagsmask);
		VectorCopy(trace.endpos, unsnappedorigin[cells]);
		VectorCopy(unsnappedorigin[cells], vieworigin[cells]);
		AABBFrom(vismins[cells], vismaxs[cells], vieworigin[cells], vieworigin[cells]);
		if (VectorDistance2(vieworigin[0], vieworigin[cells]) >= 1.0f)
			cells++;
	}

	// Here we can add any other view origins we need, currently no others implemented

	// To pick mostly-evenly distributed points on the sphere we use a fibonacci
	// spiral, and by starting with a different starting Y coordinate (-1..1)
	// each frame we get different points, using PI here arbtrarily just because
	// it is a long non-repeating sequencve, so functionally like an RNG for
	// this purpose
	base_y = r_textureframe * M_PI;
	base_y -= floor(base_y);

	for (i = 0; i < totalrays; i++)
	{
		bouncevis_ray_t ray;
		vec3_t vispoint;
		if (i < previous.numrays)
		{
			// Enumerate the rays from the previous frame in reverse order,
			// this reduces the number of supporting rays we find, otherwise
			// we would be replaying the entire same journey of improvements
			// every frame, we want only the ones that did the most to expand
			// the vis bounds, and the last ones are the furthest
			ray = previous.rays[previous.numrays - 1 - i];
			// If rendering an ortho view, always use the current view direction
			if (ortho)
				VectorCopy(r_refdef.view.frustum[4].normal, ray.direction);
		}
		else
		{
			// Cycle through the view origins
			c = i % cells;
			VectorCopy(vieworigin[c], origin);
			if (!ortho)
			{
				// Cast rays from the camera origin in all directions, this works
				// even with r_refdef.view.usevieworiginculling == false because
				// tracing rays works even with a solid starting point
				//
				// Much like in bouncegrid we need high quality random to ensure a
				// good distribution, otherwise there may be gaps
				VectorLehmerRandom(&seed, ray.origin);
				ForVec(q) ray.origin[q] = origin[q] + scatter * ray.origin[q];
				if (r_vis_bouncevis_fibonacci_spiral.integer)
				{
					vec_t radius, theta, y;
					// We want y to step -1..1 over the number of rays being cast, and
					// wrap within the range -1..1
					y = base_y + i * (1.0f / newrays);
					y = (y - floor(y)) * 2.0f - 1.0f;
					radius = sqrt(1.0 - y * y);
					// We need theta to also advance across frames so base it on base_y,
					// and make sure it wraps in the -M_PI..M_PI range because on Intel
					// CPUs the trig functions have lower precision with larger angle
					// values as they don't wrap the value before computing the factors
					theta = base_y * 16.0f + M_PI * (3.0 - sqrt(5.0)) * i;
					theta -= (M_PI * 2) * trunc(theta * (1.0f / (M_PI * 2)));
					ray.direction[0] = cos(theta) * radius;
					ray.direction[1] = sin(theta) * radius;
					ray.direction[2] = y;
				}
				else if (frustum)
				{
					// Pick a direction that is within the frustum
					float l = Math_randomf(&seed);
					float u = Math_randomf(&seed);
					vec3_t h1;
					vec3_t h2;
					VectorLerp(r_refdef.view.frustumcorner[0], l, r_refdef.view.frustumcorner[1], h1);
					VectorLerp(r_refdef.view.frustumcorner[2], l, r_refdef.view.frustumcorner[3], h2);
					VectorLerp(h1, u, h2, endpos);
					VectorSubtract(endpos, ray.origin, ray.direction);
				}
				else 
				{
					VectorLehmerRandom(&seed, ray.direction);
				}
				VectorNormalize(ray.direction);
			}
			else
			{
				// Cast rays from the view origin along the forward vector, this
				// often involves rays starting in solid
				//
				// Fire forward rays at random offsets within the XY plane
				// Trace from maxradius*-0.5 to maxradius*0.5 relative to vieworigin
				float l = Math_crandomf(&seed) * r_refdef.view.ortho_x;
				float u = Math_crandomf(&seed) * r_refdef.view.ortho_y;
				ForVec(q) ray.origin[q] = origin[q] + l * r_refdef.view.frustum[0].normal[q] + u * r_refdef.view.frustum[2].normal[q] + maxradius * -0.5f * r_refdef.view.frustum[4].normal[q];
				// Always use the current view direction
				VectorCopy(r_refdef.view.frustum[4].normal, ray.direction);
			}
		}

		// Figure out which cell the ray is closer to, it will only be
		// contributing to that cell's AABB, this way we can be converging on an
		// optimal fit for that cell in advance of the view moving there.
		bestc = 0;
		bestdist2 = VectorDistance2(ray.origin, vieworigin[bestc]);
		for (c = 1; c < cells; c++)
		{
			vec_t dist2 = VectorDistance2(ray.origin, vieworigin[c]);
			if (bestdist2 > dist2)
			{
				bestc = c;
				bestdist2 = dist2;
			}
		}
		// We picked this one
		VectorCopy(vieworigin[bestc], origin);
		VectorSubtract(origin, actualorigin, offset);

		// Skip if the ray origin is too far from origin, note that
		// this can handle multiple scene renders per frame with different
		// vieworigin
		//
		// We have to apply this check even if this is a new ray, because if we
		// generated a ray that would not fit the test for next frame, it would
		// be replacing a better ray we already have
		if (bestdist2 > scatter * scatter)
			continue;
		// If we're using frustum-aligned rays, skip any that are outside
		// the frustum
		if (frustum)
		{
			vec3_t adjusted;
			VectorMAMAM(1.0f, ray.origin, maxradius, ray.direction, -1.0f, offset, adjusted);
			if (R_CullFrustum(adjusted, adjusted))
				continue;
		}
		// First check if we can reach the starting point from the vieworigin
		// and start the actual rays from the endpos
		if (r_vis_bouncevis_clip_scattered_ray_origin.integer)
		{
			trace_t trace = CL_Cache_TraceLineSurfaces(origin, ray.origin, MOVE_NORMAL, SUPERCONTENTS_SOLID, 0, skipmaterialflagsmask);
			VectorCopy(trace.endpos, startpos);
		}
		else
			VectorCopy(ray.origin, startpos);
		ForVec(q) endpos[q] = ray.origin[q] + maxradius * ray.direction[q];
		keep = false;
//		CL_ParticleTrail(EFFECT_TR_NEXUIZPLASMA, 0.1f, vieworigin, startpos, vec3_origin, vec3_origin, NULL, 239, false, true, NULL, NULL, 1);
		VectorSnap(startpos, 0.125f, 0.125f, 0.125f, vispoint);
		AABBInsertAndSetKeep(vismins[bestc], vismaxs[bestc], vispoint, vispoint, keep);
		for (bounce = 0; bounce <= bounces; bounce++)
		{
			trace_t trace = CL_Cache_TraceLineSurfaces(startpos, endpos, MOVE_NORMAL, SUPERCONTENTS_SOLID, 0, skipmaterialflagsmask);
//			CL_ParticleTrail(EFFECT_TR_NEXUIZPLASMA, 0.1f, startpos, endpos, vec3_origin, vec3_origin, NULL, 239, false, true, NULL, NULL, 1);

			// Update the visible AABB using a snapped version of the trace,
			// this reduces the odds of setting keep=true for miniscule
			// increments that may round off to the same coordinates
			// (particularly because traces are double precision unlike AABB)
			VectorSnap(trace.endpos, 0.125f, 0.125f, 0.125f, vispoint);
			AABBInsertAndSetKeep(vismins[bestc], vismaxs[bestc], vispoint, vispoint, keep);
			if (r_vis_debug.value > 0.0f)
			{
				if (keep)
				{
					// Illustrate the ray itself
					R_DebugLine2(startpos[0], startpos[1], startpos[2], trace.endpos[0], trace.endpos[1], trace.endpos[2], r_vis_debug_linewidth.value, bounce * 0.1f, 0.1f, 0.0f, r_vis_debug.value, bounce * 0.1f, 0.5f, 0.0f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
					// Illustrate the new AABB
					//R_DebugBox(vismins[bestc][0], vismins[bestc][1], vismins[bestc][2], vismaxs[bestc][0], vismaxs[bestc][1], vismaxs[bestc][2], r_vis_debug_linewidth.value, 0.1f, 0.5f, 0.1f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
				}
				else
				{
					//R_DebugLine2(startpos[0], startpos[1], startpos[2], trace.endpos[0], trace.endpos[1], trace.endpos[2], r_vis_debug_linewidth.value, bounce * 0.1f, 0.0f, 0.1f, r_vis_debug.value, bounce * 0.1f, 0.0f, 0.5f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
				}
			}

			// If the ray flew off way out of bounds or hit sky there are no
			// further bounces to do
			if (trace.fraction == 1.0f || !!(trace.hitsupercontents & SUPERCONTENTS_SKY))
				break;

			// reflect off the surface
			vec3_t remaining;
			vec_t reflect;
			ForVec(q) remaining[q] = (endpos[q] - startpos[q]) * (1.0f - trace.fraction);
			reflect = -2.0f * DotProduct(remaining, trace.plane.normal);
			ForVec(q) { startpos[q] = trace.endpos[q]; endpos[q] = trace.endpos[q] + reflect * trace.plane.normal[q]; }
		}
		if (keep)
		{
			current.rays[current.numrays++] = ray;
		}
	}

	// We have to make sure that the visible area includes the actual view origin as it was generated for a snapped view origin
	ForVec(q) scenemins[q] = min(vismins[0][q] - pad, actualorigin[q] - minradius), scenemaxs[q] = max(vismaxs[0][q] + pad, actualorigin[q] + minradius);

	if (r_vis_debug.value > 0.0f)
	{
		// Illustrate the scene AABB
		R_DebugBox(vismins[0][0], vismins[0][1], vismins[0][2], vismaxs[0][0], vismaxs[0][1], vismaxs[0][2], r_vis_debug_linewidth.value, 0.1f, 1.0f, 0.1f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
	}

	// store the updated bouncevis frame info
	r_refdef.viewcache.bouncevis.frames[r_refdef.viewcache.bouncevis.currentframe] = current;
}

void R_View_WorldVisibility(qbool forcenovis)
{
	int i, j, *mark, q;
	mleaf_t *leaf;
	mleaf_t *viewleaf;
	model_t *model = r_refdef.scene.worldmodel;
	vec3_t vieworigin, vismins, vismaxs;
	vec_t minradius = bound(0.0f, r_vis_minimum_radius.value, 1000000000.0f);
	vec_t maxradius = bound(r_vis_minimum_radius.value, r_vis_maximum_radius.value, 1000000000.0f);

	if (!model)
		return;

	if (r_lockvisibility.integer)
		return;

	// clear the visible surface and leaf flags arrays
	memset(r_refdef.viewcache.world_surfacevisible, 0, model->num_surfaces);
	if(!r_lockpvs.integer)
		memset(r_refdef.viewcache.world_leafvisible, 0, model->brush.num_leafs);

	VectorCopy(r_refdef.view.origin, vieworigin);
	// Start with an inverted box, we'll make sure it includes minradius later
	ForVec(q) vismins[q] = vieworigin[q] + maxradius, vismaxs[q] = vieworigin[q] - maxradius;

	r_refdef.viewcache.world_novis = false;

	if (r_vis_bouncevis.integer)
	{
		unsigned int newrays = bound(1, r_vis_bouncevis_new_rays_per_frame.integer, 1000000);
		unsigned int bounces = bound(0, r_vis_bouncevis_bounces.integer, 100);
		vec_t scatter = bound(minradius, r_vis_scatter_radius.value, maxradius);
		int skipmaterialflagsmask = MATERIALFLAG_NODRAW | MATERIALFLAG_ALPHATEST | MATERIALFLAG_BLENDED;
		qbool ortho = !r_refdef.view.useperspective;
		qbool frustum = r_vis_bouncevis_use_frustum.integer != 0;
		R_BounceVis(newrays, bounces, vieworigin, scatter, minradius, maxradius, vismins, vismaxs, skipmaterialflagsmask, ortho, frustum);

		// Now that we have an AABB of the visible area, mark the leafs and surfaces
		if (model)
		{
			int surfaceindex;
			for (j = 0, leaf = model->brush.data_leafs; j < model->brush.num_leafs; j++, leaf++)
			{
				// if leaf is in current pvs and on the screen, mark its surfaces
				if (leaf->clusterindex >= 0 && BoxesOverlap(vismins, vismaxs, leaf->mins, leaf->maxs) && !R_CullFrustum(leaf->mins, leaf->maxs))
				{
					r_refdef.stats[r_stat_world_leafs]++;
					r_refdef.viewcache.world_leafvisible[j] = true;
					//if (r_vis_debug.value > 0.0f)
					//	R_DebugBox(leaf->mins[0], leaf->mins[1], leaf->mins[2], leaf->maxs[0], leaf->maxs[1], leaf->maxs[2], r_vis_debug_linewidth.value, 0.3f, 0.0f, 0.3f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
				}
			}
			// This will be culled to visible area by R_View_WorldVisibility_CullSurfaces
			for (surfaceindex = model->submodelsurfaces_start; surfaceindex < model->submodelsurfaces_end; surfaceindex++)
				r_refdef.viewcache.world_surfacevisible[surfaceindex] = true;
		}
	}
	else if (r_refdef.view.usecustompvs)
	{
		// simply cull each marked leaf to the frustum (view pyramid)
		for (j = 0, leaf = model->brush.data_leafs;j < model->brush.num_leafs;j++, leaf++)
		{
			// if leaf is in current pvs and on the screen, mark its surfaces
			if (CHECKPVSBIT(r_refdef.viewcache.world_pvsbits, leaf->clusterindex) && !R_CullFrustum(leaf->mins, leaf->maxs))
			{
				r_refdef.stats[r_stat_world_leafs]++;
				r_refdef.viewcache.world_leafvisible[j] = true;
				AABBInsert(vismins, vismaxs, leaf->mins, leaf->maxs);
				//if (r_vis_debug.value > 0.0f)
				//	R_DebugBox(leaf->mins[0], leaf->mins[1], leaf->mins[2], leaf->maxs[0], leaf->maxs[1], leaf->maxs[2], r_vis_debug_linewidth.value, 0.3f, 0.0f, 0.3f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
				if (leaf->numleafsurfaces)
					for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
						r_refdef.viewcache.world_surfacevisible[*mark] = true;
			}
		}
	}
	else
	{
		// if possible find the leaf the view origin is in
		viewleaf = model->brush.PointInLeaf ? model->brush.PointInLeaf(model, r_refdef.view.origin) : NULL;
		// if possible fetch the visible cluster bits
		if (!r_lockpvs.integer && model->brush.FatPVS)
			model->brush.FatPVS(model, r_refdef.view.origin, 2, &r_refdef.viewcache.world_pvsbits, r_main_mempool, false);

		// if floating around in the void (no pvs data available, and no
		// portals available), simply use all on-screen leafs.
		if (!viewleaf || viewleaf->clusterindex < 0 || forcenovis || !r_refdef.view.usevieworiginculling)
		{
			// no visibility method: (used when floating around in the void)
			// simply cull each leaf to the frustum (view pyramid)
			// similar to quake's RecursiveWorldNode but without cache misses
			r_refdef.viewcache.world_novis = true;
			for (j = 0, leaf = model->brush.data_leafs;j < model->brush.num_leafs;j++, leaf++)
			{
				if (leaf->clusterindex < 0)
					continue;
				// if leaf is in current pvs and on the screen, mark its surfaces
				if (!R_CullFrustum(leaf->mins, leaf->maxs))
				{
					r_refdef.stats[r_stat_world_leafs]++;
					r_refdef.viewcache.world_leafvisible[j] = true;
					AABBInsert(vismins, vismaxs, leaf->mins, leaf->maxs);
					if (r_vis_debug.value > 0.0f)
						R_DebugBox(leaf->mins[0], leaf->mins[1], leaf->mins[2], leaf->maxs[0], leaf->maxs[1], leaf->maxs[2], r_vis_debug_linewidth.value, 0.3f, 0.0f, 0.3f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
					if (leaf->numleafsurfaces)
						for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
							r_refdef.viewcache.world_surfacevisible[*mark] = true;
				}
			}
		}
		// just check if each leaf in the PVS is on screen
		// (unless portal culling is enabled)
		else if (!model->brush.data_portals || r_useportalculling.integer < 1 || (r_useportalculling.integer < 2 && !r_novis.integer))
		{
			// pvs method:
			// simply check if each leaf is in the Potentially Visible Set,
			// and cull to frustum (view pyramid)
			// similar to quake's RecursiveWorldNode but without cache misses
			for (j = 0, leaf = model->brush.data_leafs;j < model->brush.num_leafs;j++, leaf++)
			{
				if (leaf->clusterindex < 0)
					continue;
				// if leaf is in current pvs and on the screen, mark its surfaces
				if (CHECKPVSBIT(r_refdef.viewcache.world_pvsbits, leaf->clusterindex) && !R_CullFrustum(leaf->mins, leaf->maxs))
				{
					r_refdef.stats[r_stat_world_leafs]++;
					r_refdef.viewcache.world_leafvisible[j] = true;
					AABBInsert(vismins, vismaxs, leaf->mins, leaf->maxs);
					if (r_vis_debug.value > 0.0f)
						R_DebugBox(leaf->mins[0], leaf->mins[1], leaf->mins[2], leaf->maxs[0], leaf->maxs[1], leaf->maxs[2], r_vis_debug_linewidth.value, 0.3f, 0.0f, 0.3f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
					if (leaf->numleafsurfaces)
						for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
							r_refdef.viewcache.world_surfacevisible[*mark] = true;
				}
			}
		}
		// if desired use a recursive portal flow, culling each portal to
		// frustum and checking if the leaf the portal leads to is in the pvs
		else
		{
			int leafstackpos;
			mportal_t *p;
			mleaf_t *leafstack[8192];
			vec3_t cullmins, cullmaxs;
			float cullbias = r_nearclip.value * 2.0f; // the nearclip plane can easily end up culling portals in certain perfectly-aligned views, causing view blackouts
			// simple-frustum portal method:
			// follows portals leading outward from viewleaf, does not venture
			// offscreen or into leafs that are not visible, faster than
			// Quake's RecursiveWorldNode and vastly better in unvised maps,
			// often culls some surfaces that pvs alone would miss
			// (such as a room in pvs that is hidden behind a wall, but the
			//  passage leading to the room is off-screen)
			leafstack[0] = viewleaf;
			leafstackpos = 1;
			while (leafstackpos)
			{
				leaf = leafstack[--leafstackpos];
				if (r_refdef.viewcache.world_leafvisible[leaf - model->brush.data_leafs])
					continue;
				if (leaf->clusterindex < 0)
					continue;
				r_refdef.stats[r_stat_world_leafs]++;
				r_refdef.viewcache.world_leafvisible[leaf - model->brush.data_leafs] = true;
				// mark any surfaces bounding this leaf
				if (leaf->numleafsurfaces)
					for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
						r_refdef.viewcache.world_surfacevisible[*mark] = true;
				// follow portals into other leafs
				// the checks are:
				// the leaf has not been visited yet
				// and the leaf is visible in the pvs
				// the portal polygon's bounding box is on the screen
				for (p = leaf->portals;p;p = p->next)
				{
					r_refdef.stats[r_stat_world_portals]++;
					if (r_refdef.viewcache.world_leafvisible[p->past - model->brush.data_leafs])
						continue;
					if (!CHECKPVSBIT(r_refdef.viewcache.world_pvsbits, p->past->clusterindex))
						continue;
					cullmins[0] = p->mins[0] - cullbias;
					cullmins[1] = p->mins[1] - cullbias;
					cullmins[2] = p->mins[2] - cullbias;
					cullmaxs[0] = p->maxs[0] + cullbias;
					cullmaxs[1] = p->maxs[1] + cullbias;
					cullmaxs[2] = p->maxs[2] + cullbias;
					if (R_CullFrustum(cullmins, cullmaxs))
						continue;
					if (r_vis_trace.integer)
					{
						if (p->tracetime != host.realtime && R_CanSeeBox(r_vis_trace_samples.value, minradius, r_vis_trace_enlarge.value, r_vis_trace_expand.value, r_vis_trace_pad.value, r_refdef.view.origin, cullmins, cullmaxs))
							p->tracetime = host.realtime;
						if (host.realtime - p->tracetime > r_vis_trace_delay.value)
							continue;
					}
					if (r_vis_debug.value > 0.0f)
						R_DebugBox(cullmins[0], cullmins[1], cullmins[2], cullmaxs[0], cullmaxs[1], cullmaxs[2], r_vis_debug_linewidth.value, 0.3f, 0.0f, 0.3f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
					AABBInsert(vismins, vismaxs, leaf->mins, leaf->maxs);
					if (leafstackpos >= (int)(sizeof(leafstack) / sizeof(leafstack[0])))
						break;
					leafstack[leafstackpos++] = p->past;
				}
			}
		}
	}

	ForVec(q) vismins[q] = min(vismins[q], r_refdef.view.origin[q] - minradius), vismaxs[q] = max(vismaxs[q], r_refdef.view.origin[q] + minradius);
	VectorCopy(vismins, r_refdef.viewcache.vismins);
	VectorCopy(vismaxs, r_refdef.viewcache.vismaxs);

	// Cull the surfaces that are outside the vis box or frustum
	R_View_WorldVisibility_CullSurfaces();

	if (r_vis_debug.value > 0.0f)
	{
		// Illustrate the scene AABB
		R_DebugBox(r_refdef.viewcache.vismins[0], r_refdef.viewcache.vismins[1], r_refdef.viewcache.vismins[2], r_refdef.viewcache.vismaxs[0], r_refdef.viewcache.vismaxs[1], r_refdef.viewcache.vismaxs[2], r_vis_debug_linewidth.value, 0.1f, 1.0f, 0.1f, r_vis_debug.value, r_vis_debug_nodepthtest.integer != 0);
	}
}

void R_Mod_DrawSky(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	R_DrawModelSurfaces(ent, true, true, false, false, false, false);
}

void R_Mod_DrawAddWaterPlanes(entity_render_t *ent)
{
	int i, n, flagsmask;
	model_t *model = ent->model;
	msurface_t *surfaces;
	if (model == NULL)
		return;

	RSurf_ActiveModelEntity(ent, true, false, false);

	surfaces = model->data_surfaces;
	flagsmask = MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION | MATERIALFLAG_CAMERA;

	// add visible surfaces to draw list
	if (ent == r_refdef.scene.worldentity)
	{
		for (i = model->submodelsurfaces_start;i < model->submodelsurfaces_end;i++)
			if (r_refdef.viewcache.world_surfacevisible[i])
				if (surfaces[i].texture->basematerialflags & flagsmask)
					R_Water_AddWaterPlane(surfaces + i, 0);
	}
	else
	{
		if(ent->entitynumber >= MAX_EDICTS) // && CL_VM_TransformView(ent->entitynumber - MAX_EDICTS, NULL, NULL, NULL))
			n = ent->entitynumber;
		else
			n = 0;
		for (i = model->submodelsurfaces_start;i < model->submodelsurfaces_end;i++)
			if (surfaces[i].texture->basematerialflags & flagsmask)
				R_Water_AddWaterPlane(surfaces + i, n);
	}
	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveModelEntity
}

void R_Mod_Draw(entity_render_t *ent)
{
	model_t *model = ent->model;
	if (model == NULL)
		return;
	R_DrawModelSurfaces(ent, false, true, false, false, false, false);
}

void R_Mod_DrawDepth(entity_render_t *ent)
{
	model_t *model = ent->model;
	if (model == NULL || model->surfmesh.isanimated)
		return;
	GL_ColorMask(0,0,0,0);
	GL_Color(1,1,1,1);
	GL_DepthTest(true);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
//	R_Mesh_ResetTextureState();
	R_DrawModelSurfaces(ent, false, false, true, false, false, false);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
}

void R_Mod_DrawDebug(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	R_DrawModelSurfaces(ent, false, false, false, true, false, false);
}

void R_Mod_DrawPrepass(entity_render_t *ent)
{
	model_t *model = ent->model;
	if (model == NULL)
		return;
	R_DrawModelSurfaces(ent, false, true, false, false, true, false);
}

typedef struct r_q1bsp_getlightinfo_s
{
	model_t *model;
	vec3_t relativelightorigin;
	float lightradius;
	int *outleaflist;
	unsigned char *outleafpvs;
	int outnumleafs;
	unsigned char *visitingleafpvs;
	int *outsurfacelist;
	unsigned char *outsurfacepvs;
	unsigned char *tempsurfacepvs;
	unsigned char *outshadowtrispvs;
	unsigned char *outlighttrispvs;
	int outnumsurfaces;
	vec3_t outmins;
	vec3_t outmaxs;
	vec3_t lightmins;
	vec3_t lightmaxs;
	const unsigned char *pvs;
	qbool svbsp_active;
	qbool svbsp_insertoccluder;
	qbool noocclusion; // avoids PVS culling
	qbool frontsidecasting; // casts shadows from surfaces facing the light (otherwise ones facing away)
	int numfrustumplanes;
	const mplane_t *frustumplanes;
}
r_q1bsp_getlightinfo_t;

#define GETLIGHTINFO_MAXNODESTACK 4096

static void R_Q1BSP_RecursiveGetLightInfo_BSP(r_q1bsp_getlightinfo_t *info, qbool skipsurfaces)
{
	// nodestack
	mnode_t *nodestack[GETLIGHTINFO_MAXNODESTACK];
	int nodestackpos = 0;
	// node processing
	mplane_t *plane;
	mnode_t *node;
	int sides;
	// leaf processing
	mleaf_t *leaf;
	const msurface_t *surface;
	const msurface_t *surfaces = info->model->data_surfaces;
	int numleafsurfaces;
	int leafsurfaceindex;
	int surfaceindex;
	int triangleindex, t;
	int currentmaterialflags;
	qbool castshadow;
	const int *e;
	const vec_t *v[3];
	float v2[3][3];
	qbool insidebox;
	qbool noocclusion = info->noocclusion;
	qbool frontsidecasting = info->frontsidecasting;
	qbool svbspactive = info->svbsp_active;
	qbool svbspinsertoccluder = info->svbsp_insertoccluder;
	const int *leafsurfaceindices;
	qbool addedtris;
	int i;
	mportal_t *portal;
	static float points[128][3];
	// push the root node onto our nodestack
	nodestack[nodestackpos++] = info->model->brush.data_nodes;
	// we'll be done when the nodestack is empty
	while (nodestackpos)
	{
		// get a node from the stack to process
		node = nodestack[--nodestackpos];
		// is it a node or a leaf?
		plane = node->plane;
		if (plane)
		{
			// node
#if 0
			if (!BoxesOverlap(info->lightmins, info->lightmaxs, node->mins, node->maxs))
				continue;
#endif
#if 0
			if (!r_shadow_compilingrtlight && R_CullBox(node->mins, node->maxs, rtlight->cached_numfrustumplanes, rtlight->cached_frustumplanes))
				continue;
#endif
			// axial planes can be processed much more quickly
			if (plane->type < 3)
			{
				// axial plane
				if (info->lightmins[plane->type] > plane->dist)
					nodestack[nodestackpos++] = node->children[0];
				else if (info->lightmaxs[plane->type] < plane->dist)
					nodestack[nodestackpos++] = node->children[1];
				else
				{
					// recurse front side first because the svbsp building prefers it
					if (info->relativelightorigin[plane->type] >= plane->dist)
					{
						if (nodestackpos < GETLIGHTINFO_MAXNODESTACK-1)
							nodestack[nodestackpos++] = node->children[0];
						nodestack[nodestackpos++] = node->children[1];
					}
					else
					{
						if (nodestackpos < GETLIGHTINFO_MAXNODESTACK-1)
							nodestack[nodestackpos++] = node->children[1];
						nodestack[nodestackpos++] = node->children[0];
					}
				}
			}
			else
			{
				// sloped plane
				sides = BoxOnPlaneSide(info->lightmins, info->lightmaxs, plane);
				switch (sides)
				{
				default:
					continue; // ERROR: NAN bounding box!
				case 1:
					nodestack[nodestackpos++] = node->children[0];
					break;
				case 2:
					nodestack[nodestackpos++] = node->children[1];
					break;
				case 3:
					// recurse front side first because the svbsp building prefers it
					if (PlaneDist(info->relativelightorigin, plane) >= 0)
					{
						if (nodestackpos < GETLIGHTINFO_MAXNODESTACK-1)
							nodestack[nodestackpos++] = node->children[0];
						nodestack[nodestackpos++] = node->children[1];
					}
					else
					{
						if (nodestackpos < GETLIGHTINFO_MAXNODESTACK-1)
							nodestack[nodestackpos++] = node->children[1];
						nodestack[nodestackpos++] = node->children[0];
					}
					break;
				}
			}
		}
		else
		{
			// leaf
			leaf = (mleaf_t *)node;
#if 1
			if (!info->noocclusion && info->pvs != NULL && !CHECKPVSBIT(info->pvs, leaf->clusterindex))
				continue;
#endif
#if 1
			if (!BoxesOverlap(info->lightmins, info->lightmaxs, leaf->mins, leaf->maxs))
				continue;
#endif
#if 1
			if (!r_shadow_compilingrtlight && R_CullBox(leaf->mins, leaf->maxs, info->numfrustumplanes, info->frustumplanes))
				continue;
#endif

			if (svbspactive)
			{
				// we can occlusion test the leaf by checking if all of its portals
				// are occluded (unless the light is in this leaf - but that was
				// already handled by the caller)
				for (portal = leaf->portals;portal;portal = portal->next)
				{
					for (i = 0;i < portal->numpoints;i++)
						VectorCopy(portal->points[i].position, points[i]);
					if (SVBSP_AddPolygon(&r_svbsp, portal->numpoints, points[0], false, NULL, NULL, 0) & 2)
						break;
				}
				if (leaf->portals && portal == NULL)
					continue; // no portals of this leaf visible
			}

			// add this leaf to the reduced light bounds
			info->outmins[0] = min(info->outmins[0], leaf->mins[0]);
			info->outmins[1] = min(info->outmins[1], leaf->mins[1]);
			info->outmins[2] = min(info->outmins[2], leaf->mins[2]);
			info->outmaxs[0] = max(info->outmaxs[0], leaf->maxs[0]);
			info->outmaxs[1] = max(info->outmaxs[1], leaf->maxs[1]);
			info->outmaxs[2] = max(info->outmaxs[2], leaf->maxs[2]);

			// mark this leaf as being visible to the light
			if (info->outleafpvs)
			{
				int leafindex = leaf - info->model->brush.data_leafs;
				if (!CHECKPVSBIT(info->outleafpvs, leafindex))
				{
					SETPVSBIT(info->outleafpvs, leafindex);
					info->outleaflist[info->outnumleafs++] = leafindex;
				}
			}

			// when using BIH, we skip the surfaces here
			if (skipsurfaces)
				continue;

			// iterate the surfaces linked by this leaf and check their triangles
			leafsurfaceindices = leaf->firstleafsurface;
			numleafsurfaces = leaf->numleafsurfaces;
			if (svbspinsertoccluder)
			{
				for (leafsurfaceindex = 0;leafsurfaceindex < numleafsurfaces;leafsurfaceindex++)
				{
					surfaceindex = leafsurfaceindices[leafsurfaceindex];
					if (CHECKPVSBIT(info->outsurfacepvs, surfaceindex))
						continue;
					SETPVSBIT(info->outsurfacepvs, surfaceindex);
					surface = surfaces + surfaceindex;
					if (!BoxesOverlap(info->lightmins, info->lightmaxs, surface->mins, surface->maxs))
						continue;
					currentmaterialflags = R_GetCurrentTexture(surface->texture)->currentmaterialflags;
					castshadow = !(currentmaterialflags & MATERIALFLAG_NOSHADOW);
					if (!castshadow)
						continue;
					insidebox = BoxInsideBox(surface->mins, surface->maxs, info->lightmins, info->lightmaxs);
					for (triangleindex = 0, t = surface->num_firsttriangle, e = info->model->surfmesh.data_element3i + t * 3;triangleindex < surface->num_triangles;triangleindex++, t++, e += 3)
					{
						v[0] = info->model->surfmesh.data_vertex3f + e[0] * 3;
						v[1] = info->model->surfmesh.data_vertex3f + e[1] * 3;
						v[2] = info->model->surfmesh.data_vertex3f + e[2] * 3;
						VectorCopy(v[0], v2[0]);
						VectorCopy(v[1], v2[1]);
						VectorCopy(v[2], v2[2]);
						if (insidebox || TriangleBBoxOverlapsBox(v2[0], v2[1], v2[2], info->lightmins, info->lightmaxs))
							SVBSP_AddPolygon(&r_svbsp, 3, v2[0], true, NULL, NULL, 0);
					}
				}
			}
			else
			{
				for (leafsurfaceindex = 0;leafsurfaceindex < numleafsurfaces;leafsurfaceindex++)
				{
					surfaceindex = leafsurfaceindices[leafsurfaceindex];
					surface = surfaces + surfaceindex;
					if(!surface->texture)
						continue;	
					if (CHECKPVSBIT(info->outsurfacepvs, surfaceindex))
						continue;
					SETPVSBIT(info->outsurfacepvs, surfaceindex);
					if (!BoxesOverlap(info->lightmins, info->lightmaxs, surface->mins, surface->maxs))
						continue;
					addedtris = false;
					currentmaterialflags = R_GetCurrentTexture(surface->texture)->currentmaterialflags;
					castshadow = !(currentmaterialflags & MATERIALFLAG_NOSHADOW);
					insidebox = BoxInsideBox(surface->mins, surface->maxs, info->lightmins, info->lightmaxs);
					for (triangleindex = 0, t = surface->num_firsttriangle, e = info->model->surfmesh.data_element3i + t * 3;triangleindex < surface->num_triangles;triangleindex++, t++, e += 3)
					{
						v[0] = info->model->surfmesh.data_vertex3f + e[0] * 3;
						v[1] = info->model->surfmesh.data_vertex3f + e[1] * 3;
						v[2] = info->model->surfmesh.data_vertex3f + e[2] * 3;
						VectorCopy(v[0], v2[0]);
						VectorCopy(v[1], v2[1]);
						VectorCopy(v[2], v2[2]);
						if (!insidebox && !TriangleBBoxOverlapsBox(v2[0], v2[1], v2[2], info->lightmins, info->lightmaxs))
							continue;
						if (svbspactive && !(SVBSP_AddPolygon(&r_svbsp, 3, v2[0], false, NULL, NULL, 0) & 2))
							continue;
						// we don't omit triangles from lighting even if they are
						// backfacing, because when using shadowmapping they are often
						// not fully occluded on the horizon of an edge
						SETPVSBIT(info->outlighttrispvs, t);
						addedtris = true;
						if (castshadow)
						{
							if (noocclusion || (currentmaterialflags & MATERIALFLAG_NOCULLFACE))
							{
								// if the material is double sided we
								// can't cull by direction
								SETPVSBIT(info->outshadowtrispvs, t);
							}
							else if (frontsidecasting)
							{
								// front side casting occludes backfaces,
								// so they are completely useless as both
								// casters and lit polygons
								if (PointInfrontOfTriangle(info->relativelightorigin, v2[0], v2[1], v2[2]))
									SETPVSBIT(info->outshadowtrispvs, t);
							}
							else
							{
								// back side casting does not occlude
								// anything so we can't cull lit polygons
								if (!PointInfrontOfTriangle(info->relativelightorigin, v2[0], v2[1], v2[2]))
									SETPVSBIT(info->outshadowtrispvs, t);
							}
						}
					}
					if (addedtris)
						info->outsurfacelist[info->outnumsurfaces++] = surfaceindex;
				}
			}
		}
	}
}

static void R_Q1BSP_RecursiveGetLightInfo_BIH(r_q1bsp_getlightinfo_t *info, const bih_t *bih)
{
	bih_leaf_t *leaf;
	bih_node_t *node;
	int nodenum;
	int axis;
	int surfaceindex;
	int t;
	int nodeleafindex;
	int currentmaterialflags;
	qbool castshadow;
	qbool noocclusion = info->noocclusion;
	qbool frontsidecasting = info->frontsidecasting;
	msurface_t *surface;
	const int *e;
	const vec_t *v[3];
	float v2[3][3];
	int nodestack[GETLIGHTINFO_MAXNODESTACK];
	int nodestackpos = 0;
	// note: because the BSP leafs are not in the BIH tree, the _BSP function
	// must be called to mark leafs visible for entity culling...
	// we start at the root node
	nodestack[nodestackpos++] = bih->rootnode;
	// we'll be done when the stack is empty
	while (nodestackpos)
	{
		// pop one off the stack to process
		nodenum = nodestack[--nodestackpos];
		// node
		node = bih->nodes + nodenum;
		if (node->type == BIH_UNORDERED)
		{
			for (nodeleafindex = 0;nodeleafindex < BIH_MAXUNORDEREDCHILDREN && node->children[nodeleafindex] >= 0;nodeleafindex++)
			{
				leaf = bih->leafs + node->children[nodeleafindex];
				if (leaf->type != BIH_RENDERTRIANGLE)
					continue;
#if 1
				if (!BoxesOverlap(info->lightmins, info->lightmaxs, leaf->mins, leaf->maxs))
					continue;
#endif
#if 1
				if (!r_shadow_compilingrtlight && R_CullBox(leaf->mins, leaf->maxs, info->numfrustumplanes, info->frustumplanes))
					continue;
#endif
				surfaceindex = leaf->surfaceindex;
				surface = info->model->data_surfaces + surfaceindex;
				currentmaterialflags = R_GetCurrentTexture(surface->texture)->currentmaterialflags;
				castshadow = !(currentmaterialflags & MATERIALFLAG_NOSHADOW);
				t = leaf->itemindex;
				e = info->model->surfmesh.data_element3i + t * 3;
				v[0] = info->model->surfmesh.data_vertex3f + e[0] * 3;
				v[1] = info->model->surfmesh.data_vertex3f + e[1] * 3;
				v[2] = info->model->surfmesh.data_vertex3f + e[2] * 3;
				VectorCopy(v[0], v2[0]);
				VectorCopy(v[1], v2[1]);
				VectorCopy(v[2], v2[2]);
				if (info->svbsp_insertoccluder)
				{
					if (castshadow)
						SVBSP_AddPolygon(&r_svbsp, 3, v2[0], true, NULL, NULL, 0);
					continue;
				}
				if (info->svbsp_active && !(SVBSP_AddPolygon(&r_svbsp, 3, v2[0], false, NULL, NULL, 0) & 2))
					continue;
				// we don't occlude triangles from lighting even
				// if they are backfacing, because when using
				// shadowmapping they are often not fully occluded
				// on the horizon of an edge
				SETPVSBIT(info->outlighttrispvs, t);
				if (castshadow)
				{
					if (noocclusion || (currentmaterialflags & MATERIALFLAG_NOCULLFACE))
					{
						// if the material is double sided we
						// can't cull by direction
						SETPVSBIT(info->outshadowtrispvs, t);
					}
					else if (frontsidecasting)
					{
						// front side casting occludes backfaces,
						// so they are completely useless as both
						// casters and lit polygons
						if (PointInfrontOfTriangle(info->relativelightorigin, v2[0], v2[1], v2[2]))
							SETPVSBIT(info->outshadowtrispvs, t);
					}
					else
					{
						// back side casting does not occlude
						// anything so we can't cull lit polygons
						if (!PointInfrontOfTriangle(info->relativelightorigin, v2[0], v2[1], v2[2]))
							SETPVSBIT(info->outshadowtrispvs, t);
					}
				}
				if (!CHECKPVSBIT(info->outsurfacepvs, surfaceindex))
				{
					SETPVSBIT(info->outsurfacepvs, surfaceindex);
					info->outsurfacelist[info->outnumsurfaces++] = surfaceindex;
				}
			}
		}
		else
		{
			axis = node->type - BIH_SPLITX;
#if 0
			if (!BoxesOverlap(info->lightmins, info->lightmaxs, node->mins, node->maxs))
				continue;
#endif
#if 0
			if (!r_shadow_compilingrtlight && R_CullBox(node->mins, node->maxs, rtlight->cached_numfrustumplanes, rtlight->cached_frustumplanes))
				continue;
#endif
			if (info->lightmins[axis] <= node->backmax)
			{
				if (info->lightmaxs[axis] >= node->frontmin && nodestackpos < GETLIGHTINFO_MAXNODESTACK-1)
					nodestack[nodestackpos++] = node->front;
				nodestack[nodestackpos++] = node->back;
				continue;
			}
			else if (info->lightmaxs[axis] >= node->frontmin)
			{
				nodestack[nodestackpos++] = node->front;
				continue;
			}
			else
				continue; // light falls between children, nothing here
		}
	}
}

static void R_Q1BSP_CallRecursiveGetLightInfo(r_q1bsp_getlightinfo_t *info, qbool use_svbsp)
{
	extern cvar_t r_shadow_usebihculling;
	if (use_svbsp)
	{
		float origin[3];
		VectorCopy(info->relativelightorigin, origin);
		r_svbsp.maxnodes = max(r_svbsp.maxnodes, 1<<12);
		r_svbsp.nodes = (svbsp_node_t*) R_FrameData_Alloc(r_svbsp.maxnodes * sizeof(svbsp_node_t));
		info->svbsp_active = true;
		info->svbsp_insertoccluder = true;
		for (;;)
		{
			SVBSP_Init(&r_svbsp, origin, r_svbsp.maxnodes, r_svbsp.nodes);
			R_Q1BSP_RecursiveGetLightInfo_BSP(info, false);
			// if that failed, retry with more nodes
			if (r_svbsp.ranoutofnodes)
			{
				// an upper limit is imposed
				if (r_svbsp.maxnodes >= 2<<22)
					break;
				r_svbsp.maxnodes *= 2;
				r_svbsp.nodes = (svbsp_node_t*) R_FrameData_Alloc(r_svbsp.maxnodes * sizeof(svbsp_node_t));
				//Mem_Free(r_svbsp.nodes);
				//r_svbsp.nodes = (svbsp_node_t*) Mem_Alloc(tempmempool, r_svbsp.maxnodes * sizeof(svbsp_node_t));
			}
			else
				break;
		}
		// now clear the visibility arrays because we need to redo it
		info->outnumleafs = 0;
		info->outnumsurfaces = 0;
		memset(info->outleafpvs, 0, (info->model->brush.num_leafs + 7) >> 3);
		memset(info->outsurfacepvs, 0, (info->model->num_surfaces + 7) >> 3);
		memset(info->outshadowtrispvs, 0, (info->model->surfmesh.num_triangles + 7) >> 3);
		memset(info->outlighttrispvs, 0, (info->model->surfmesh.num_triangles + 7) >> 3);
	}
	else
		info->svbsp_active = false;

	// we HAVE to mark the leaf the light is in as lit, because portals are
	// irrelevant to a leaf that the light source is inside of
	// (and they are all facing away, too)
	{
		mnode_t *node = info->model->brush.data_nodes;
		mleaf_t *leaf;
		while (node->plane)
			node = node->children[(node->plane->type < 3 ? info->relativelightorigin[node->plane->type] : DotProduct(info->relativelightorigin,node->plane->normal)) < node->plane->dist];
		leaf = (mleaf_t *)node;
		info->outmins[0] = min(info->outmins[0], leaf->mins[0]);
		info->outmins[1] = min(info->outmins[1], leaf->mins[1]);
		info->outmins[2] = min(info->outmins[2], leaf->mins[2]);
		info->outmaxs[0] = max(info->outmaxs[0], leaf->maxs[0]);
		info->outmaxs[1] = max(info->outmaxs[1], leaf->maxs[1]);
		info->outmaxs[2] = max(info->outmaxs[2], leaf->maxs[2]);
		if (info->outleafpvs)
		{
			int leafindex = leaf - info->model->brush.data_leafs;
			if (!CHECKPVSBIT(info->outleafpvs, leafindex))
			{
				SETPVSBIT(info->outleafpvs, leafindex);
				info->outleaflist[info->outnumleafs++] = leafindex;
			}
		}
	}

	info->svbsp_insertoccluder = false;
	// use BIH culling on single leaf maps (generally this only happens if running a model as a map), otherwise use BSP culling to make use of vis data
	if (r_shadow_usebihculling.integer > 0 && (r_shadow_usebihculling.integer == 2 || info->model->brush.num_leafs == 1) && info->model->render_bih.leafs != NULL)
	{
		R_Q1BSP_RecursiveGetLightInfo_BSP(info, true);
		R_Q1BSP_RecursiveGetLightInfo_BIH(info, &info->model->render_bih);
	}
	else
		R_Q1BSP_RecursiveGetLightInfo_BSP(info, false);
	// we're using temporary framedata memory, so this pointer will be invalid soon, clear it
	r_svbsp.nodes = NULL;
	if (developer_extra.integer && use_svbsp)
	{
		Con_DPrintf("GetLightInfo: svbsp built with %i nodes, polygon stats:\n", r_svbsp.numnodes);
		Con_DPrintf("occluders: %i accepted, %i rejected, %i fragments accepted, %i fragments rejected.\n", r_svbsp.stat_occluders_accepted, r_svbsp.stat_occluders_rejected, r_svbsp.stat_occluders_fragments_accepted, r_svbsp.stat_occluders_fragments_rejected);
		Con_DPrintf("queries  : %i accepted, %i rejected, %i fragments accepted, %i fragments rejected.\n", r_svbsp.stat_queries_accepted, r_svbsp.stat_queries_rejected, r_svbsp.stat_queries_fragments_accepted, r_svbsp.stat_queries_fragments_rejected);
	}
}

static msurface_t *r_q1bsp_getlightinfo_surfaces;

static int R_Q1BSP_GetLightInfo_comparefunc(const void *ap, const void *bp)
{
	int a = *(int*)ap;
	int b = *(int*)bp;
	const msurface_t *as = r_q1bsp_getlightinfo_surfaces + a;
	const msurface_t *bs = r_q1bsp_getlightinfo_surfaces + b;
	if (as->texture < bs->texture)
		return -1;
	if (as->texture > bs->texture)
		return 1;
	return a - b;
}

extern cvar_t r_shadow_sortsurfaces;

void R_Mod_GetLightInfo(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, vec3_t clipmins, vec3_t clipmaxs, vec3_t outmins, vec3_t outmaxs, int *outleaflist, unsigned char *outleafpvs, int *outnumleafspointer, int *outsurfacelist, unsigned char *outsurfacepvs, int *outnumsurfacespointer, unsigned char *outshadowtrispvs, unsigned char *outlighttrispvs, unsigned char *visitingleafpvs, int numfrustumplanes, const mplane_t *frustumplanes, qbool noocclusion)
{
	r_q1bsp_getlightinfo_t info;
	info.frontsidecasting = r_shadow_frontsidecasting.integer != 0;
	info.noocclusion = noocclusion || !info.frontsidecasting;
	VectorCopy(relativelightorigin, info.relativelightorigin);
	info.lightradius = lightradius;
	info.lightmins[0] = clipmins[0];
	info.lightmins[1] = clipmins[1];
	info.lightmins[2] = clipmins[2];
	info.lightmaxs[0] = clipmaxs[0];
	info.lightmaxs[1] = clipmaxs[1];
	info.lightmaxs[2] = clipmaxs[2];
	if (ent->model == NULL)
	{
		VectorCopy(info.lightmins, outmins);
		VectorCopy(info.lightmaxs, outmaxs);
		*outnumleafspointer = 0;
		*outnumsurfacespointer = 0;
		return;
	}
	info.model = ent->model;
	info.outleaflist = outleaflist;
	info.outleafpvs = outleafpvs;
	info.outnumleafs = 0;
	info.visitingleafpvs = visitingleafpvs;
	info.outsurfacelist = outsurfacelist;
	info.outsurfacepvs = outsurfacepvs;
	info.outshadowtrispvs = outshadowtrispvs;
	info.outlighttrispvs = outlighttrispvs;
	info.outnumsurfaces = 0;
	info.numfrustumplanes = numfrustumplanes;
	info.frustumplanes = frustumplanes;
	// Don't simply initialize the bounds to the light origin because the only things in radius might be very far away
	float r = 1024.0f * 1024.0f * 1024.0f * 1024.0f;
	VectorSet(info.outmins, r, r, r);
	VectorSet(info.outmaxs, -r, -r, -r);
	memset(visitingleafpvs, 0, (info.model->brush.num_leafs + 7) >> 3);
	memset(outleafpvs, 0, (info.model->brush.num_leafs + 7) >> 3);
	memset(outsurfacepvs, 0, (info.model->num_surfaces + 7) >> 3);
	memset(outshadowtrispvs, 0, (info.model->surfmesh.num_triangles + 7) >> 3);
	memset(outlighttrispvs, 0, (info.model->surfmesh.num_triangles + 7) >> 3);
	if (info.model->brush.GetPVS && !info.noocclusion)
		info.pvs = info.model->brush.GetPVS(info.model, info.relativelightorigin);
	else
		info.pvs = NULL;
	RSurf_ActiveModelEntity(r_refdef.scene.worldentity, false, false, false);

	if (!info.noocclusion && r_shadow_compilingrtlight && r_shadow_realtime_world_compileportalculling.integer && info.model->brush.data_portals)
	{
		// use portal recursion for exact light volume culling, and exact surface checking
		Portal_Visibility(info.model, info.relativelightorigin, info.outleaflist, info.outleafpvs, &info.outnumleafs, info.outsurfacelist, info.outsurfacepvs, &info.outnumsurfaces, NULL, 0, true, info.lightmins, info.lightmaxs, info.outmins, info.outmaxs, info.outshadowtrispvs, info.outlighttrispvs, info.visitingleafpvs);
	}
	else if (!info.noocclusion && r_shadow_realtime_dlight_portalculling.integer && info.model->brush.data_portals)
	{
		// use portal recursion for exact light volume culling, but not the expensive exact surface checking
		Portal_Visibility(info.model, info.relativelightorigin, info.outleaflist, info.outleafpvs, &info.outnumleafs, info.outsurfacelist, info.outsurfacepvs, &info.outnumsurfaces, NULL, 0, r_shadow_realtime_dlight_portalculling.integer >= 2, info.lightmins, info.lightmaxs, info.outmins, info.outmaxs, info.outshadowtrispvs, info.outlighttrispvs, info.visitingleafpvs);
	}
	else
	{
		// recurse the bsp tree, checking leafs and surfaces for visibility
		// optionally using svbsp for exact culling of compiled lights
		// (or if the user enables dlight svbsp culling, which is mostly for
		//  debugging not actual use)
		R_Q1BSP_CallRecursiveGetLightInfo(&info, !info.noocclusion && (r_shadow_compilingrtlight ? r_shadow_realtime_world_compilesvbsp.integer : r_shadow_realtime_dlight_svbspculling.integer) != 0);
	}

	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveModelEntity

	// limit combined leaf box to light boundaries
	outmins[0] = max(info.outmins[0] - 1, info.lightmins[0]);
	outmins[1] = max(info.outmins[1] - 1, info.lightmins[1]);
	outmins[2] = max(info.outmins[2] - 1, info.lightmins[2]);
	outmaxs[0] = min(info.outmaxs[0] + 1, info.lightmaxs[0]);
	outmaxs[1] = min(info.outmaxs[1] + 1, info.lightmaxs[1]);
	outmaxs[2] = min(info.outmaxs[2] + 1, info.lightmaxs[2]);

	*outnumleafspointer = info.outnumleafs;
	*outnumsurfacespointer = info.outnumsurfaces;

	// now sort surfaces by texture for faster rendering
	r_q1bsp_getlightinfo_surfaces = info.model->data_surfaces;
	if (r_shadow_sortsurfaces.integer)
		qsort(info.outsurfacelist, info.outnumsurfaces, sizeof(*info.outsurfacelist), R_Q1BSP_GetLightInfo_comparefunc);
}

void R_Mod_CompileShadowMap(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist)
{
	model_t *model = ent->model;
	msurface_t *surface;
	int surfacelistindex;
	int sidetotals[6] = { 0, 0, 0, 0, 0, 0 }, sidemasks = 0;
	int i;
	// FIXME: the sidetotals code incorrectly assumes that static_meshchain is
	// a single mesh - to prevent that from crashing (sideoffsets, sidetotals
	// exceeding the number of triangles in a single mesh) we have to make sure
	// that we make only a single mesh - so over-estimate the size of the mesh
	// to match the model.
	// bones_was_here: +128 works around BUG https://github.com/DarkPlacesEngine/darkplaces/issues/119
	// +64 was enough to start the map without triggering ASan, +96 was enough to also edit the light.
	// See also: warning in Mod_ShadowMesh_AddMesh
	r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap = Mod_ShadowMesh_Begin(r_main_mempool, model->surfmesh.num_vertices, model->surfmesh.num_triangles + 128);
	R_Shadow_PrepareShadowSides(model->surfmesh.num_triangles);
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		surface = model->data_surfaces + surfacelist[surfacelistindex];
		sidemasks |= R_Shadow_ChooseSidesFromBox(surface->num_firsttriangle, surface->num_triangles, model->surfmesh.data_vertex3f, model->surfmesh.data_element3i, &r_shadow_compilingrtlight->matrix_worldtolight, relativelightorigin, relativelightdirection, r_shadow_compilingrtlight->cullmins, r_shadow_compilingrtlight->cullmaxs, surface->mins, surface->maxs, surface->texture->basematerialflags & MATERIALFLAG_NOSHADOW ? NULL : sidetotals);
	}
	R_Shadow_ShadowMapFromList(model->surfmesh.num_vertices, model->surfmesh.num_triangles, model->surfmesh.data_vertex3f, model->surfmesh.data_element3i, numshadowsides, sidetotals, shadowsides, shadowsideslist);
	r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap = Mod_ShadowMesh_Finish(r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap, true);
	r_shadow_compilingrtlight->static_shadowmap_receivers &= sidemasks;
	for(i = 0;i<6;i++)
		if(!sidetotals[i])
			r_shadow_compilingrtlight->static_shadowmap_casters &= ~(1 << i);
}

#define RSURF_MAX_BATCHSURFACES 8192

static const msurface_t *batchsurfacelist[RSURF_MAX_BATCHSURFACES];

void R_Mod_DrawShadowMap(int side, entity_render_t *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int modelnumsurfaces, const int *modelsurfacelist, const unsigned char *surfacesides, const vec3_t lightmins, const vec3_t lightmaxs)
{
	model_t *model = ent->model;
	const msurface_t *surface;
	int modelsurfacelistindex, batchnumsurfaces;
	// check the box in modelspace, it was already checked in worldspace
	if (!BoxesOverlap(model->normalmins, model->normalmaxs, lightmins, lightmaxs))
		return;
	R_FrameData_SetMark();
	// identify lit faces within the bounding box
	for (modelsurfacelistindex = 0;modelsurfacelistindex < modelnumsurfaces;modelsurfacelistindex++)
	{
		surface = model->data_surfaces + modelsurfacelist[modelsurfacelistindex];
		if (surfacesides && !(surfacesides[modelsurfacelistindex] & (1 << side)))
			continue;
		rsurface.texture = R_GetCurrentTexture(surface->texture);
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_NOSHADOW)
			continue;
		if (!BoxesOverlap(lightmins, lightmaxs, surface->mins, surface->maxs))
			continue;
		r_refdef.stats[r_stat_lights_dynamicshadowtriangles] += surface->num_triangles;
		r_refdef.stats[r_stat_lights_shadowtriangles] += surface->num_triangles;
		batchsurfacelist[0] = surface;
		batchnumsurfaces = 1;
		while(++modelsurfacelistindex < modelnumsurfaces && batchnumsurfaces < RSURF_MAX_BATCHSURFACES)
		{
			surface = model->data_surfaces + modelsurfacelist[modelsurfacelistindex];
			if (surfacesides && !(surfacesides[modelsurfacelistindex] & (1 << side)))
				continue;
			if (surface->texture != batchsurfacelist[0]->texture)
				break;
			if (!BoxesOverlap(lightmins, lightmaxs, surface->mins, surface->maxs))
				continue;
			r_refdef.stats[r_stat_lights_dynamicshadowtriangles] += surface->num_triangles;
			r_refdef.stats[r_stat_lights_shadowtriangles] += surface->num_triangles;
			batchsurfacelist[batchnumsurfaces++] = surface;
		}
		--modelsurfacelistindex;
		GL_CullFace(rsurface.texture->currentmaterialflags & MATERIALFLAG_NOCULLFACE ? GL_NONE : r_refdef.view.cullface_back);
		RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ALLOWMULTIDRAW, batchnumsurfaces, batchsurfacelist);
		R_Mesh_PrepareVertices_Vertex3f(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.batchvertex3f_vertexbuffer, rsurface.batchvertex3f_bufferoffset);
		RSurf_DrawBatch();
	}
	R_FrameData_ReturnToMark();
}

#define BATCHSIZE 1024

static void R_Q1BSP_DrawLight_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i, j, endsurface;
	texture_t *t;
	const msurface_t *surface;
	R_FrameData_SetMark();
	// note: in practice this never actually receives batches
	R_Shadow_RenderMode_Begin();
	R_Shadow_RenderMode_ActiveLight(rtlight);
	R_Shadow_RenderMode_Lighting(true, rtlight->shadowmapatlassidesize != 0, (ent->flags & RENDER_NOSELFSHADOW) != 0);
	R_Shadow_SetupEntityLight(ent);
	for (i = 0;i < numsurfaces;i = j)
	{
		j = i + 1;
		surface = rsurface.modelsurfaces + surfacelist[i];
		t = surface->texture;
		rsurface.texture = R_GetCurrentTexture(t);
		endsurface = min(j + BATCHSIZE, numsurfaces);
		for (j = i;j < endsurface;j++)
		{
			surface = rsurface.modelsurfaces + surfacelist[j];
			if (t != surface->texture)
				break;
			R_Shadow_RenderLighting(1, &surface);
		}
	}
	R_Shadow_RenderMode_End();
	R_FrameData_ReturnToMark();
}

extern qbool r_shadow_usingdeferredprepass;
void R_Mod_DrawLight(entity_render_t *ent, int numsurfaces, const int *surfacelist, const unsigned char *lighttrispvs)
{
	model_t *model = ent->model;
	const msurface_t *surface;
	int i, k, kend, l, endsurface, batchnumsurfaces, texturenumsurfaces;
	const msurface_t **texturesurfacelist;
	texture_t *tex;
	CHECKGLERROR
	R_FrameData_SetMark();
	// this is a double loop because non-visible surface skipping has to be
	// fast, and even if this is not the world model (and hence no visibility
	// checking) the input surface list and batch buffer are different formats
	// so some processing is necessary.  (luckily models have few surfaces)
	for (i = 0;i < numsurfaces;)
	{
		batchnumsurfaces = 0;
		endsurface = min(i + RSURF_MAX_BATCHSURFACES, numsurfaces);
		if (ent == r_refdef.scene.worldentity)
		{
			for (;i < endsurface;i++)
				if (r_refdef.viewcache.world_surfacevisible[surfacelist[i]])
					batchsurfacelist[batchnumsurfaces++] = model->data_surfaces + surfacelist[i];
		}
		else
		{
			for (;i < endsurface;i++)
				batchsurfacelist[batchnumsurfaces++] = model->data_surfaces + surfacelist[i];
		}
		if (!batchnumsurfaces)
			continue;
		for (k = 0;k < batchnumsurfaces;k = kend)
		{
			surface = batchsurfacelist[k];
			tex = surface->texture;
			rsurface.texture = R_GetCurrentTexture(tex);
			// gather surfaces into a batch range
			for (kend = k;kend < batchnumsurfaces && tex == batchsurfacelist[kend]->texture;kend++)
				;
			// now figure out what to do with this particular range of surfaces
			// VorteX: added MATERIALFLAG_NORTLIGHT
			if ((rsurface.texture->currentmaterialflags & (MATERIALFLAG_WALL | MATERIALFLAG_NORTLIGHT)) != MATERIALFLAG_WALL)
				continue;
			if (r_fb.water.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION | MATERIALFLAG_CAMERA)))
				continue;
			if (rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED)
			{
				vec3_t tempcenter, center;
				for (l = k;l < kend;l++)
				{
					surface = batchsurfacelist[l];
					if (r_transparent_sortsurfacesbynearest.integer)
					{
						tempcenter[0] = bound(surface->mins[0], rsurface.localvieworigin[0], surface->maxs[0]);
						tempcenter[1] = bound(surface->mins[1], rsurface.localvieworigin[1], surface->maxs[1]);
						tempcenter[2] = bound(surface->mins[2], rsurface.localvieworigin[2], surface->maxs[2]);
					}
					else
					{
						tempcenter[0] = (surface->mins[0] + surface->maxs[0]) * 0.5f;
						tempcenter[1] = (surface->mins[1] + surface->maxs[1]) * 0.5f;
						tempcenter[2] = (surface->mins[2] + surface->maxs[2]) * 0.5f;
					}
					Matrix4x4_Transform(&rsurface.matrix, tempcenter, center);
					if (ent->transparent_offset) // transparent offset
					{
						center[0] += r_refdef.view.forward[0]*ent->transparent_offset;
						center[1] += r_refdef.view.forward[1]*ent->transparent_offset;
						center[2] += r_refdef.view.forward[2]*ent->transparent_offset;
					}
					R_MeshQueue_AddTransparent((rsurface.entity->flags & RENDER_WORLDOBJECT) ? TRANSPARENTSORT_SKY : ((rsurface.texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST) ? TRANSPARENTSORT_HUD : rsurface.texture->transparentsort), center, R_Q1BSP_DrawLight_TransparentCallback, ent, surface - rsurface.modelsurfaces, rsurface.rtlight);
				}
				continue;
			}
			if (r_shadow_usingdeferredprepass)
				continue;
			texturenumsurfaces = kend - k;
			texturesurfacelist = batchsurfacelist + k;
			R_Shadow_RenderLighting(texturenumsurfaces, texturesurfacelist);
		}
	}
	R_FrameData_ReturnToMark();
}

//Made by [515]
static void R_ReplaceWorldTexture_f(cmd_state_t *cmd)
{
	model_t		*m;
	texture_t	*t;
	int			i;
	const char	*r, *newt;
	skinframe_t *skinframe;
	if (!r_refdef.scene.worldmodel)
	{
		Con_Printf("There is no worldmodel\n");
		return;
	}
	m = r_refdef.scene.worldmodel;

	if(Cmd_Argc(cmd) < 2)
	{
		Con_Print("r_replacemaptexture <texname> <newtexname> - replaces texture\n");
		Con_Print("r_replacemaptexture <texname> - switch back to default texture\n");
		return;
	}
	if(!cl.islocalgame || !cl.worldmodel)
	{
		Con_Print("This command works only in singleplayer\n");
		return;
	}
	r = Cmd_Argv(cmd, 1);
	newt = Cmd_Argv(cmd, 2);
	if(!newt[0])
		newt = r;
	for(i=0,t=m->data_textures;i<m->num_textures;i++,t++)
	{
		if(/*t->width && !strcasecmp(t->name, r)*/ matchpattern( t->name, r, true ) )
		{
			if ((skinframe = R_SkinFrame_LoadExternal(newt, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PICMIP, true, false)))
			{
//				t->skinframes[0] = skinframe;
				t->currentskinframe = skinframe;
				Con_Printf("%s replaced with %s\n", r, newt);
			}
			else
			{
				Con_Printf("%s was not found\n", newt);
				return;
			}
		}
	}
}

//Made by [515]
static void R_ListWorldTextures_f(cmd_state_t *cmd)
{
	model_t		*m;
	texture_t	*t;
	int			i;
	if (!r_refdef.scene.worldmodel)
	{
		Con_Printf("There is no worldmodel\n");
		return;
	}
	m = r_refdef.scene.worldmodel;

	Con_Print("Worldmodel textures :\n");
	for(i=0,t=m->data_textures;i<m->num_textures;i++,t++)
		if (t->name[0] && strcasecmp(t->name, "NO TEXTURE FOUND"))
			Con_Printf("%s\n", t->name);
}

#if 0
static void gl_surf_start(void)
{
}

static void gl_surf_shutdown(void)
{
}

static void gl_surf_newmap(void)
{
}
#endif

void GL_Surf_Init(void)
{

	Cvar_RegisterVariable(&r_ambient);
	Cvar_RegisterVariable(&r_lockpvs);
	Cvar_RegisterVariable(&r_lockvisibility);
	Cvar_RegisterVariable(&r_useportalculling);
	Cvar_RegisterVariable(&r_usesurfaceculling);
	Cvar_RegisterVariable(&r_vis_debug);
	Cvar_RegisterVariable(&r_vis_debug_linewidth);
	Cvar_RegisterVariable(&r_vis_debug_nodepthtest);
	Cvar_RegisterVariable(&r_vis_minimum_radius);
	Cvar_RegisterVariable(&r_vis_maximum_radius);
	Cvar_RegisterVariable(&r_vis_scatter_radius);
	Cvar_RegisterVariable(&r_vis_bouncevis_clip_scattered_ray_origin);
	Cvar_RegisterVariable(&r_vis_bouncevis_movement_prediction_time);
	Cvar_RegisterVariable(&r_vis_bouncevis_fibonacci_spiral);
	Cvar_RegisterVariable(&r_vis_bouncevis);
	Cvar_RegisterVariable(&r_vis_bouncevis_new_rays_per_frame);
	Cvar_RegisterVariable(&r_vis_bouncevis_use_frustum);
	Cvar_RegisterVariable(&r_vis_bouncevis_bounces);
	Cvar_RegisterVariable(&r_vis_bouncevis_pad);
	Cvar_RegisterVariable(&r_vis_trace);
	Cvar_RegisterVariable(&r_vis_trace_samples);
	Cvar_RegisterVariable(&r_vis_trace_delay);
	Cvar_RegisterVariable(&r_vis_trace_enlarge);
	Cvar_RegisterVariable(&r_vis_trace_expand);
	Cvar_RegisterVariable(&r_vis_trace_pad);
	Cvar_RegisterVariable(&r_vis_trace_surfaces);
	Cvar_RegisterVariable(&r_q3bsp_renderskydepth);

	Cmd_AddCommand(CF_CLIENT, "r_replacemaptexture", R_ReplaceWorldTexture_f, "override a map texture for testing purposes");
	Cmd_AddCommand(CF_CLIENT, "r_listmaptextures", R_ListWorldTextures_f, "list all textures used by the current map");

	//R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

