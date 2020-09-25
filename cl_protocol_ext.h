#include "cl_protocol_basenq.h"

static void Netmsg_svc_showlmp (protocol_t *protocol)	// [string] iconlabel [string] lmpfile [short] x [short] y
{
	if (gamemode == GAME_TENEBRAE)
	{
		// particle effect
		protocol->ReadCoord(&cl_message);
		protocol->ReadCoord(&cl_message);
		protocol->ReadCoord(&cl_message);
		(void) MSG_ReadByte(&cl_message);
		MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring));
	}
	else
		SHOWLMP_decodeshow();
}

static void Netmsg_svc_hidelmp (protocol_t *protocol)	// [string] iconlabel
{
	if (gamemode == GAME_TENEBRAE)
	{
		// repeating particle effect
		protocol->ReadCoord(&cl_message);
		protocol->ReadCoord(&cl_message);
		protocol->ReadCoord(&cl_message);
		protocol->ReadCoord(&cl_message);
		protocol->ReadCoord(&cl_message);
		protocol->ReadCoord(&cl_message);
		(void) MSG_ReadByte(&cl_message);
		MSG_ReadLong(&cl_message);
		MSG_ReadLong(&cl_message);
		MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring));
	}
	else
		SHOWLMP_decodehide();
}

static void Netmsg_svc_skybox (protocol_t *protocol) // [string] skyname
{
	R_SetSkyBox(MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring)));
}

static void Netmsg_svc_downloaddata (protocol_t *protocol) //				50		// [int] start [short] size [variable length] data
{
	CL_ParseDownload();
}

static void Netmsg_svc_updatestatubyte (protocol_t *protocol) //			51		// [byte] stat [byte] value
{
	int i = MSG_ReadByte(&cl_message);
	if (i < 0 || i >= MAX_CL_STATS)
		Host_Error ("svc_updatestat: %i is invalid", i);
	cl.stats[i] = MSG_ReadByte(&cl_message);
}

static void Netmsg_svc_effect (protocol_t *protocol) //			52		// [vector] org [byte] modelindex [byte] startframe [byte] framecount [byte] framerate
{
	CL_ParseEffect ();
}

static void Netmsg_svc_effect2 (protocol_t *protocol) //			53		// [vector] org [short] modelindex [short] startframe [byte] framecount [byte] framerate
{
	CL_ParseEffect2 ();
}

// FIXME: Lazy
static void Netmsg_svc_precache (protocol_t *protocol) //			54		// short soundindex instead of byte
{
	if (protocol->num == PROTOCOL_DARKPLACES1 || protocol->num == PROTOCOL_DARKPLACES2 || protocol->num == PROTOCOL_DARKPLACES3)
	{
		// was svc_sound2 in protocols 1, 2, 3, removed in 4, 5, changed to svc_precache in 6
		CL_ParseStartSoundPacket(true);
	}
	else
	{
		// was svc_sound2 in protocols 1, 2, 3, removed in 4, 5, changed to svc_precache in 6
		int i = (unsigned short)MSG_ReadShort(&cl_message);
		char *s = MSG_ReadString(&cl_message, cl_readstring, sizeof(cl_readstring));
		if (i < 32768)
		{
			if (i >= 1 && i < MAX_MODELS)
			{
				dp_model_t *model = Mod_ForName(s, false, false, s[0] == '*' ? cl.model_name[1] : NULL);
				if (!model)
					Con_DPrintf("svc_precache: Mod_ForName(\"%s\") failed\n", s);
				cl.model_precache[i] = model;
			}
			else
				Con_Printf("svc_precache: index %i outside range %i...%i\n", i, 1, MAX_MODELS);
		}
		else
		{
			i -= 32768;
			if (i >= 1 && i < MAX_SOUNDS)
			{
				sfx_t *sfx = S_PrecacheSound (s, true, true);
				if (!sfx && snd_initialized.integer)
					Con_DPrintf("svc_precache: S_PrecacheSound(\"%s\") failed\n", s);
				cl.sound_precache[i] = sfx;
			}
			else
				Con_Printf("svc_precache: index %i outside range %i...%i\n", i, 1, MAX_SOUNDS);
		}
	}
}

static void Netmsg_svc_spawnbaseline2 (protocol_t *protocol) //	55		// short modelindex instead of byte
{
	int i = (unsigned short) MSG_ReadShort(&cl_message);
	if (i < 0 || i >= MAX_EDICTS)
		Host_Error ("CL_ParseServerMessage: svc_spawnbaseline2: invalid entity number %i", i);
	if (i >= cl.max_entities)
		CL_ExpandEntities(i);
	CL_ParseBaseline (cl.entities + i, true);
}

static void Netmsg_svc_spawnstatic2 (protocol_t *protocol) //		56		// short modelindex instead of byte
{
	CL_ParseStatic (true);
}

static void Netmsg_svc_entities (protocol_t *protocol) //			57		// [int] deltaframe [int] thisframe [float vector] eye [variable length] entitydata
{
	if (cls.signon == SIGNONS - 1)
	{
		// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	protocol->ReadFrame();
}

static void Netmsg_svc_csqcentities (protocol_t *protocol) //		58		// [short] entnum [variable length] entitydata ... [short] 0x0000
{
	CSQC_ReadEntities();
}

static void Netmsg_svc_spawnstaticsound2 (protocol_t *protocol) //	59		// [coord3] [short] samp [byte] vol [byte] aten
{
	CL_ParseStaticSound (true);
}

static void Netmsg_svc_trailparticles (protocol_t *protocol) //	60		// [short] entnum [short] effectnum [vector] start [vector] end
{
	CL_ParseTrailParticles();
}

static void Netmsg_svc_pointparticles (protocol_t *protocol) //	61		// [short] effectnum [vector] start [vector] velocity [short] count
{
	CL_ParsePointParticles();
}

static void Netmsg_svc_pointparticles1 (protocol_t *protocol) //	62		// [short] effectnum [vector] start, same as Netmsg_svc_pointparticles except velocity is zero and count is 1
{
	CL_ParsePointParticles1();
}
#define NETMSG_DPEXT_SVC \
	NETMSG_BASENQ_SVC, \
	{"svc_showlmp", Netmsg_svc_showlmp}, \
	{"svc_hidelmp", Netmsg_svc_hidelmp}, \
	{"svc_skybox", Netmsg_svc_skybox}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"", NULL}, \
	{"svc_downloaddata", Netmsg_svc_downloaddata}, \
	{"svc_updatestatubyte", Netmsg_svc_updatestatubyte}, \
	{"svc_effect", Netmsg_svc_effect}, \
	{"svc_effect2", Netmsg_svc_effect2}, \
	{"svc_sound2", Netmsg_svc_precache}, \
	{"svc_spawnbaseline2", Netmsg_svc_spawnbaseline2}, \
	{"svc_spawnstatic2", Netmsg_svc_spawnstatic2}, \
	{"svc_entities", Netmsg_svc_entities}, \
	{"svc_csqcentities", Netmsg_svc_csqcentities}, \
	{"svc_spawnstaticsound2", Netmsg_svc_spawnstaticsound2}, \
	{"svc_trailparticles", Netmsg_svc_trailparticles}, \
	{"svc_pointparticles", Netmsg_svc_pointparticles}, \
	{"svc_pointparticles1", Netmsg_svc_pointparticles1}
