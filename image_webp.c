/*
	Copyright (C) 2013 Dale "graphitemaster" Weiler

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	(comment)
	* I Dale Weiler allow this code to be relicensed if required.
	* The GPL is just for compatability with darkplaces. If in the
	* distant future this code needs to be relicensed for what ever
	* reason, I herby allow it, no questions asked. Consider this
	* public domain.
	(endcomment)
*/

#include "quakedef.h"
#include "image.h"
#include "image_webp.h"

static int            (*qwebp_get_info)          (const unsigned char *, size_t, int *, int *);
static unsigned char *(*qwebp_decode_bgra_into)  (const unsigned char *, size_t, unsigned char *, size_t, int);
static size_t         (*qwebp_encode_rgb)        (const unsigned char *, int, int, int, float, unsigned char **);
static size_t         (*qwebp_encode_rgba)       (const unsigned char *, int, int, int, float, unsigned char **);

static dllfunction_t webpfuncs[] =
{
	{"WebPGetInfo",         (void **) &qwebp_get_info},
	{"WebPDecodeBGRAInto",  (void **) &qwebp_decode_bgra_into},
	{"WebPEncodeRGB",       (void **) &qwebp_encode_rgb},
	{"WebPEncodeRGBA",      (void **) &qwebp_encode_rgba},
	{NULL, NULL}
};

dllhandle_t webp_dll = NULL;

qboolean WEBP_OpenLibrary (void)
{

	// anything older than 2 doesn't have WebPGetInfo
	const char* dllnames [] =
	{
#if WIN32
		"libwebp-4.dll", // always search newest version
		"libwebp-3.dll",
		"libwebp-2.dll", // this one ships with SDL2
		"libwebp_a.dll", // this one only ships with old SDL releases
#elif defined(MACOSX)
		"libwebp.dylib", // no versions for OSX
#else
		"libwebp.so.4",
		"libwebp.so.3",
		"libwebp.so.2",
		"libwebp.so.0",
		"libwebp.so",
#endif
		NULL
	};

	if (webp_dll)
		return true;

	if(!Sys_LoadLibrary (dllnames, &webp_dll, webpfuncs))
		return false;

	return true;
}

void WEBP_CloseLibrary (void)
{
	Sys_UnloadLibrary (&webp_dll);
}

unsigned char *WEBP_LoadImage_BGRA (const unsigned char *raw, int filesize, int *miplevel)
{
	unsigned char *data = NULL;

	if (!webp_dll)
		return NULL;

	if (!qwebp_get_info(raw, filesize, &image_width, &image_height))
		return NULL;

	if ((data = (unsigned char *)Mem_Alloc(tempmempool, image_width * image_height * 4)))
	{
		if (!qwebp_decode_bgra_into(raw, filesize, data, 4 * image_height, 4))
		{
			Con_Printf("WEBP_LoadImage : failed decode\n");
			Mem_Free(data);
			return NULL;
		}
		return data;
	}
	
	Con_Printf("WEBP_LoadImage : not enough memory\n");
	return NULL;
}


qboolean WEBP_SaveImage_preflipped (const char *filename, int width, int height, qboolean has_alpha, unsigned char *data)
{
	qfile_t       *file   = NULL;
	unsigned char *memory = NULL;
	unsigned int   wrote  = 0;
	
	size_t (*encode)(const unsigned char *, int, int, int, float, unsigned char **) = (has_alpha)
		? qwebp_encode_rgba
		: qwebp_encode_rgb;
		

	if (!(wrote = encode(data, width, height, 4, scr_screenshot_webp_quality.value * 100, &memory)))
		return false;


	if (!(file = FS_OpenRealFile(filename, "wb", true)))
	{
		free(memory);
		return false;
	}

	FS_Write(file, memory, wrote);
	FS_Close(file);

	free(memory);
	return true;
}
