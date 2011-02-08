//      irc.c
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

#include <pthread.h>
#include <string.h>

#include "quakedef.h"
#include "irc.h"
#include "cvar.h"

cvar_t irc_server = {CVAR_SAVE, "irc_server", "", "IRC server to connect to"};
cvar_t irc_port = {CVAR_SAVE, "irc_port", "6667", "Port of the IRC server"};
cvar_t irc_password = {CVAR_SAVE, "irc_password", "", "IRC server password"};
cvar_t irc_nick = {CVAR_SAVE, "irc_nick", "", "Your nickname to use on IRC. Note: this cvar only defines your prefered nick, do NOT use this to change your nickname while connected, use irc_chnick instead"};
cvar_t irc_connected = {CVAR_READONLY, "irc_connected", "0", "IRC connection state (0 = not connected, 1 = connecting, 2 = connected)"};
cvar_t irc_msgprefix = {CVAR_SAVE, "irc_msgprefix", "^5IRC^0|^7", "What all IRC events will be prefixed with when printed to the console"};
cvar_t irc_chatwindow = {CVAR_SAVE, "irc_chatwindow", "2", "0 = IRC messages will be printed in the console only, 1 = IRC messages will go to the chat window, 2 = Only hilights and private messages will appear in the chat window"};
cvar_t irc_numeric_errorsonly = {CVAR_SAVE, "irc_numeric_errorsonly", "0", "If 1, any numeric event below 400 won't be printed'"};
cvar_t irc_save_target = {CVAR_SAVE, "irc_save_target", "1", "Whether to save target for irc_messagemode or not"};
cvar_t irc_current_nick = {CVAR_READONLY, "irc_current_nick", "", "Holds your current IRC nick"};
cvar_t irc_hilights = {CVAR_SAVE, "irc_hilights", "", "Space separated list of words to hilight"};
cvar_t irc_autochangetarget = {CVAR_SAVE, "irc_autochangetarget", "0", "if 1, will automatically change target for irc_messagemode when a hilight or a private message is recieved, so you can reply instantly. Requires irc_save_target"};
cvar_t irc_watched_channels = {CVAR_SAVE, "irc_watched_channels", "", "Space separated list of watched channels. PRIVMSGs from those channels will be always printed to the chat area, regardless of irc_chatwindow setting"};

irc_session_t *irc_session_global = NULL;
int irc_msgmode;
char last_channel[MAX_INPUTLINE];

//
//  Initialization
//

void CL_Irc_Init(void)
{
    Cvar_RegisterVariable(&irc_server);
    Cvar_RegisterVariable(&irc_port);
    Cvar_RegisterVariable(&irc_password);
    Cvar_RegisterVariable(&irc_nick);
    Cvar_RegisterVariable(&irc_connected);
    Cvar_RegisterVariable(&irc_msgprefix);
    Cvar_RegisterVariable(&irc_chatwindow);
    Cvar_RegisterVariable(&irc_numeric_errorsonly);
    Cvar_RegisterVariable(&irc_save_target);
    Cvar_RegisterVariable(&irc_current_nick);
    Cvar_RegisterVariable(&irc_hilights);
    Cvar_RegisterVariable(&irc_autochangetarget);
    Cvar_RegisterVariable(&irc_watched_channels);
    
    Cmd_AddCommand ("irc_connect", CL_Irc_Connect_f, "Connects you to the IRC server");
    Cmd_AddCommand ("irc_disconnect", CL_Irc_Disconnect_f, "Disconnects you from the IRC server");
    Cmd_AddCommand ("irc_say", CL_Irc_Say_f, "Sends a privmsg to a channel or nick");
    Cmd_AddCommand ("irc_notice", CL_Irc_Notice_f, "Sends a notice to a channel or nick");
    Cmd_AddCommand ("irc_me", CL_Irc_Me_f, "Sends a CTCP ACTION (/me) to a channel or nick");
    Cmd_AddCommand ("irc_join", CL_Irc_Join_f, "Joins an IRC channel");
    Cmd_AddCommand ("irc_part", CL_Irc_Part_f, "Parts an IRC channel");
    Cmd_AddCommand ("irc_names", CL_Irc_Names_f, "Lists users of an IRC channel");
    Cmd_AddCommand ("irc_messagemode", CL_Irc_MessageMode_f, "Interactive prompt for sending IRC messages. Meant to be bound to a key");
    Cmd_AddCommand ("irc_chnick", CL_Irc_ChNick_f, "Changes your nick");
    Cmd_AddCommand ("irc_raw", CL_Irc_Raw_f, "Sends a raw string to the IRC server");
}

//
//  Function that starts IRC main loop in a thread
//

static void IRC_Thread(void *p)
{
    if(irc_run(irc_session_global))
    {
        Con_Printf("%s^1Error: ^7%s\n",
            irc_msgprefix.string,
            irc_strerror(irc_errno(irc_session_global))
        );
        
        CL_Irc_Disconnect_f();
        return;
    }
}

//
//  irc_ console commands callbacks
//

static void CL_Irc_Connect_f(void)
{
    irc_callbacks_t cb;
    pthread_t irc_thread;
    
    switch(irc_connected.integer)
    {
        case 1:
            Con_Printf("%sAlready connecting to %s:%i\n",
                irc_msgprefix.string, 
                irc_server.string,
                irc_port.integer
            );
            return;
        
        case 2:
            Con_Printf("%sAlready connected to %s:%i.\n",
                irc_msgprefix.string,
                irc_server.string,
                irc_port.integer
            );
            return;
    }
    
    if(NOTSET(irc_server))
    {
        Con_Printf("Please set the irc_server variable\n");
        return;
    }
    
    if(NOTSET(irc_nick))
    {
        Con_Printf("Please set the irc_nick variable\n");
        return;
    }
    
    if(NOTSET(irc_port))
    {
        Con_Printf("Please set the irc_port variable\n");
        return;
    }
    
    Cvar_SetQuick(&irc_connected, "1");
    
    memset(&cb, 0, sizeof(cb));
    cb.event_connect = event_connect;
    cb.event_numeric = event_numeric;
    cb.event_privmsg = event_privmsg;
    cb.event_channel = event_channel;
    cb.event_nick    = event_nick;
    cb.event_quit    = event_quit;
    cb.event_join    = event_join;
    cb.event_part    = event_part;
    cb.event_mode    = event_mode;
    cb.event_umode   = event_umode;
    cb.event_topic   = event_topic;
    cb.event_kick    = event_kick;
    cb.event_notice  = event_notice;
    cb.event_invite  = event_invite;

    Cvar_SetQuick(&irc_current_nick, irc_nick.string);

    irc_session_global = irc_create_session(&cb);
    irc_option_set(irc_session_global, LIBIRC_OPTION_STRIPNICKS);
    if(irc_connect(
        irc_session_global,
        irc_server.string,
        irc_port.integer,
        NOTSET(irc_password)? NULL : irc_password.string,
        irc_nick.string,
        "dpirc", "DPIRC user"
    ))
    {   //Connection failed
        Con_Printf("%s^1Connection failed: ^7%s\n",
            irc_msgprefix.string,
            irc_strerror(irc_errno(irc_session_global))
        );
        
        CL_Irc_Disconnect_f();
        return;
    }
    
    pthread_create (&irc_thread, NULL, (void *) &IRC_Thread, NULL);
}

static void CL_Irc_Disconnect_f(void)
{
    if(!irc_connected.integer)
    {
        Con_Printf("%sNot connected\n",
            irc_msgprefix.string
        );
        
        return;
    }
    
    Con_Printf("^1Disconnected from the IRC server\n");
    irc_cmd_quit(irc_session_global, "Disconnected");
    
    irc_destroy_session(irc_session_global);

    Cvar_SetQuick(&irc_connected, "0");
}

static void CL_Irc_Say_Universal_f(void)
{
    int cmdlen, i, j, space;
    const char *cmd, *dest;
    char message[MAX_INPUTLINE];
    qboolean watched;

    if(Cmd_Argc() < 3) switch(irc_msgmode)
    {
        case MSGMODE_PRIVMSG:
            Con_Printf("Usage: irc_say channel_or_nick message\n");
            return;
        case MSGMODE_NOTICE:
            Con_Printf("Usage: irc_notice channel_or_nick message\n");
            return;
        case MSGMODE_ACTION:
            Con_Printf("Usage: irc_me channel_or_nick message\n");
            return;
    }
    
    cmd = Cmd_Args();
    cmdlen = strlen(cmd);
    
    space = 0;
    for(i = 0, j = 0; i < cmdlen; ++i)
    {
        if(space)
        {
            message[j++] = cmd[i];
        }
        else if(cmd[i] == ' ') ++space;
    }
    
    message[j] = '\0';
    
    dest = Cmd_Argv(1);
    
    switch(irc_msgmode)
    {
        case MSGMODE_PRIVMSG:
            irc_cmd_msg(irc_session_global, dest, message);
            watched = Irc_IsWatchedChannel(dest);
            
            if(ISCHANNEL(dest)) Con_Printf("%s%s^3%s^0|^7<^2%s^7> %s\n",
                watched? "\001" : CHATWINDOW,
                irc_msgprefix.string,
                dest,
                irc_current_nick.string,
                message
            ); else Con_Printf("%s%s^1Privmsg to ^2%s^7: ^3%s\n",
                CHATWINDOW_URGENT,
                irc_msgprefix.string,
                dest,
                message
            );
            
            break;
        
        case MSGMODE_NOTICE:
            irc_cmd_notice(irc_session_global, dest, message);
            
            if(!ISCHANNEL(dest)) Con_Printf("%s%sNotice to ^2%s^7: ^9%s\n",
                CHATWINDOW,
                irc_msgprefix.string,
                dest,
                message
            ); else Con_Printf("%s%s^3%s^0|^9-^2%s^9- %s\n",
                CHATWINDOW,
                irc_msgprefix.string,
                dest,
                irc_current_nick.string,
                message
            );
            
            break;
        
        case MSGMODE_ACTION: //to-do
            irc_cmd_me(irc_session_global, dest, message);
            break;
    }
}

static void CL_Irc_Raw_f(void)
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("Usage: irc_raw command\n");
        return;
    }
    
    irc_send_raw(irc_session_global, "%s", Cmd_Args());
}

static void CL_Irc_Say_f(void)
{
    MUSTCONNECT
    irc_msgmode = MSGMODE_PRIVMSG;
    CL_Irc_Say_Universal_f();
}

static void CL_Irc_Notice_f(void)
{
    MUSTCONNECT
    irc_msgmode = MSGMODE_NOTICE;
    CL_Irc_Say_Universal_f();
}

static void CL_Irc_Me_f(void)
{
    MUSTCONNECT
    irc_msgmode = MSGMODE_ACTION;
    CL_Irc_Say_Universal_f();
}

static void CL_Irc_Join_f(void)
{
    int argc = Cmd_Argc();
    const char *channel;
    
    MUSTCONNECT
    
    if(argc < 2)
    {
        Con_Printf("Usage: irc_join channel [key]\n");
        return;
    }
    
    channel = Cmd_Argv(1);
    
    if(argc > 2)
        irc_cmd_join(irc_session_global, channel, Cmd_Argv(2));
    else
        irc_cmd_join(irc_session_global, channel, NULL);
}

static void CL_Irc_Part_f(void)
{
    MUSTCONNECT
    
    if(Cmd_Argc() < 2)
    {
        Con_Printf("Usage: irc_part channel\n");
        return;
    }
    
    irc_cmd_part(irc_session_global, Cmd_Argv(1));
}

static void CL_Irc_Names_f(void)
{
    MUSTCONNECT
    
    if(Cmd_Argc() < 2)
    {
        Con_Printf("Usage: irc_names channel\n");
        return;
    }
    
    irc_cmd_names(irc_session_global, Cmd_Argv(1));
}

void CL_Irc_MessageMode_f(void)
{
    const char *tmp;
    
    key_dest = key_message;
    chat_mode = 2;
    chat_bufferlen = 0;
    chat_buffer[0] = 0;
    
    if(irc_save_target.integer)
    {
        tmp = Irc_GetLastChannel();
        strlcpy(chat_buffer, tmp, sizeof(chat_buffer));
        chat_bufferlen = strlen(chat_buffer);
    }
}

void CL_Irc_ChNick_f(void)
{
    MUSTCONNECT
    
    if(Cmd_Argc() < 2)
    {
        Con_Printf("Usage: irc_chnick nick\n");
        return;
    }
    
    irc_cmd_nick(irc_session_global, Cmd_Argv(1));
}


void Irc_SetLastChannel(const char *value)
{
    strlcpy(last_channel, value, sizeof(last_channel));
}

const char* Irc_GetLastChannel(void)
{
    return last_channel;
}


void Irc_SendMessage(const char *message)
{
    const char* dest = Irc_GetLastChannel();
    qboolean watched = Irc_IsWatchedChannel(dest);
    
    MUSTCONNECT
    
    irc_cmd_msg(irc_session_global, dest, message);
            
    if(ISCHANNEL(dest)) Con_Printf("%s%s^3%s^0|^7<^2%s^7> %s\n",
        watched? "\001" : CHATWINDOW,
        irc_msgprefix.string,
        dest,
        irc_current_nick.string,
        message
    ); else Con_Printf("%s%s^1Privmsg to ^2%s^7: ^3%s\n",
        CHATWINDOW_URGENT,
        irc_msgprefix.string,
        dest,
        message
    );
}

//
//  IRC events
//

IRCEVENT(event_connect)
{
    Cvar_SetQuick(&irc_connected, "2");
    Con_Printf("%sConnected to %s:%i\n",
        irc_msgprefix.string, 
        irc_server.string,
        irc_port.integer
    );
}

IRCNUMEVENT(event_numeric)
{
    //Get our initial nick from the welcome message
    if(event == 001)
        Cvar_SetQuick(&irc_current_nick, params[0]);
    
    if (!irc_numeric_errorsonly.integer || event > 400)
    {
        Con_Printf("%s^3%d ^2%s ^1%s^7 %s %s %s\n",
            irc_msgprefix.string,
            event,
            origin ? origin : "^8(unknown)",
            params[0],
            count > 1 ? params[1] : "",
            count > 2 ? params[2] : "",
            count > 3 ? params[3] : ""
        );
    }
}

IRCEVENT(event_privmsg)
{
    char* msgstr = "";
    
    if(count > 1)
        msgstr = irc_color_strip_from_mirc(params[1]);
    
    UPDATETARGET(origin)
    
    Con_Printf("%s%s^1Privmsg from ^2%s^7: ^3%s\n",
        CHATWINDOW_URGENT,
        irc_msgprefix.string,
        origin,
        msgstr
    );
    
    if(count > 1) free(msgstr);
}


IRCEVENT(event_channel)
{
    char* msgstr = "";
    qboolean watched;
    
    //weird shit
    if(!ISCHANNEL(params[0]))
        return event_privmsg(session, event, origin, params, count);
    
    if(count > 1)
        msgstr = irc_color_strip_from_mirc(params[1]);
    
    watched = Irc_IsWatchedChannel(params[0]);
    
    if(Irc_CheckHilight(msgstr))
    {   
        UPDATETARGET(params[0])
        
        Con_Printf("%s%s^3%s^0|^7<^2%s^7> ^1%s\n",
            watched? "\001" : CHATWINDOW_URGENT,
            irc_msgprefix.string,
            params[0],
            origin,
            msgstr
        );
    }
    else Con_Printf("%s%s^3%s^0|^7<^2%s^7> %s\n",
        watched? "\001" : CHATWINDOW,
        irc_msgprefix.string,
        params[0],
        origin,
        msgstr
    );
    
    if(count > 1) free(msgstr);
}

IRCEVENT(event_nick)
{
    //printf("%sorigin -> %s (%s, %s)\n", origin, params[0], irc_current_nick.string, irc_nick.string);
    //crash!
    //cvar_t dumb;
    //Cvar_SetQuick(&dumb, NULL);
    
    if(!strncmp(origin, irc_current_nick.string, 512)) //Our nick is changed
    {
        Cvar_SetQuick(&irc_current_nick, params[0]);
        
        Con_Printf("%s%s^7Your nick is now ^2%s\n",
            CHATWINDOW,
            irc_msgprefix.string,
            params[0]
        );
        
        return;
    }
    
    Con_Printf("%s%s^2%s^7 is now known as ^2%s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        params[0]
    );
}

IRCEVENT(event_quit)
{
    Con_Printf("%s%s^2%s^7 has quit IRC: ^2%s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        count? params[0] : "^8(quit message missing)"
    );
}

IRCEVENT(event_join)
{
    Con_Printf("%s%s^2%s^7 has joined ^3%s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        params[0]
    );
}

IRCEVENT(event_part)
{
    Con_Printf("%s%s^2%s^7 has left ^3%s^7: %s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        params[0],
        count > 1? params[1] : "^8(part message missing)"
    );
}

IRCEVENT(event_mode)
{
    char paramstring[MAX_INPUTLINE] = "";
    unsigned int i;
    
    for(i = 2; i < count; i++)
    {
        strlcat(paramstring, params[i], sizeof(paramstring));
        strlcat(paramstring, " ", sizeof(paramstring));
    }
    
    Con_Printf("%s%s^2%s^7 set mode on ^3%s^7: %s %s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        params[0],
        params[1],
        paramstring
    );
}

IRCEVENT(event_umode)
{
    Con_Printf("%sUsermode changed for ^2%s^7: %s\n",
        irc_msgprefix.string,
        origin,
        params[0]
    );
}

IRCEVENT(event_topic)
{
    if(count > 1) Con_Printf("%s%s^2%s^7 changed the topic of ^3%s^7: %s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        params[0],
        params[1]
    );
}

IRCEVENT(event_kick)
{
    Con_Printf("%s%s^2%s^7 has kicked ^2%s^7 out of ^3%s^7: %s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        count > 1? params[1] : "^8(nick missing)", //libircclient documentation says params[1] is optinal.
        params[0],
        count > 2? params[2] : "^8(random kick victim! yay!)"
    );
}

IRCEVENT(event_notice)
{
    char* msgstr = "";
    
    if(count > 1)
        msgstr = irc_color_strip_from_mirc(params[1]);
    
    if(!ISCHANNEL(params[0])) Con_Printf("%s%sNotice from ^2%s^7: ^9%s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        msgstr
    ); else Con_Printf("%s%s^3%s^0|^9-^2%s^9- %s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        params[0],
        origin,
        msgstr
    );
    
    if(count > 1) free(msgstr);
}

IRCEVENT(event_invite)
{
    Con_Printf("%s%s^2%s^7 invites you to ^3%s\n",
        CHATWINDOW,
        irc_msgprefix.string,
        origin,
        params[1]
    );
}

//
//  Function that checks if a message contains hilights
//

qboolean Irc_CheckHilight(const char *msg)
{
    int start, idx, len;
    char buffer[512];
    
    if(strcasestr(msg, irc_current_nick.string))
        return TRUE; //Contains our nick
    
    if(NOTSET(irc_hilights))
        return FALSE;
    
    len = strlen(irc_hilights.string);
    start = 0;
    
    for(idx = 0; idx < len; ++idx)
    {
        if(irc_hilights.string[idx] == ' ')
        {
            strlcpy(buffer, irc_hilights.string+start, idx+1);
            if(strcasestr(msg, buffer))
                return TRUE; //Contains a word from hilight list
                
            start = idx+1;
        }
    }
    
    //Catch the final word
    strlcpy(buffer, irc_hilights.string+start, idx+1);
    if(strcasestr(msg, buffer))
        return TRUE; //Contains a word from hilight list
    
    return FALSE;
}

//
//  Checks if channel is watched
//

qboolean Irc_IsWatchedChannel(const char* chan)
{
    int start, idx, len;
    char buffer[512];
    
    if(NOTSET(irc_watched_channels))
        return FALSE;
    
    len = strlen(irc_watched_channels.string);
    start = 0;
    
    for(idx = 0; idx < len; ++idx)
    {
        if(irc_watched_channels.string[idx] == ' ')
        {
            strlcpy(buffer, irc_watched_channels.string+start, idx+1);
            if(strcasestr(chan, buffer))
                return TRUE;
                
            start = idx+1;
        }
    }
    
    //Catch the final channel
    strlcpy(buffer, irc_watched_channels.string+start, idx+1);
    if(strcasestr(chan, buffer))
        return TRUE;
    
    return FALSE;
}

//
//  Functions that should have been in libircclient
//

int irc_cmd_nick(irc_session_t * session, const char *nick)
{
    if(!nick)
    {
        //session->lasterror = LIBIRC_ERR_STATE; Won't compile
        return 1;
    }

    return irc_send_raw(session, "NICK %s", nick);
}
