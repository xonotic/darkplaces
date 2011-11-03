#ifndef FT2_PRIVATE_H__
#define FT2_PRIVATE_H__

// FIXME: When all this is done: rename
// this to glyph_slot_s again...

// How big textures should be...
#define FONT_GLYPHTEX_WIDTH 512
#define FONT_GLYPHTEX_HEIGHT 512

typedef struct ft2_glyphtex_s
{
	int width, height;
	int texflags;

	rtexture_t    *tex;
	//unsigned char *data;
	int            bytesPerPixel;
	//int            pitch;
	/* FIXME: remove this, I have it here so I don't need to switch over to model_shared.h to see the defs :P
void Mod_AllocLightmap_Init(mod_alloclightmap_state_t *state, int width, int height);
void Mod_AllocLightmap_Free(mod_alloclightmap_state_t *state);
void Mod_AllocLightmap_Reset(mod_alloclightmap_state_t *state);
qboolean Mod_AllocLightmap_Block(mod_alloclightmap_state_t *state, int blockwidth, int blockheight, int *outx, int *outy);
*/
	mod_alloclightmap_state_t blockstate;

	// This acts as a reference count for glyphs
	// When a glyph is loaded, this gets increased
	// When a font is unloaded, this is decreased
	// for every glyph on this texture. If we hit
	// 0, we remove the texture completely.
	// The goal is that if there's only few characters
	// being used, but many characters allocated, we
	// destroy this texture to have the used glyphs be
	// moved to a more-active texture.
	int glyph_count;
	int glyphs_freed;
	//int lastusedframe; // <- useful, we unload unused images

	// NOTE: We also need a way to remove glyphs
	// from still-existing fonts when we remove
	// this texture, to force them to reload them.
	// NOTE: Rather then re-rendering, we could just copy them directly.
	struct glyph_s *first_glyph;

	struct ft2_glyphtex_s *prev;
	struct ft2_glyphtex_s *next;
} ft2_glyphtex_t;

ft2_glyphtex_t *Font_New_GlyphTex(int width, int height);

// anything should work, but I recommend multiples of 8
// since the texture size should be a power of 2
#define FONT_CHARS_PER_LINE 16
#define FONT_CHAR_LINES 16
#define FONT_CHARS_PER_MAP (FONT_CHARS_PER_LINE * FONT_CHAR_LINES)

typedef struct glyph_slot_s
{
	qboolean image;
	// we keep the quad coords here only currently
	// if you need other info, make Font_LoadMapForIndex fill it into this slot
	float txmin; // texture coordinate in [0,1]
	float txmax;
	float tymin;
	float tymax;
	float vxmin;
	float vxmax;
	float vymin;
	float vymax;
	float advance_x;
	float advance_y;
} glyph_slot_t;

struct ft2_font_map_s
{
	Uchar                  start;
	struct ft2_font_map_s *next;
	float                  size;
	// the actual size used in the freetype code
	// by convention, the requested size is the height of the font's bounding box.
	float                  intSize;
	int                    glyphSize;

	cachepic_t            *pic;
	qboolean               static_tex;
	glyph_slot_t           glyphs[FONT_CHARS_PER_MAP];

	// contains the kerning information for the first 256 characters
	// for the other characters, we will lookup the kerning information
	ft2_kerning_t          kerning;
	// safes us the trouble of calculating these over and over again
	double                 sfx, sfy;

	// the width_of for the image-font, pixel-snapped for this size
	float           width_of[256];
};

struct ft2_attachment_s
{
	const unsigned char *data;
	fs_offset_t    size;
};

//qboolean Font_LoadMapForIndex(ft2_font_t *font, Uchar _ch, ft2_font_map_t **outmap);
//qboolean Font_LoadMapForIndex(ft2_font_t *font, int map_index, Uchar _ch, ft2_font_map_t **outmap);

void font_start(void);
void font_shutdown(void);
void font_newmap(void);

#endif // FT2_PRIVATE_H__
