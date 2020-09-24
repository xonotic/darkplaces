#include "quakedef.h"

// this is 88 bytes (must match entity_state_t in protocol.h)
entity_state_t defaultstate =
{
	// ! means this is not sent to client
	0,//double time; // ! time this state was built (used on client for interpolation)
	{0,0,0},//float netcenter[3]; // ! for network prioritization, this is the center of the bounding box (which may differ from the origin)
	{0,0,0},//float origin[3];
	{0,0,0},//float angles[3];
	0,//int effects;
	0,//unsigned int customizeentityforclient; // !
	0,//unsigned short number; // entity number this state is for
	0,//unsigned short modelindex;
	0,//unsigned short frame;
	0,//unsigned short tagentity;
	0,//unsigned short specialvisibilityradius; // ! larger if it has effects/light
	0,//unsigned short viewmodelforclient; // !
	0,//unsigned short exteriormodelforclient; // ! not shown if first person viewing from this entity, shown in all other cases
	0,//unsigned short nodrawtoclient; // !
	0,//unsigned short drawonlytoclient; // !
	0,//unsigned short traileffectnum;
	{0,0,0,0},//unsigned short light[4]; // color*256 (0.00 to 255.996), and radius*1
	ACTIVE_NOT,//unsigned char active; // true if a valid state
	0,//unsigned char lightstyle;
	0,//unsigned char lightpflags;
	0,//unsigned char colormap;
	0,//unsigned char skin; // also chooses cubemap for rtlights if lightpflags & LIGHTPFLAGS_FULLDYNAMIC
	255,//unsigned char alpha;
	16,//unsigned char scale;
	0,//unsigned char glowsize;
	254,//unsigned char glowcolor;
	0,//unsigned char flags;
	0,//unsigned char internaleffects; // INTEF_FLAG1QW and so on
	0,//unsigned char tagindex;
	{32, 32, 32},//unsigned char colormod[3];
	{32, 32, 32},//unsigned char glowmod[3];
};

// LadyHavoc: I own protocol ranges 96, 97, 3500-3599

protocol_t protocol_netquake =
{
	.name = "QUAKE",
	.num = 15,
	.max_edicts = 640,
	.WriteCoord = MSG_WriteCoord13i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector13i,
	.ReadCoord = MSG_ReadCoord13i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector13i,
	.svc = &netmsg_nq_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_quakedp =
{
	.name = "QUAKEDP",
	.num = 15,
	.max_edicts = 2048,
	.WriteCoord = MSG_WriteCoord13i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector13i,
	.ReadCoord = MSG_ReadCoord13i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector13i,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_quakeworld =
{
	.name = "QW",
	.num = 28,
	.WriteCoord = MSG_WriteCoord13i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector13i,
	.ReadCoord = MSG_ReadCoord13i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector13i,
	.svc = &netmsg_qw_svc,
	.clc = NULL
};

protocol_t protocol_dpp1 =
{
	.name = "DP1",
	.num = 96,
	.max_edicts = MAX_EDICTS,
	.WriteCoord = MSG_WriteCoord32f,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector32f,
	.ReadCoord = MSG_ReadCoord32f,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector32f,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_dpp2 =
{
	.name = "DP2",
	.num = 97,
	.max_edicts = MAX_EDICTS,
	.WriteCoord = MSG_WriteCoord16i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector16i,
	.ReadCoord = MSG_ReadCoord16i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector16i,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_dpp3 =
{
	.name = "DP3",
	.num = 3500,
	.max_edicts = MAX_EDICTS,
	.WriteCoord = MSG_WriteCoord16i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector16i,
	.ReadCoord = MSG_ReadCoord16i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector16i,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_dpp4 =
{
	.name = "DP4",
	.num = 3501,
	.max_edicts = MAX_EDICTS,
	.WriteCoord = MSG_WriteCoord16i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector16i,
	.ReadCoord = MSG_ReadCoord16i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector16i,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_dpp5 =
{
	.name = "DP5",
	.num = 3502,
	.max_edicts = MAX_EDICTS,
	.WriteCoord = MSG_WriteCoord32f,
	.WriteAngle = MSG_WriteAngle16i,
	.WriteVector = MSG_WriteVector32f,
	.ReadCoord = MSG_ReadCoord32f,
	.ReadAngle = MSG_ReadAngle16i,
	.ReadVector = MSG_ReadVector32f,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_dpp6 =
{
	.name = "DP6",
	.num = 3503,
	.max_edicts = MAX_EDICTS,
	.WriteCoord = MSG_WriteCoord32f,
	.WriteAngle = MSG_WriteAngle16i,
	.WriteVector = MSG_WriteVector32f,
	.ReadCoord = MSG_ReadCoord32f,
	.ReadAngle = MSG_ReadAngle16i,
	.ReadVector = MSG_ReadVector32f,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_dpp7 =
{
	.name = "DP7",
	.num = 3504,
	.max_edicts = MAX_EDICTS,
	.WriteCoord = MSG_WriteCoord32f,
	.WriteAngle = MSG_WriteAngle16i,
	.WriteVector = MSG_WriteVector32f,
	.ReadCoord = MSG_ReadCoord32f,
	.ReadAngle = MSG_ReadAngle16i,
	.ReadVector = MSG_ReadVector32f,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc

};

protocol_t protocol_nehahramovie =
{
	.name = "NEHAHRAMOVIE",
	.num = 250,
	.max_edicts = 2048,
	.WriteCoord = MSG_WriteCoord13i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector13i,
	.ReadCoord = MSG_ReadCoord13i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector13i,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_nehahrabjp =
{
	.name = "NEHAHRABJP",
	.num = 10000,
	.max_edicts = 4096,
	.WriteCoord = MSG_WriteCoord13i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector13i,
	.ReadCoord = MSG_ReadCoord13i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector13i,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_nehahrabjp2 =
{
	.name = "NEHAHRABJP2",
	.num = 10001,
	.max_edicts = 4096,
	.WriteCoord = MSG_WriteCoord13i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector13i,
	.ReadCoord = MSG_ReadCoord13i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector13i,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t protocol_nehahrabjp3 =
{
	.name = "NEHAHRABJP3",
	.num = 10002,
	.max_edicts = 4096,
	.WriteCoord = MSG_WriteCoord13i,
	.WriteAngle = MSG_WriteAngle8i,
	.WriteVector = MSG_WriteVector13i,
	.ReadCoord = MSG_ReadCoord13i,
	.ReadAngle = MSG_ReadAngle8i,
	.ReadVector = MSG_ReadVector13i,
	.svc = &netmsg_dpext_svc,
	.clc = &netmsg_base_clc
};

protocol_t *protocols[] =
{
	&protocol_netquake,
	&protocol_quakedp,
	&protocol_quakeworld,
	&protocol_dpp1,
	&protocol_dpp2,
	&protocol_dpp3,
	&protocol_dpp4,
	&protocol_dpp5,
	&protocol_dpp6,
	&protocol_dpp7,
	&protocol_nehahramovie,
	&protocol_nehahrabjp,
	&protocol_nehahrabjp2,
	&protocol_nehahrabjp3,
	NULL
};

void Protocol_UpdateClientStats(const int *stats)
{
	int i;
	// update the stats array and set deltabits for any changed stats
	for (i = 0;i < MAX_CL_STATS;i++)
	{
		if (host_client->stats[i] != stats[i])
		{
			host_client->statsdeltabits[i >> 3] |= 1 << (i & 7);
			host_client->stats[i] = stats[i];
		}
	}
}

// only a few stats are within the 32 stat limit of Quake, and most of them
// are sent every frame in svc_clientdata messages, so we only send the
// remaining ones here
static const int sendquakestats[] =
{
// quake did not send these secrets/monsters stats in this way, but doing so
// allows a mod to increase STAT_TOTALMONSTERS during the game, and ensures
// that STAT_SECRETS and STAT_MONSTERS are always correct (even if a client
// didn't receive an svc_foundsecret or svc_killedmonster), which may be most
// valuable if randomly seeking around in a demo
STAT_TOTALSECRETS, // never changes during game
STAT_TOTALMONSTERS, // changes in some mods
STAT_SECRETS, // this makes svc_foundsecret unnecessary
STAT_MONSTERS, // this makes svc_killedmonster unnecessary
STAT_VIEWHEIGHT, // sent just for FTEQW clients
STAT_VIEWZOOM, // this rarely changes
-1,
};

protocol_t *Protocol_ForName(const char *name)
{
	int i;
	for(i = 0; &protocols[i]; i++)
	{
		if(!strncmp(name, protocols[i]->name, strlen(name)))
			break;
	}
	return protocols[i];
}

protocol_t *Protocol_ForNumber(int num)
{
	int i;
	for(i = 0; &protocols[i]; i++)
	{
		if(protocols[i]->num == num)
			break;
	}
	return protocols[i];
}

void Protocol_Names(char *buffer, size_t buffersize)
{
	int i;
	if (buffersize < 1)
		return;
	buffer[0] = 0;
	for (i = 0; protocols[i]; i++)
	{
		if (i > 1)
			strlcat(buffer, " ", buffersize);
		strlcat(buffer, protocols[i]->name, buffersize);
	}
}

void Protocol_WriteStatsReliable(void)
{
	int i, j;
	if (!host_client->netconnection)
		return;
	// detect changes in stats and write reliable messages
	// this only deals with 32 stats because the older protocols which use
	// this function can only cope with 32 stats,
	// they also do not support svc_updatestatubyte which was introduced in
	// DP6 protocol (except for QW)
	for (j = 0;sendquakestats[j] >= 0;j++)
	{
		i = sendquakestats[j];
		// check if this bit is set
		if (host_client->statsdeltabits[i >> 3] & (1 << (i & 7)))
		{
			host_client->statsdeltabits[i >> 3] -= (1 << (i & 7));
			// send the stat as a byte if possible
			if (sv.protocol == &protocol_quakeworld)
			{
				if (host_client->stats[i] >= 0 && host_client->stats[i] < 256)
				{
					MSG_WriteByte(&host_client->netconnection->message, qw_svc_updatestat);
					MSG_WriteByte(&host_client->netconnection->message, i);
					MSG_WriteByte(&host_client->netconnection->message, host_client->stats[i]);
				}
				else
				{
					MSG_WriteByte(&host_client->netconnection->message, qw_svc_updatestatlong);
					MSG_WriteByte(&host_client->netconnection->message, i);
					MSG_WriteLong(&host_client->netconnection->message, host_client->stats[i]);
				}
			}
			else
			{
				// this could make use of svc_updatestatubyte in DP6 and later
				// protocols but those protocols do not use this function
				MSG_WriteByte(&host_client->netconnection->message, svc_updatestat);
				MSG_WriteByte(&host_client->netconnection->message, i);
				MSG_WriteLong(&host_client->netconnection->message, host_client->stats[i]);
			}
		}
	}
}
