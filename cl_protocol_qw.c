#include "cl_protocol_baseqw.h"

protocol_netmsg_t netmsg_qw_svc =
{
	.size = 53,
	.msg =
	{
		{"svc_bad", NULL},					// 0
		{"svc_nop", Netmsg_svc_nop},					// 1
		{"svc_disconnect", Netmsg_svc_disconnect},			// 2
		{"svc_updatestat", Netmsg_svc_updatestat},			// 3	// [byte] [byte]
		{"", NULL},								// 4
		{"svc_setview", NULL},				// 5	// [short] entity number
		{"svc_sound", Netmsg_svc_sound},					// 6	// <see code>
		{"", NULL},								// 7
		{"svc_print", Netmsg_svc_print},					// 8	// [byte] id [string] null terminated string
		{"svc_stufftext", Netmsg_svc_stufftext},				// 9	// [string] stuffed into client's console buffer
		{"svc_setangle", Netmsg_svc_setangle},				// 10	// [angle3] set the view angle to this absolute value
		{"svc_serverdata", Netmsg_svc_serverdata},			// 11	// [long] protocol ...
		{"svc_lightstyle", Netmsg_svc_lightstyle},			// 12	// [byte] [string]
		{"", NULL},								// 13
		{"svc_updatefrags", Netmsg_svc_updatefrags},			// 14	// [byte] [short]
		{"", NULL},								// 15
		{"svc_stopsound", Netmsg_svc_stopsound},				// 16	// <see code>
		{"", NULL},								// 17
		{"", NULL},								// 18
		{"svc_damage", Netmsg_svc_damage},				// 19
		{"svc_spawnstatic", Netmsg_svc_spawnstatic},			// 20
		{"", NULL},								// 21
		{"svc_spawnbaseline", Netmsg_svc_spawnbaseline},			// 22
		{"svc_temp_entity", Netmsg_svc_temp_entity},			// 23	// variable
		{"svc_setpause", Netmsg_svc_setpause},				// 24	// [byte] on / off
		{"", NULL},								// 25
		{"svc_centerprint", Netmsg_svc_centerprint},			// 26	// [string] to put in center of the screen
		{"svc_killedmonster", Netmsg_svc_killedmonster},			// 27
		{"svc_foundsecret", Netmsg_svc_foundsecret},			// 28
		{"svc_spawnstaticsound", Netmsg_svc_spawnstaticsound},		// 29	// [coord3] [byte] samp [byte] vol [byte] aten
		{"svc_intermission", Netmsg_svc_intermission},			// 30		// [vec3_t] origin [vec3_t] angle
		{"svc_finale", Netmsg_svc_finale},				// 31		// [string] text
		{"svc_cdtrack", Netmsg_svc_cdtrack},				// 32		// [byte] track
		{"svc_sellscreen", Netmsg_svc_sellscreen},			// 33
		{"svc_smallkick", Netmsg_svc_smallkick},				// 34		// set client punchangle to 2
		{"svc_bigkick", Netmsg_svc_bigkick},				// 35		// set client punchangle to 4
		{"svc_updateping", Netmsg_svc_updateping},			// 36		// [byte] [short]
		{"svc_updateentertime", Netmsg_svc_updateentertime},		// 37		// [byte] [float]
		{"svc_updatestatlong", Netmsg_svc_updatestatlong},		// 38		// [byte] [long]
		{"svc_muzzleflash", Netmsg_svc_muzzleflash},			// 39		// [short] entity
		{"svc_updateuserinfo", Netmsg_svc_updateuserinfo},		// 40		// [byte] slot [long] uid
		{"svc_download", Netmsg_svc_download},				// 41		// [short] size [size bytes]
		{"svc_playerinfo", Netmsg_svc_playerinfo},			// 42		// variable
		{"svc_nails", Netmsg_svc_nails},					// 43		// [byte] num [48 bits] xyzpy 12 12 12 4 8
		{"svc_chokecount", Netmsg_svc_chokecount},			// 44		// [byte] packets choked
		{"svc_modellist", Netmsg_svc_modellist},				// 45		// [strings]
		{"svc_soundlist", Netmsg_svc_soundlist},				// 46		// [strings]
		{"svc_packetentities", Netmsg_svc_packetentities},		// 47		// [...]
		{"svc_deltapacketentities", Netmsg_svc_deltapacketentities},	// 48		// [...]
		{"svc_maxspeed", Netmsg_svc_maxspeed},				// 49		// maxspeed change, for prediction
		{"svc_entgravity", Netmsg_svc_entgravity},			// 50		// gravity change, for prediction
		{"svc_setinfo", Netmsg_svc_setinfo},				// 51		// setinfo on a client
		{"svc_serverinfo", Netmsg_svc_serverinfo},			// 52		// serverinfo
		{"svc_updatepl", Netmsg_svc_updatepl}				// 53		// [byte] [byte]
	}
};
