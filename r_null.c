#include "quakedef.h"
#include "cl_collision.h"
#include "dpsoftrast.h"
#ifdef SUPPORTD3D
#include <d3d9.h>
#endif
#include "image.h"
#include "wad.h"
#include "cl_video.h"
#include "cl_dyntexture.h"
#include "ft2.h"
#include "ft2_fontdefs.h"
#include "r_shadow.h"
#include "polygon.h"
#include "csprogs.h"
#ifdef SUPPORTD3D
#include <d3dx9.h>
#endif
#include "portals.h"
#include "jpeg.h"
#include "image_png.h"
#include "intoverflow.h"
#ifdef __ANDROID__
#include "ktx10/include/ktx.h"
#endif

cvar_t developer_texturelogging = {0, "developer_texturelogging", "0", "produces a textures.log file containing names of skins and map textures the engine tried to load"};
cvar_t gl_dither = {CVAR_SAVE, "gl_dither", "1", "enables OpenGL dithering (16bit looks bad with this off)"};
cvar_t gl_max_lightmapsize = {CVAR_SAVE, "gl_max_lightmapsize", "1024", "maximum allowed texture size for lightmap textures, use larger values to improve rendering speed, as long as there is enough video memory available (setting it too high for the hardware will cause very bad performance)"};
cvar_t gl_paranoid = {0, "gl_paranoid", "0", "enables OpenGL error checking and other tests"};
cvar_t gl_polyblend = {CVAR_SAVE, "gl_polyblend", "1", "tints view while underwater, hurt, etc"};
cvar_t gl_printcheckerror = {0, "gl_printcheckerror", "0", "prints all OpenGL error checks, useful to identify location of driver crashes"};
cvar_t gl_texturecompression = {CVAR_SAVE, "gl_texturecompression", "0", "whether to compress textures, a value of 0 disables compression (even if the individual cvars are 1), 1 enables fast (low quality) compression at startup, 2 enables slow (high quality) compression at startup"};
cvar_t gl_texturecompression_q3bspdeluxemaps = {CVAR_SAVE, "gl_texturecompression_q3bspdeluxemaps", "0", "whether to compress deluxemaps in q3bsp format levels (only levels compiled with q3map2 -deluxe have these)"};
cvar_t gl_texturecompression_q3bsplightmaps = {CVAR_SAVE, "gl_texturecompression_q3bsplightmaps", "0", "whether to compress lightmaps in q3bsp format levels"};
cvar_t gl_texturecompression_sprites = {CVAR_SAVE, "gl_texturecompression_sprites", "1", "whether to compress sprites"};
cvar_t r_drawworld = {0, "r_drawworld","1", "draw world (most static stuff)"};
cvar_t r_dynamic = {CVAR_SAVE, "r_dynamic","1", "enables dynamic lights (rocket glow and such)"};
cvar_t r_equalize_entities_fullbright = {CVAR_SAVE, "r_equalize_entities_fullbright", "0", "render fullbright entities by equalizing their lightness, not by not rendering light"};
cvar_t r_fog_clear = {0, "r_fog_clear", "1", "clears renderbuffer with fog color before render starts"};
cvar_t r_fullbright = {0, "r_fullbright","0", "makes map very bright and renders faster"};
cvar_t r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1", "enables glowing pixels in quake textures (changes need r_restart to take effect)"};
cvar_t r_lerplightstyles = {CVAR_SAVE, "r_lerplightstyles", "0", "enable animation smoothing on flickering lights"};
cvar_t r_lerpmodels = {CVAR_SAVE, "r_lerpmodels", "1", "enables animation smoothing on models"};
cvar_t r_lerpsprites = {CVAR_SAVE, "r_lerpsprites", "0", "enables animation smoothing on sprites"};
cvar_t r_shadow_lightattenuationdividebias = {0, "r_shadow_lightattenuationdividebias", "1", "changes attenuation texture generation"};
cvar_t r_shadow_lightattenuationlinearscale = {0, "r_shadow_lightattenuationlinearscale", "2", "changes attenuation texture generation"};
cvar_t r_smoothnormals_areaweighting = {0, "r_smoothnormals_areaweighting", "1", "uses significantly faster (and supposedly higher quality) area-weighted vertex normals and tangent vectors rather than summing normalized triangle normals and tangents"};
cvar_t r_speeds = {0, "r_speeds","0", "displays rendering statistics and per-subsystem timings"};
cvar_t r_texture_dds_save = {CVAR_SAVE, "r_texture_dds_save", "0", "save compressed dds/filename.dds texture when filename.tga is loaded, so that it can be loaded instead next time"};
cvar_t r_transparent_sortarraysize = {CVAR_SAVE, "r_transparent_sortarraysize", "4096", "number of distance-sorting layers"};
cvar_t r_transparent_sortmaxdist = {CVAR_SAVE, "r_transparent_sortmaxdist", "32768", "upper distance limit for transparent sorting"};
cvar_t r_transparent_sortmindist = {CVAR_SAVE, "r_transparent_sortmindist", "0", "lower distance limit for transparent sorting"};
cvar_t r_transparent_useplanardistance = {0, "r_transparent_useplanardistance", "0", "sort transparent meshes by distance from view plane rather than spherical distance to the chosen point"};
cvar_t r_viewfbo = {CVAR_SAVE, "r_viewfbo", "0", "enables use of an 8bit (1) or 16bit (2) or 32bit (3) per component float framebuffer render, which may be at a different resolution than the video mode"};
cvar_t r_waterwarp = {CVAR_SAVE, "r_waterwarp", "1", "warp view while underwater"};
cvar_t v_flipped = {0, "v_flipped", "0", "mirror the screen (poor man's left handed mode)"};

rtexturepool_t *r_main_texturepool;
rtexturepool_t *drawtexturepool;

rtexture_t *r_texture_blanknormalmap;
rtexture_t *r_texture_white;
rtexture_t *r_texture_grey128;
rtexture_t *r_texture_notexture;

r_refdef_t r_refdef;

dp_fonts_t dp_fonts;

int polygonelement3i[(POLYGONELEMENTS_MAXPOINTS-2)*3];
unsigned short polygonelement3s[(POLYGONELEMENTS_MAXPOINTS-2)*3];
int quadelement3i[QUADELEMENTS_MAXQUADS*6];
unsigned short quadelement3s[QUADELEMENTS_MAXQUADS*6];

qboolean r_draw2d_force = false;

skinframe_t *R_SkinFrame_LoadExternal(const char *name, int textureflags, qboolean complain)
{
}

skinframe_t *R_SkinFrame_LoadInternalQuake(const char *name, int textureflags, int loadpantsandshirt, int loadglowtexture, const unsigned char *skindata, int width, int height)
{
}

skinframe_t *R_SkinFrame_FindNextByName( skinframe_t *last, const char *name )
{
}

skinframe_t *R_SkinFrame_LoadInternalBGRA(const char *name, int textureflags, const unsigned char *skindata, int width, int height, qboolean sRGB)
{
}

skinframe_t *R_SkinFrame_LoadMissing(void)
{
}

rtexture_t *R_LoadTexture2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, textype_t textype, int flags, int miplevel, const unsigned int *palette)
{
}

rtexture_t *R_GetCubemap(const char *basename)
{
}

rtexturepool_t *R_AllocTexturePool(void)
{
}

cachepic_t *Draw_CachePic_Flags(const char *path, unsigned int cachepicflags)
{
}

cachepic_t *Draw_CachePic (const char *path)
{
}

rtexture_t *Draw_GetPicTexture(cachepic_t *pic)
{
}

cachepic_t *Draw_NewPic(const char *picname, int width, int height, int alpha, unsigned char *pixels_bgra)
{
}

float DrawQ_Color[4];
float DrawQ_String_Scale(float startx, float starty, const char *text, size_t maxlen, float w, float h, float sw, float sh, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt)
{
}

float DrawQ_String(float startx, float starty, const char *text, size_t maxlen, float w, float h, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt)
{
}

float DrawQ_TextWidth_UntilWidth_TrackColors(const char *text, size_t *maxlen, float w, float h, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxwidth)
{
}

float DrawQ_TextWidth(const char *text, size_t maxlen, float w, float h, qboolean ignorecolorcodes, const dp_font_t *fnt)
{
}

float DrawQ_TextWidth_UntilWidth(const char *text, size_t *maxlen, float w, float h, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxWidth)
{
}

float DrawQ_TextWidth_UntilWidth_TrackColors_Scale(const char *text, size_t *maxlen, float w, float h, float sw, float sh, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxwidth)
{
}

dp_font_t *FindFont(const char *title, qboolean allocate_new)
{
}

r_meshbuffer_t *R_Mesh_CreateMeshBuffer(const void *data, size_t size, const char *name, qboolean isindexbuffer, qboolean isuniformbuffer, qboolean isdynamic, qboolean isindex16)
{
}

r_vertexgeneric_t *R_Mesh_PrepareVertices_Generic_Lock(int numvertices)
{
}

qboolean R_Mesh_PrepareVertices_Generic_Unlock(void)
{
}

int R_PicmipForFlags(int flags)
{
}

int R_SaveTextureDDSFile(rtexture_t *rt, const char *filename, qboolean skipuncompressed, qboolean hasalpha)
{
}

int R_TextureWidth(rtexture_t *rt)
{
}

int R_TextureHeight(rtexture_t *rt)
{
}

int R_Shadow_GetRTLightInfo(unsigned int lightindex, float *origin, float *radius, float *color)
{
}

int R_SetSkyBox(const char *sky)
{
}

void Draw_Frame(void)
{
}

void Draw_FreePic(const char *picname)
{
}

void DrawQ_Fill(float x, float y, float width, float height, float red, float green, float blue, float alpha, int flags)
{
}

void DrawQ_Finish(void)
{
}

void DrawQ_Line(float width, float x1, float y1, float x2, float y2, float r, float g, float b, float alpha, int flags)
{
}

void DrawQ_Lines(float width, int numlines, int flags, qboolean hasalpha)
{
}

void DrawQ_Mesh(drawqueuemesh_t *mesh, int flags, qboolean hasalpha)
{
}

void DrawQ_Pic(float x, float y, cachepic_t *pic, float width, float height, float red, float green, float blue, float alpha, int flags)
{
}

void DrawQ_ProcessDrawFlag(int flags, qboolean alpha)
{
}

void DrawQ_RecalcView(void)
{
}

void DrawQ_ResetClipArea(void)
{
}

void DrawQ_RotPic(float x, float y, cachepic_t *pic, float width, float height, float org_x, float org_y, float angle, float red, float green, float blue, float alpha, int flags)
{
}

void DrawQ_SetClipArea(float x, float y, float width, float height)
{
}

void DrawQ_SuperPic(float x, float y, cachepic_t *pic, float width, float height, float s1, float t1, float r1, float g1, float b1, float a1, float s2, float t2, float r2, float g2, float b2, float a2, float s3, float t3, float r3, float g3, float b3, float a3, float s4, float t4, float r4, float g4, float b4, float a4, int flags)
{
}

void FOG_clear(void)
{
}



void GL_BlendFunc(int blendfunc1, int blendfunc2)
{
}

void GL_Clear(int mask, const float *colorvalue, float depthvalue, int stencilvalue)
{
}

void GL_Color(float cr, float cg, float cb, float ca)
{
}

void GL_ColorMask(int r, int g, int b, int a)
{
}

void GL_CullFace(int state)
{
}

void GL_DepthMask(int state)
{
}

void GL_DepthRange(float nearfrac, float farfrac)
{
}

void GL_DepthTest(int state)
{
}

void GL_Finish(void)
{
}

void GL_Mesh_ListVBOs(qboolean printeach)
{
}

void GL_PolygonOffset(float planeoffset, float depthoffset)
{
}

#ifdef DEBUGGL
int errornumber = 0;
void GL_PrintError(int errornumber, const char *filename, int linenumber)
{
}
#endif

void GL_ReadPixelsBGRA(int x, int y, int width, int height, unsigned char *outpixels)
{
}

void GL_Scissor(int x, int y, int width, int height)
{
}

void GL_ScissorTest(int state)
{
}

void LoadFont(qboolean override, const char *name, dp_font_t *fnt, float scale, float voffset)
{
}


void R_BufferData_NewFrame(void)
{
}

void R_CalcBeam_Vertex3f(float *vert, const float *org1, const float *org2, float width)
{
}

void R_CompleteLightPoint(vec3_t ambient, vec3_t diffuse, vec3_t lightdir, const vec3_t p, const int flags)
{
}

void R_DecalSystem_Reset(decalsystem_t *decalsystem)
{
}

void R_DecalSystem_SplatEntities(const vec3_t worldorigin, const vec3_t worldnormal, float r, float g, float b, float a, float s1, float t1, float s2, float t2, float worldsize)
{
}


void R_DrawGamma(void)
{
}




void R_EntityMatrix(const matrix4x4_t *matrix)
{
}

void R_FrameData_NewFrame(void)
{
}


void R_FreeTexturePool(rtexturepool_t **rtexturepool)
{
}

void R_FreeTexture(rtexture_t *rt)
{
}

void R_HDR_UpdateIrisAdaptation(const vec3_t point)
{
}

void R_LightPoint(float *color, const vec3_t p, const int flags)
{
}

void R_MakeTextureDynamic(rtexture_t *rt, updatecallback_t updatecallback, void *data)
{
}

void R_MarkDirtyTexture(rtexture_t *rt)
{
}



void R_Mesh_CopyToTexture(rtexture_t *tex, int tx, int ty, int sx, int sy, int width, int height)
{
}

void R_Mesh_DestroyMeshBuffer(r_meshbuffer_t *buffer)
{
}

void R_Mesh_Draw(int firstvertex, int numvertices, int firsttriangle, int numtriangles, const int *element3i, const r_meshbuffer_t *element3i_indexbuffer, int element3i_bufferoffset, const unsigned short *element3s, const r_meshbuffer_t *element3s_indexbuffer, int element3s_bufferoffset)
{
}

void R_Mesh_Finish(void)
{
}

void R_Mesh_PrepareVertices_Generic_Arrays(int numvertices, const float *vertex3f, const float *color4f, const float *texcoord2f)
{
}

void R_Mesh_PrepareVertices_Generic(int numvertices, const r_vertexgeneric_t *vertex, const r_meshbuffer_t *vertexbuffer, int bufferoffset)
{
}


void R_Mesh_ResetTextureState(void)
{
}

void R_Mesh_SetRenderTargets(int fbo, rtexture_t *depthtexture, rtexture_t *colortexture, rtexture_t *colortexture2, rtexture_t *colortexture3, rtexture_t *colortexture4)
{
}

void R_Mesh_Start(void)
{
}




void R_Model_Sprite_Draw(entity_render_t *ent)
{
}

void R_NewExplosion(const vec3_t org)
{
}

void R_Q1BSP_CompileShadowMap(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist)
{
}

void R_Q1BSP_CompileShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist)
{
}

void R_Q1BSP_DrawAddWaterPlanes(entity_render_t *ent)
{
}

void R_Q1BSP_DrawDebug(entity_render_t *ent)
{
}

void R_Q1BSP_DrawDepth(entity_render_t *ent)
{
}

void R_Q1BSP_Draw(entity_render_t *ent)
{
}

void R_Q1BSP_DrawLight(entity_render_t *ent, int numsurfaces, const int *surfacelist, const unsigned char *lighttrispvs)
{
}

void R_Q1BSP_DrawPrepass(entity_render_t *ent)
{
}

void R_Q1BSP_DrawShadowMap(int side, entity_render_t *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int modelnumsurfaces, const int *modelsurfacelist, const unsigned char *surfacesides, const vec3_t lightmins, const vec3_t lightmaxs)
{
}

void R_Q1BSP_DrawShadowVolume(entity_render_t *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int modelnumsurfaces, const int *modelsurfacelist, const vec3_t lightmins, const vec3_t lightmaxs)
{
}

void R_Q1BSP_DrawSky(entity_render_t *ent)
{
}

void R_Q1BSP_GetLightInfo(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outleaflist, unsigned char *outleafpvs, int *outnumleafspointer, int *outsurfacelist, unsigned char *outsurfacepvs, int *outnumsurfacespointer, unsigned char *outshadowtrispvs, unsigned char *outlighttrispvs, unsigned char *visitingleafpvs, int numfrustumplanes, const mplane_t *frustumplanes)
{
}

void R_RegisterModule(const char *name, void(*start)(void), void(*shutdown)(void), void(*newmap)(void), void(*devicelost)(void), void(*devicerestored)(void))
{
}

void R_RenderView_UpdateViewVectors(void)
{
}

void R_RenderView(void)
{
}

void R_ResetSkyBox(void)
{
}



void R_RTLight_Update(rtlight_t *rtlight, int isstatic, matrix4x4_t *matrix, vec3_t color, int style, const char *cubemapname, int shadow, vec_t corona, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags)
{
}

void R_SelectScene( r_refdef_scene_type_t scenetype )
{
}



void R_SetupShader_Generic_NoTexture(qboolean usegamma, qboolean notrippy)
{
}

void R_SetupShader_Generic(rtexture_t *first, rtexture_t *second, int texturemode, int rgbscale, qboolean usegamma, qboolean notrippy, qboolean suppresstexalpha)
{
}

void R_SetViewport(const r_viewport_t *v)
{
}

void R_Shadow_EditLights_DrawSelectedLightProperties(void)
{
}

void R_SkinFrame_MarkUsed(skinframe_t *skinframe)
{
}

void R_Stain(const vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2)
{
}

void RSurf_ActiveWorldEntity(void)
{
}

float RSurf_FogVertex(const float *v)
{
}

void R_Textures_Frame(void)
{
}

void R_TextureStats_Print(qboolean printeach, qboolean printpool, qboolean printtotal)
{
}

void R_UpdateFog(void)
{
}

void R_UpdateTexture(rtexture_t *rt, const unsigned char *data, int x, int y, int z, int width, int height, int depth)
{
}

void R_UpdateVariables(void)
{
}

void R_Viewport_InitOrtho(r_viewport_t *v, const matrix4x4_t *cameramatrix, int x, int y, int width, int height, float x1, float y1, float x2, float y2, float nearclip, float farclip, const float *nearplane)
{
}


