#include "quakedef.h"
#include "client.h"
#include "protocol.h"
#include "cl_parse.h"
#include "cdaudio.h"

static void Netmsg_svc_nop (protocol_t *protocol)
{
	if (cls.signon < SIGNONS)
		Con_Print("<-- server to client keepalive\n");

}

static void Netmsg_svc_disconnect (protocol_t *protocol)
{
	Con_Printf ("Server disconnected\n");
	if (cls.demonum != -1)
		CL_NextDemo ();
	else
		CL_Disconnect ();
}

static void Netmsg_svc_updatestat (protocol_t *protocol)
{
	int i = MSG_ReadByte(&cl_message);
	if (i < 0 || i >= MAX_CL_STATS)
		Host_Error ("svc_updatestat: %i is invalid", i);
	cl.stats[i] = MSG_ReadLong(&cl_message);
}

static void Netmsg_svc_version (protocol_t *protocol)		// [int] server version
{
	int i = MSG_ReadLong(&cl_message);
	protocol = Protocol_ForNumber(i);
	if (!protocol)
		Host_Error("CL_ParseServerMessage: Server is unrecognized protocol number (%i)", i);
	// hack for unmarked Nehahra movie demos which had a custom protocol
	// FIXME: Remove hack once we get some .fmf-like system going?
	if (protocol->num == PROTOCOL_QUAKE && cls.demoplayback && gamemode == GAME_NEHAHRA)
		protocol = &protocol_nehahramovie;
	cls.protocol = protocol;
}

static void Netmsg_svc_setview (protocol_t *protocol)		// [short] entity number
{
	cl.viewentity = (unsigned short)MSG_ReadShort(&cl_message);
	if (cl.viewentity >= MAX_EDICTS)
		Host_Error("svc_setview >= MAX_EDICTS");
	if (cl.viewentity >= cl.max_entities)
		CL_ExpandEntities(cl.viewentity);
	// LadyHavoc: assume first setview recieved is the real player entity
	if (!cl.realplayerentity)
		cl.realplayerentity = cl.viewentity;
	// update cl.playerentity to this one if it is a valid player
	if (cl.viewentity >= 1 && cl.viewentity <= cl.maxclients)
		cl.playerentity = cl.viewentity;

}

static void Netmsg_svc_sound (protocol_t *protocol)			// <see code>
{
	CL_ParseStartSoundPacket(protocol == &protocol_nehahrabjp2 || protocol == &protocol_nehahrabjp3 ? true : false);
}

static void Netmsg_svc_time (protocol_t *protocol)			// [float] server time
{
	CL_NetworkTimeReceived(MSG_ReadFloat(&cl_message));
}

static void Netmsg_svc_print (protocol_t *protocol)			// [string] null terminated string
{
	const char *temp = MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring));
	if (CL_ExaminePrintString(temp)) // look for anything interesting like player IP addresses or ping reports
		CSQC_AddPrintText(temp);	//[515]: csqc
}

static void Netmsg_svc_stufftext (protocol_t *protocol)		// [string] stuffed into client's console buffer
{											// the string should be \n terminated
	qbool strip_pqc;
	const char *temp = MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring));
	/* if(utf8_enable.integer)
	{
		strip_pqc = true;
		// we can safely strip and even
		// interpret these in utf8 mode
	}
	else */ switch(cls.protocol->num)
	{
		case PROTOCOL_QUAKE:
			// maybe add other protocols if
			// so desired, but not DP7
			strip_pqc = true;
			break;
		default:
			// ProQuake does not support
			// these protocols
			strip_pqc = false;
			break;
	}
	if(strip_pqc)
	{
		// skip over ProQuake messages,
		// TODO actually interpret them
		// (they are sbar team score
		// updates), see proquake cl_parse.c
		if(*temp == 0x01)
		{
			++temp;
			while(*temp >= 0x01 && *temp <= 0x1F)
				++temp;
		}
	}
	CL_VM_Parse_StuffCmd(temp);	//[515]: csqc

}

static void Netmsg_svc_setangle (protocol_t *protocol)		// [vec3] set the view angle to this absolute value
{
	int i;
	for (i=0 ; i<3 ; i++)
		cl.viewangles[i] = protocol->ReadAngle(&cl_message);
	if (!cls.demoplayback)
	{
		cl.fixangle[0] = true;
		VectorCopy(cl.viewangles, cl.mviewangles[0]);
		// disable interpolation if this is new
		if (!cl.fixangle[1])
			VectorCopy(cl.viewangles, cl.mviewangles[1]);
	}
}

static void Netmsg_svc_serverinfo (protocol_t *protocol)		// [int] version
												// [string] signon string
												// [string]..[0]model cache [string]...[0]sounds cache
												// [string]..[0]item cache
{
	CL_ParseServerInfo ();
}

static void Netmsg_svc_lightstyle (protocol_t *protocol)		// [byte] [string]
{
	int i = MSG_ReadByte(&cl_message);
	if (i >= cl.max_lightstyle)
	{
		Con_Printf ("svc_lightstyle >= MAX_LIGHTSTYLES");
		return;
	}
	strlcpy (cl.lightstyle[i].map,  MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)), sizeof (cl.lightstyle[i].map));
	cl.lightstyle[i].map[MAX_STYLESTRING - 1] = 0;
	cl.lightstyle[i].length = (int)strlen(cl.lightstyle[i].map);
}

static void Netmsg_svc_updatename (protocol_t *protocol)		// [byte] [string]
{
	int i = MSG_ReadByte(&cl_message);
	if (i >= cl.maxclients)
		Host_Error ("CL_ParseServerMessage: svc_updatename >= cl.maxclients");
	strlcpy (cl.scores[i].name, MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)), sizeof (cl.scores[i].name));
}

static void Netmsg_svc_updatefrags (protocol_t *protocol)	// [byte] [short]
{
	int i = MSG_ReadByte(&cl_message);
	if (i >= cl.maxclients)
		Host_Error ("CL_ParseServerMessage: svc_updatefrags >= cl.maxclients");
	cl.scores[i].frags = (signed short) MSG_ReadShort(&cl_message);
}

static void Netmsg_svc_clientdata (protocol_t *protocol)		// <shortbits + data>
{
	CL_ParseClientdata();
}

static void Netmsg_svc_stopsound (protocol_t *protocol)		// <see code>
{
	int i = (unsigned short) MSG_ReadShort(&cl_message);
	S_StopSound(i>>3, i&7);
}

static void Netmsg_svc_updatecolors (protocol_t *protocol)	// [byte] [byte]
{
	int i = MSG_ReadByte(&cl_message);
	if (i >= cl.maxclients)
		Host_Error ("CL_ParseServerMessage: svc_updatecolors >= cl.maxclients");
	cl.scores[i].colors = MSG_ReadByte(&cl_message);

}

static void Netmsg_svc_particle (protocol_t *protocol)		// [vec3] <variable>
{
	CL_ParseParticleEffect ();
}

static void Netmsg_svc_damage (protocol_t *protocol)			// [byte] impact [byte] blood [vec3] from
{
	V_ParseDamage ();
}

static void Netmsg_svc_spawnstatic (protocol_t *protocol)
{
	CL_ParseStatic (false);
}

static void Netmsg_svc_spawnbaseline (protocol_t *protocol)
{
	int i = (unsigned short) MSG_ReadShort(&cl_message);
	if (i < 0 || i >= MAX_EDICTS)
		Host_Error ("CL_ParseServerMessage: svc_spawnbaseline: invalid entity number %i", i);
	if (i >= cl.max_entities)
		CL_ExpandEntities(i);
	CL_ParseBaseline (cl.entities + i, false);
}

static void Netmsg_svc_temp_entity (protocol_t *protocol)		// <variable>
{
	if(!CL_VM_Parse_TempEntity())
		CL_ParseTempEntity ();	
}

static void Netmsg_svc_setpause (protocol_t *protocol)
{
	cl.paused = MSG_ReadByte(&cl_message) != 0;
	if (cl.paused)
		CDAudio_Pause ();
	else
		CDAudio_Resume ();
	S_PauseGameSounds (cl.paused);
}

static void Netmsg_svc_signonnum (protocol_t *protocol)
{
	int i = MSG_ReadByte(&cl_message);
	// LadyHavoc: it's rude to kick off the client if they missed the
	// reconnect somehow, so allow signon 1 even if at signon 1
	if (i <= cls.signon && i != 1)
		Host_Error ("Received signon %i when at %i", i, cls.signon);
	cls.signon = i;
	CL_SignonReply ();
}

static void Netmsg_svc_centerprint (protocol_t *protocol)
{
	CL_VM_Parse_CenterPrint(MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)));	//[515]: csqc
}

static void Netmsg_svc_killedmonster (protocol_t *protocol)
{
	cl.stats[STAT_MONSTERS]++;
}

static void Netmsg_svc_foundsecret (protocol_t *protocol)
{
	cl.stats[STAT_SECRETS]++;
}

static void Netmsg_svc_spawnstaticsound (protocol_t *protocol)
{
	CL_ParseStaticSound (protocol == &protocol_nehahrabjp2 || protocol == &protocol_nehahrabjp3 ? true : false);
}

static void Netmsg_svc_intermission (protocol_t *protocol)
{
	if(!cl.intermission)
		cl.completed_time = cl.time;
	cl.intermission = 1;
	CL_VM_UpdateIntermissionState(cl.intermission);
}

static void Netmsg_svc_finale (protocol_t *protocol)			// [string] music [string] text
{
	if(!cl.intermission)
		cl.completed_time = cl.time;
	cl.intermission = 2;
	CL_VM_UpdateIntermissionState(cl.intermission);
	SCR_CenterPrint(MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)));
}

static void Netmsg_svc_cdtrack (protocol_t *protocol)			// [byte] track [byte] looptrack
{
	cl.cdtrack = MSG_ReadByte(&cl_message);
	cl.looptrack = MSG_ReadByte(&cl_message);
	if ( (cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1) )
		CDAudio_Play ((unsigned char)cls.forcetrack, true);
	else
		CDAudio_Play ((unsigned char)cl.cdtrack, true);

}

static void Netmsg_svc_sellscreen (protocol_t *protocol)
{
	Cmd_ExecuteString(&cmd_client, "help", src_local, true);
}

static void Netmsg_svc_cutscene (protocol_t *protocol)
{
	if(!cl.intermission)
		cl.completed_time = cl.time;
	cl.intermission = 3;
	CL_VM_UpdateIntermissionState(cl.intermission);
	SCR_CenterPrint(MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)));
}

#define NETMSG_BASENQ_SVC \
	{"svc_bad", NULL}, \
	{"svc_nop", Netmsg_svc_nop}, \
	{"svc_disconnect", Netmsg_svc_disconnect}, \
	{"svc_updatestat", Netmsg_svc_updatestat}, \
	{"svc_version", Netmsg_svc_version}, \
	{"svc_setview", Netmsg_svc_setview}, \
	{"svc_sound", Netmsg_svc_sound}, \
	{"svc_time", Netmsg_svc_time}, \
	{"svc_print", Netmsg_svc_print}, \
	{"svc_stufftext", Netmsg_svc_stufftext}, \
	{"svc_setangle", Netmsg_svc_setangle}, \
	{"svc_serverinfo", Netmsg_svc_serverinfo}, \
	{"svc_lightstyle", Netmsg_svc_lightstyle}, \
	{"svc_updatename", Netmsg_svc_updatename}, \
	{"svc_updatefrags", Netmsg_svc_updatefrags}, \
	{"svc_clientdata", Netmsg_svc_clientdata}, \
	{"svc_stopsound", Netmsg_svc_stopsound}, \
	{"svc_updatecolors", Netmsg_svc_updatecolors}, \
	{"svc_particle", Netmsg_svc_particle}, \
	{"svc_damage",Netmsg_svc_damage}, \
	{"svc_spawnstatic", Netmsg_svc_spawnstatic}, \
	{"OBSOLETE svc_spawnbinary", NULL}, \
	{"svc_spawnbaseline", Netmsg_svc_spawnbaseline}, \
	{"svc_temp_entity", Netmsg_svc_temp_entity}, \
	{"svc_setpause", Netmsg_svc_setpause}, \
	{"svc_signonnum", Netmsg_svc_signonnum}, \
	{"svc_centerprint", Netmsg_svc_centerprint}, \
	{"svc_killedmonster", Netmsg_svc_killedmonster}, \
	{"svc_foundsecret", Netmsg_svc_foundsecret}, \
	{"svc_spawnstaticsound", Netmsg_svc_spawnstaticsound}, \
	{"svc_intermission", Netmsg_svc_intermission}, \
	{"svc_finale", Netmsg_svc_finale}, \
	{"svc_cdtrack", Netmsg_svc_cdtrack}, \
	{"svc_sellscreen", Netmsg_svc_sellscreen}, \
	{"svc_cutscene", Netmsg_svc_cutscene}
