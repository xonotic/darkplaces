//      irc.h
//      
//      Copyright 2011 Akari <Akari` @ irc.quakenet.org>
//      
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//      
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.

#include <libircclient/libircclient.h>

#ifndef IRCINCLUDED
#define IRCINCLUDED

static void CL_Irc_Connect_f(void);
static void CL_Irc_Disconnect_f(void);
static void CL_Irc_Say_Universal_f(void);
static void CL_Irc_Notice_f(void);
static void CL_Irc_Me_f(void);
static void CL_Irc_Say_f(void);
static void CL_Irc_Join_f(void);
static void CL_Irc_Part_f(void);
static void CL_Irc_Names_f(void);
static void CL_Irc_MessageMode_f(void);
static void CL_Irc_ChNick_f(void);
static void CL_Irc_Raw_f(void);
void CL_Irc_Init (void);
static void IRC_Thread(void *p);
void Irc_SetLastChannel(const char *value);
const char* Irc_GetLastChannel(void);
void Irc_SendMessage(const char *msg);
int irc_cmd_nick(irc_session_t * session, const char *nick);
qboolean Irc_CheckHilight(const char* msg);
qboolean Irc_IsWatchedChannel(const char* chan);

#define IRCEVENT(funcname) void funcname(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
#define IRCNUMEVENT(funcname) void funcname (irc_session_t *session, unsigned int event, const char *origin, const char **params, unsigned int count)
IRCEVENT(event_connect);
IRCNUMEVENT(event_numeric);
IRCEVENT(event_privmsg);
IRCEVENT(event_channel);
IRCEVENT(event_nick);
IRCEVENT(event_quit);
IRCEVENT(event_join);
IRCEVENT(event_part);
IRCEVENT(event_mode);
IRCEVENT(event_umode);
IRCEVENT(event_topic);
IRCEVENT(event_kick);
IRCEVENT(event_notice);
IRCEVENT(event_invite);
IRCEVENT(event_ctcp_action);
IRCEVENT(event_ctcp_action_priv);

#define MSGMODE_PRIVMSG 0
#define MSGMODE_NOTICE  1
#define MSGMODE_ACTION  2

#define ISCHANNEL(c) (c[0] == '#' || c[0] == '&')
#define CHATWINDOW (irc_chatwindow.integer == 1? "\001" : "")
#define CHATWINDOW_URGENT (irc_chatwindow.integer? "\001" : "")
#define MUSTCONNECT if(irc_connected.integer != 2) { Con_Printf("Not connected to an IRC server\n"); return; }
#define UPDATETARGET(c) if(irc_autochangetarget.integer) Irc_SetLastChannel(c);
#define NOTSET(cvar) (!strlen((cvar).string))

extern char *strcasestr (__const char*, __const char*);

#endif
