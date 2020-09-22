#include "quakedef.h"
#include "client.h"
#include "protocol.h"
#include "cl_parse.h"
#include "cdaudio.h"

static void Netmsg_svc_nop(protocol_t *protocol)
{
	//Con_Printf("qw_svc_nop\n");
}

static void Netmsg_svc_disconnect(protocol_t *protocol)
{
	Con_Printf("Server disconnected\n");
	if (cls.demonum != -1)
		CL_NextDemo();
	else
		CL_Disconnect();
	return;
}

static void Netmsg_svc_print(protocol_t *protocol)
{
	char vabuf[1024];
	int i = MSG_ReadByte(&cl_message);
	char *temp = MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring));
	if (CL_ExaminePrintString(temp)) // look for anything interesting like player IP addresses or ping reports
	{
		if (i == 3) // chat
			CSQC_AddPrintText(va(vabuf, sizeof(vabuf), "\1%s", temp));	//[515]: csqc
		else
			CSQC_AddPrintText(temp);
	}
}

static void Netmsg_svc_centerprint(protocol_t *protocol)
{
	CL_VM_Parse_CenterPrint(MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)));	//[515]: csqc
}

static void Netmsg_svc_stufftext(protocol_t *protocol)
{
	CL_VM_Parse_StuffCmd(MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)));	//[515]: csqc
}

static void Netmsg_svc_damage(protocol_t *protocol)
{
	// svc_damage protocol is identical to nq
	V_ParseDamage ();
}

static void Netmsg_svc_serverdata(protocol_t *protocol)
{
	//Cbuf_Execute(); // make sure any stuffed commands are done
	CL_ParseServerInfo();
}

static void Netmsg_svc_setangle(protocol_t *protocol)
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

static void Netmsg_svc_lightstyle(protocol_t *protocol)
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

static void Netmsg_svc_sound(protocol_t *protocol)
{
	CL_ParseStartSoundPacket(false);
}

static void Netmsg_svc_stopsound(protocol_t *protocol)
{
	int i = (unsigned short) MSG_ReadShort(&cl_message);
	S_StopSound(i>>3, i&7);
}

static void Netmsg_svc_updatefrags(protocol_t *protocol)
{
	int i = MSG_ReadByte(&cl_message);
	if (i >= cl.maxclients)
		Host_Error("CL_ParseServerMessage: svc_updatefrags >= cl.maxclients");
	cl.scores[i].frags = (signed short) MSG_ReadShort(&cl_message);
}

static void Netmsg_svc_updateping(protocol_t *protocol)
{
	int i = MSG_ReadByte(&cl_message);
	if (i >= cl.maxclients)
		Host_Error("CL_ParseServerMessage: svc_updateping >= cl.maxclients");
	cl.scores[i].qw_ping = MSG_ReadShort(&cl_message);
}

static void Netmsg_svc_updatepl(protocol_t *protocol)
{
	int i = MSG_ReadByte(&cl_message);
	if (i >= cl.maxclients)
		Host_Error("CL_ParseServerMessage: svc_updatepl >= cl.maxclients");
	cl.scores[i].qw_packetloss = MSG_ReadByte(&cl_message);
}

static void Netmsg_svc_updateentertime(protocol_t *protocol)
{
	int i = MSG_ReadByte(&cl_message);
	if (i >= cl.maxclients)
		Host_Error("CL_ParseServerMessage: svc_updateentertime >= cl.maxclients");
	// seconds ago
	cl.scores[i].qw_entertime = cl.time - MSG_ReadFloat(&cl_message);
}

static void Netmsg_svc_spawnbaseline(protocol_t *protocol)
{
	int i = (unsigned short) MSG_ReadShort(&cl_message);
	if (i < 0 || i >= MAX_EDICTS)
		Host_Error ("CL_ParseServerMessage: svc_spawnbaseline: invalid entity number %i", i);
	if (i >= cl.max_entities)
		CL_ExpandEntities(i);
	CL_ParseBaseline(cl.entities + i, false);
}
static void Netmsg_svc_spawnstatic(protocol_t *protocol)
{
	CL_ParseStatic(false);
}
static void Netmsg_svc_temp_entity(protocol_t *protocol)
{
	if(!CL_VM_Parse_TempEntity())
		CL_ParseTempEntity ();
}

static void Netmsg_svc_killedmonster(protocol_t *protocol)
{
	cl.stats[STAT_MONSTERS]++;
}

static void Netmsg_svc_foundsecret(protocol_t *protocol)
{
	cl.stats[STAT_SECRETS]++;
}

static void Netmsg_svc_updatestat(protocol_t *protocol)
{
	int i = MSG_ReadByte(&cl_message);
	if (i < 0 || i >= MAX_CL_STATS)
		Host_Error ("svc_updatestat: %i is invalid", i);
	cl.stats[i] = MSG_ReadByte(&cl_message);
}

static void Netmsg_svc_updatestatlong(protocol_t *protocol)
{
	int i = MSG_ReadByte(&cl_message);
	if (i < 0 || i >= MAX_CL_STATS)
		Host_Error ("svc_updatestatlong: %i is invalid", i);
	cl.stats[i] = MSG_ReadLong(&cl_message);
}

static void Netmsg_svc_spawnstaticsound(protocol_t *protocol)
{
	CL_ParseStaticSound (false);
}

static void Netmsg_svc_cdtrack(protocol_t *protocol)
{
	cl.cdtrack = cl.looptrack = MSG_ReadByte(&cl_message);
#ifdef CONFIG_CD
	if ( (cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1) )
		CDAudio_Play ((unsigned char)cls.forcetrack, true);
	else
		CDAudio_Play ((unsigned char)cl.cdtrack, true);
#endif
}

static void Netmsg_svc_intermission(protocol_t *protocol)
{
	int i;
	if(!cl.intermission)
		cl.completed_time = cl.time;
	cl.intermission = 1;
	protocol->ReadVector(&cl_message, cl.qw_intermission_origin);
	for (i = 0;i < 3;i++)
		cl.qw_intermission_angles[i] = protocol->ReadAngle(&cl_message);
}

static void Netmsg_svc_finale(protocol_t *protocol)
{
	if(!cl.intermission)
		cl.completed_time = cl.time;
	cl.intermission = 2;
	SCR_CenterPrint(MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)));
}

static void Netmsg_svc_sellscreen(protocol_t *protocol)
{
	Cmd_ExecuteString (&cmd_client, "help", src_local, true);
}

static void Netmsg_svc_smallkick(protocol_t *protocol)
{
	cl.qw_weaponkick = -2;
}
static void Netmsg_svc_bigkick(protocol_t *protocol)
{
	cl.qw_weaponkick = -4;
}

static void Netmsg_svc_muzzleflash(protocol_t *protocol)
{
	int i = (unsigned short) MSG_ReadShort(&cl_message);
	// NOTE: in QW this only worked on clients
	if (i < 0 || i >= MAX_EDICTS)
		Host_Error("CL_ParseServerMessage: svc_spawnbaseline: invalid entity number %i", i);
	if (i >= cl.max_entities)
		CL_ExpandEntities(i);
	cl.entities[i].persistent.muzzleflash = 1.0f;
}

static void Netmsg_svc_updateuserinfo(protocol_t *protocol)
{
	QW_CL_UpdateUserInfo();
}

static void Netmsg_svc_setinfo(protocol_t *protocol)
{
	QW_CL_SetInfo();
}

static void Netmsg_svc_serverinfo(protocol_t *protocol)
{
	QW_CL_ServerInfo();
}

static void Netmsg_svc_download(protocol_t *protocol)
{
	QW_CL_ParseDownload();
}

static void Netmsg_svc_playerinfo(protocol_t *protocol)
{
	int i;
	// slightly kill qw player entities now that we know there is
	// an update of player entities this frame...
	if (!cl.qwplayerupdatereceived)
	{
		cl.qwplayerupdatereceived = true;
		for (i = 1;i < cl.maxclients;i++)
			cl.entities_active[i] = false;
	}
	EntityStateQW_ReadPlayerUpdate();
}

static void Netmsg_svc_nails(protocol_t *protocol)
{
	QW_CL_ParseNails();
}

static void Netmsg_svc_chokecount(protocol_t *protocol)
{
	(void) MSG_ReadByte(&cl_message);
	// FIXME: apply to netgraph
	//for (j = 0;j < i;j++)
	//	cl.frames[(cls.netcon->qw.incoming_acknowledged-1-j)&QW_UPDATE_MASK].receivedtime = -2;
}

static void Netmsg_svc_modellist(protocol_t *protocol)
{
	QW_CL_ParseModelList();
}

static void Netmsg_svc_soundlist(protocol_t *protocol)
{
	QW_CL_ParseSoundList();
}

static void Netmsg_svc_packetentities(protocol_t *protocol)
{
	EntityFrameQW_CL_ReadFrame(false);
	// first update is the final signon stage
	if (cls.signon == SIGNONS - 1)
	{
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}
}

static void Netmsg_svc_deltapacketentities(protocol_t *protocol)
{
	EntityFrameQW_CL_ReadFrame(true);
	// first update is the final signon stage
	if (cls.signon == SIGNONS - 1)
	{
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}
}

static void Netmsg_svc_maxspeed(protocol_t *protocol)
{
	cl.movevars_maxspeed = MSG_ReadFloat(&cl_message);
}

static void Netmsg_svc_entgravity(protocol_t *protocol)
{
	cl.movevars_entgravity = MSG_ReadFloat(&cl_message);
	if (!cl.movevars_entgravity)
		cl.movevars_entgravity = 1.0f;
}

static void Netmsg_svc_setpause(protocol_t *protocol)
{
	cl.paused = MSG_ReadByte(&cl_message) != 0;
#ifdef CONFIG_CD
	if (cl.paused)
		CDAudio_Pause ();
	else
		CDAudio_Resume ();
#endif
	S_PauseGameSounds (cl.paused);
}