/*
Copyright (C) 2026 Xonotic Touch contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.
*/

#include "quakedef.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// Linux input bits (from linux/input-event-codes.h) — avoid depending on kernel headers.
#define VID_KEY_A                 30
#define VID_ABS_MT_POSITION_X     53
#define VID_INPUT_PROP_DIRECT     1

static qbool vid_touch_applying;
static qbool vid_touch_finger_seen;

extern cvar_t vid_touchscreen;
extern cvar_t vid_touchscreen_mode;
extern cvar_t vid_touchscreen_touchonly;
extern cvar_t vid_touchscreen_detected;
extern cvar_t vid_touchscreen_touchonly_detected;

qbool VID_SDL_HasTouchDevices(void);

static qbool vid_strcasestr_has(const char *hay, const char *needle)
{
	size_t nlen, hlen, i, j;

	if (!hay || !needle || !needle[0])
		return false;
	nlen = strlen(needle);
	hlen = strlen(hay);
	if (nlen > hlen)
		return false;
	for (i = 0; i + nlen <= hlen; i++)
	{
		for (j = 0; j < nlen; j++)
		{
			if (tolower((unsigned char)hay[i + j]) != tolower((unsigned char)needle[j]))
				break;
		}
		if (j == nlen)
			return true;
	}
	return false;
}

static qbool vid_name_is_ignored_keyboard(const char *name)
{
	if (!name || !name[0])
		return true;
	if (vid_strcasestr_has(name, "power button"))
		return true;
	if (vid_strcasestr_has(name, "sleep button"))
		return true;
	if (vid_strcasestr_has(name, "lid switch"))
		return true;
	if (vid_strcasestr_has(name, "video bus"))
		return true;
	if (vid_strcasestr_has(name, "gpio-keys"))
		return true;
	if (vid_strcasestr_has(name, "headset"))
		return true;
	if (vid_strcasestr_has(name, "hdmi"))
		return true;
	if (vid_strcasestr_has(name, "sof-hda"))
		return true;
	if (vid_strcasestr_has(name, "consumer control"))
		return true;
	return false;
}

// Parse space-separated hex capability bitmaps from /proc/bus/input/devices.
// Words are printed most-significant first.
static qbool vid_bitmap_has_bit(const char *hex, unsigned bit)
{
	unsigned long words[32];
	int n = 0;
	int word_bits = 32;
	int idx;
	unsigned word_from_low;
	unsigned bit_in_word;
	const char *p = hex;
	char *end;

	memset(words, 0, sizeof(words));
	while (p && *p && n < 32)
	{
		const char *start;

		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if (!*p)
			break;
		start = p;
		words[n] = strtoul(p, &end, 16);
		if (end == p)
			break;
		if ((int)(end - start) > 8)
			word_bits = 64;
		n++;
		p = end;
	}
	if (n == 0)
		return false;

	word_from_low = bit / (unsigned)word_bits;
	bit_in_word = bit % (unsigned)word_bits;
	idx = n - 1 - (int)word_from_low;
	if (idx < 0 || idx >= n)
		return false;
	return (words[idx] & (1UL << bit_in_word)) != 0;
}

static void vid_scan_proc_bus_input(qbool *has_touch, qbool *has_keyboard)
{
	FILE *f;
	char line[512];
	char name[256];
	unsigned prop = 0;
	qbool key_a = false;
	qbool abs_mt = false;

	name[0] = 0;
	f = fopen("/proc/bus/input/devices", "r");
	if (!f)
		return;

	while (fgets(line, sizeof(line), f))
	{
		if (!strncmp(line, "N: Name=\"", 9))
		{
			name[0] = 0;
			sscanf(line, "N: Name=\"%255[^\"]\"", name);
		}
		else if (!strncmp(line, "B: PROP=", 8))
			prop = (unsigned)strtoul(line + 8, NULL, 16);
		else if (!strncmp(line, "B: KEY=", 7))
			key_a = vid_bitmap_has_bit(line + 7, VID_KEY_A);
		else if (!strncmp(line, "B: ABS=", 7))
			abs_mt = vid_bitmap_has_bit(line + 7, VID_ABS_MT_POSITION_X);
		else if (line[0] == '\n' || line[0] == '\r' || line[0] == 0)
		{
			if ((prop & (1u << VID_INPUT_PROP_DIRECT)) || vid_strcasestr_has(name, "touchscreen"))
				*has_touch = true;
			else if (abs_mt && !(prop & 1u) && vid_strcasestr_has(name, "touch"))
				*has_touch = true;
			if (key_a && !vid_name_is_ignored_keyboard(name))
				*has_keyboard = true;
			name[0] = 0;
			prop = 0;
			key_a = false;
			abs_mt = false;
		}
	}
	fclose(f);
}

static int vid_read_chassis_type(void)
{
	FILE *f;
	int type = -1;

	f = fopen("/sys/class/dmi/id/chassis_type", "r");
	if (!f)
		return -1;
	if (fscanf(f, "%d", &type) != 1)
		type = -1;
	fclose(f);
	return type;
}

static qbool vid_os_release_has(const char *needle)
{
	FILE *f;
	char line[512];

	f = fopen("/etc/os-release", "r");
	if (!f)
		return false;
	while (fgets(line, sizeof(line), f))
	{
		if (vid_strcasestr_has(line, needle))
		{
			fclose(f);
			return true;
		}
	}
	fclose(f);
	return false;
}

static qbool vid_is_ubuntu_touch(void)
{
	const char *desktop;

	if (vid_os_release_has("Ubuntu Touch") || vid_os_release_has("UBUNTU_TOUCH")
		|| vid_os_release_has("VARIANT_ID=touch") || vid_os_release_has("lomiri"))
		return true;
#ifndef WIN32
	if (access("/usr/share/ubports", F_OK) == 0)
		return true;
#endif
	desktop = getenv("XDG_CURRENT_DESKTOP");
	if (desktop && (vid_strcasestr_has(desktop, "Lomiri") || vid_strcasestr_has(desktop, "Unity8")))
		return true;
	if (getenv("CLICK_FRAMEWORK") && getenv("CLICK_FRAMEWORK")[0])
		return true;
	return false;
}

static qbool vid_chassis_is_handheld_or_tablet(int chassis)
{
	// SMBIOS chassis: 11 Hand Held, 30 Tablet.
	// 31 Convertible / 32 Detachable still have a keyboard when docked.
	return chassis == 11 || chassis == 30;
}

void VID_DetectTouchHardware(qbool *has_touchscreen, qbool *is_touch_only)
{
	qbool touch = false;
	qbool keyboard = false;
	int chassis;

	if (VID_SDL_HasTouchDevices() || vid_touch_finger_seen)
		touch = true;

	vid_scan_proc_bus_input(&touch, &keyboard);

	chassis = vid_read_chassis_type();
	if (vid_chassis_is_handheld_or_tablet(chassis) && touch)
		keyboard = false;
	if (vid_is_ubuntu_touch())
	{
		touch = true;
		keyboard = false;
	}

#ifdef DP_MOBILETOUCH
	touch = true;
	keyboard = false;
#endif

	*has_touchscreen = touch;
	*is_touch_only = touch && !keyboard;
}

void VID_ApplyTouchscreenMode(void)
{
	qbool has_touch = false;
	qbool touch_only = false;
	int mode;
	int want;

	VID_DetectTouchHardware(&has_touch, &touch_only);

	vid_touch_applying = true;
	Cvar_SetValueQuick(&vid_touchscreen_detected, has_touch ? 1 : 0);
	Cvar_SetValueQuick(&vid_touchscreen_touchonly_detected, touch_only ? 1 : 0);

	mode = vid_touchscreen_mode.integer;
	if (mode <= 0)
		want = 0;
	else if (mode >= 2)
		want = 1;
	else if (vid_touchscreen_touchonly.integer)
		want = touch_only ? 1 : 0;
	else
		want = has_touch ? 1 : 0;

	if (vid_touchscreen.integer != want)
	{
		Cvar_SetValueQuick(&vid_touchscreen, want);
		Con_Printf("Touch controls: %s (mode=%s, hardware touch=%s, touch-only=%s)\n",
			want ? "on" : "off",
			mode <= 0 ? "off" : (mode >= 2 ? "always" : "auto"),
			has_touch ? "yes" : "no",
			touch_only ? "yes" : "no");
	}
	vid_touch_applying = false;
}

void VID_NoteTouchFingerSeen(void)
{
	if (vid_touch_finger_seen)
		return;
	vid_touch_finger_seen = true;
	if (vid_touchscreen_mode.integer == 1 && !vid_touchscreen.integer)
		VID_ApplyTouchscreenMode();
}

void VID_TouchscreenMode_c(cvar_t *var)
{
	(void)var;
	if (vid_touch_applying)
		return;
	VID_ApplyTouchscreenMode();
}

void VID_Touchscreen_c(cvar_t *var)
{
	if (vid_touch_applying)
		return;
	// Direct console / config writes to vid_touchscreen become an explicit mode.
	vid_touch_applying = true;
	if (var->integer)
		Cvar_SetValueQuick(&vid_touchscreen_mode, 2);
	else
		Cvar_SetValueQuick(&vid_touchscreen_mode, 0);
	vid_touch_applying = false;
}

void VID_TouchscreenRescan_f(cmd_state_t *cmd)
{
	(void)cmd;
	VID_ApplyTouchscreenMode();
	Con_Printf("vid_touchscreen_mode %d, vid_touchscreen %d, detected %d, touch-only %d\n",
		vid_touchscreen_mode.integer,
		vid_touchscreen.integer,
		vid_touchscreen_detected.integer,
		vid_touchscreen_touchonly_detected.integer);
}
