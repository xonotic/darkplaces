/*
	Copyright (C) 2013  Dale "graphitemaster" Weiler

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

#ifndef WEBP_H
#define WEBP_H

qboolean WEBP_OpenLibrary (void);
void WEBP_CloseLibrary (void);
unsigned char* WEBP_LoadImage_BGRA (const unsigned char *f, int filesize, int *miplevel);
qboolean WEBP_SaveImage_preflipped (const char *filename, int width, int height, qboolean has_alpha, unsigned char *data);

#endif

