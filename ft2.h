/* Header for FreeType 2 and UTF-8 encoding support for
 * DarkPlaces
 */

#ifndef DP_FREETYPE2_H__
#define DP_FREETYPE2_H__

//#include <sys/types.h>

#include "utf8lib.h"

/* 
 * From http://www.unicode.org/Public/UNIDATA/Blocks.txt
 *
 *   E000..F8FF; Private Use Area
 *   F0000..FFFFF; Supplementary Private Use Area-A
 *
 * We use:
 *   Range E000 - E0FF
 *     Contains the non-FreeType2 version of characters.
 */

typedef struct ft2_glyphtex_s ft2_glyphtex_t;
typedef struct ft2_font_map_s ft2_font_map_t;
typedef struct ft2_attachment_s ft2_attachment_t;
#define ft2_oldstyle_map ((ft2_font_map_t*)-1)

typedef float ft2_kernvec[2];
typedef struct ft2_kerning_s
{
	ft2_kernvec kerning[256][256]; /* kerning[left char][right char] */
} ft2_kerning_t;

typedef struct ft2_glyph_tree_s
{
	struct ft2_glyph_tree_s* next[0x10];
	struct glyph_s *endglyph;
} ft2_glyph_tree_t;

typedef struct ft2_font_size_s
{
	float         size;
	// the actual size used in the freetype code
	// by convention, the requested size is the height of the font's bounding box.
	float         intSize;
	int           glyphSize;

	// For quick access, the first 256 glyphs
	// are stored here - but not preloaded!
	struct glyph_s *main_glyphs[256];
	ft2_glyph_tree_t glyphtree;

	// contains the kerning information for the first 256 characters
	// for the other characters, we will lookup the kerning information
	// ft2_kerning_t kerning;
	// safes us the trouble of calculating these over and over again
	double        sfx, sfy;
	// the width_of for the image-font, pixel-snapped for this size
	float         width_of[256];
} ft2_font_size_t;

typedef struct ft2_font_s
{
	char            name[64];
	qboolean        has_kerning;
	// last requested size loaded using Font_SetSize
	float		currentw;
	float		currenth;
	float           ascend;
	float           descend;
	qboolean        image_font; // only fallbacks are freetype fonts

	// TODO: clean this up and do not expose everything.
	
	const unsigned char  *data; // FT2 needs it to stay
	//fs_offset_t     datasize;
	void           *face;

	ft2_font_size_t     font_sizes[MAX_FONT_SIZES];
	int             num_size;

/* DEPRECATED:
	// an unordered array of ordered linked lists of glyph maps for a specific size
	ft2_font_map_t *font_maps[MAX_FONT_SIZES];
	int             num_sizes;
*/

	// attachments
	size_t            attachmentcount;
	ft2_attachment_t *attachments;

	ft2_settings_t *settings;
	// fallback mechanism
	struct ft2_font_s *next;

	// Virtual2Real translated sizes
	float              req_sizes[MAX_FONT_SIZES];
} ft2_font_t;

typedef struct glyph_s
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

	ft2_glyphtex_t *glyphtex;
	// So we don't need to care about what glyphtex looks like outside the font code:
	rtexture_t     *tex;

	// We keep a linked list of glyphs of the same glyphtex
	struct glyph_s *next;
	struct glyph_s *prev;

	// We also need to find the glyph, to be able to remove it
	// from the glyphtree.
	Uchar       treech;
	ft2_font_size_t *ftsize;
} glyph_t;


void            Font_CloseLibrary(void);
void            Font_Init(void);
qboolean        Font_OpenLibrary(void);
ft2_font_t*     Font_Alloc(void);
void            Font_UnloadFont(ft2_font_t *font);
// IndexForSize suggests to change the width and height if a font size is in a reasonable range
// for example, you render at a size of 12.4, and a font of size 12 has been loaded
// in such a case, *outw and *outh are set to 12, which is often a good alternative size
int             Font_IndexForSize(ft2_font_t *font, float size, float *outw, float *outh);
qboolean        Font_LoadFont(const char *name, dp_font_t *dpfnt);
// We use sizeindex to make sure we don't try finding the index anew every time...
qboolean        Font_GetKerning(ft2_font_t *font, int sizeindex, float w, float h, Uchar left, Uchar right, float *outx, float *outy);
glyph_t*        Font_GetGlyph(ft2_font_t *font, int sizeindex, float w, float h, Uchar ch);
float           Font_VirtualToRealSize(float sz);
float           Font_SnapTo(float val, float snapwidth);




//ft2_font_map_t *Font_MapForIndex(ft2_font_t *font, int index);
//qboolean        Font_GetKerningForMap(ft2_font_t *font, int map_index, float w, float h, Uchar left, Uchar right, float *outx, float *outy);
// since this is used on a font_map_t, let's name it FontMap_*
//ft2_font_map_t *FontMap_FindForChar(ft2_font_map_t *start, Uchar ch);
#endif // DP_FREETYPE2_H__
