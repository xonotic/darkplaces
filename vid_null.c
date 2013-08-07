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

#include "quakedef.h"

void VID_Shutdown(void)
{
}

void VID_SetMouse (qboolean fullscreengrab, qboolean relative, qboolean hidecursor)
{
}

void VID_Finish (void)
{
}

int VID_SetGamma(unsigned short *ramps, int rampsize)
{
	return FALSE;
}

int VID_GetGamma(unsigned short *ramps, int rampsize)
{
	return FALSE;
}

qboolean VID_InitMode(viddef_mode_t *mode)
{
	return false;
}

void *GL_GetProcAddress(const char *name)
{
	return NULL;
}

void Sys_SendKeyEvents(void)
{
}

void VID_BuildJoyState(vid_joystate_t *joystate)
{
}

void IN_Move(void)
{
}
