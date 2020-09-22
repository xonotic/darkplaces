#include "quakedef.h"
#include "server.h"
#include "protocol.h"
#include "sv_user.h"

static void Netmsg_clc_nop(protocol_t *protocol)
{
}

static void Netmsg_clc_stringcmd(protocol_t *protocol)
{
	prvm_prog_t *prog = SVVM_prog;
	char *s, *p, *q;

	// allow reliable messages now as the client is done with initial loading
	if (host_client->sendsignon == 2)
		host_client->sendsignon = 0;
	s = MSG_ReadString(&sv_message, sv_readstring, sizeof(sv_readstring));
	q = NULL;
	for(p = s; *p; ++p) switch(*p)
	{
		case 10:
		case 13:
			if(!q)
				q = p;
			break;
		default:
			if(q)
				goto clc_stringcmd_invalid; // newline seen, THEN something else -> possible exploit
			break;
	}
	if(q)
		*q = 0;
	if (strncasecmp(s, "spawn", 5) == 0
		|| strncasecmp(s, "begin", 5) == 0
		|| strncasecmp(s, "prespawn", 8) == 0)
		Cmd_ExecuteString (&cmd_serverfromclient, s, src_client, true);
	else if (PRVM_serverfunction(SV_ParseClientCommand))
	{
		int restorevm_tempstringsbuf_cursize;
		restorevm_tempstringsbuf_cursize = prog->tempstringsbuf.cursize;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(prog, s);
		PRVM_serverglobalfloat(time) = sv.time;
		PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
		prog->ExecuteProgram(prog, PRVM_serverfunction(SV_ParseClientCommand), "QC function SV_ParseClientCommand is missing");
		prog->tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
	}
	else
		Cmd_ExecuteString (&cmd_serverfromclient, s, src_client, true);
	return;

clc_stringcmd_invalid:
	Con_Printf("Received invalid stringcmd from %s\n", host_client->name);
	if(developer.integer > 0)
		Com_HexDumpToConsole((unsigned char *) s, (int)strlen(s));
	return;
}

static void Netmsg_clc_disconnect(protocol_t *protocol)
{
	SV_DropClient (false); // client wants to disconnect
}

static void Netmsg_clc_move(protocol_t *protocol)
{
	SV_ReadClientMove();
}

static void Netmsg_clc_ackdownloaddata(protocol_t *protocol)
{
	int start = MSG_ReadLong(&sv_message);
	int num = MSG_ReadShort(&sv_message);
	if (host_client->download_file && host_client->download_started)
	{
		if (host_client->download_expectedposition == start)
		{
			int size = (int)FS_FileSize(host_client->download_file);
			// a data block was successfully received by the client,
			// update the expected position on the next data block
			host_client->download_expectedposition = start + num;
			// if this was the last data block of the file, it's done
			if (host_client->download_expectedposition >= FS_FileSize(host_client->download_file))
			{
				// tell the client that the download finished
				// we need to calculate the crc now
				//
				// note: at this point the OS probably has the file
				// entirely in memory, so this is a faster operation
				// now than it was when the download started.
				//
				// it is also preferable to do this at the end of the
				// download rather than the start because it reduces
				// potential for Denial Of Service attacks against the
				// server.
				int crc;
				unsigned char *temp;
				FS_Seek(host_client->download_file, 0, SEEK_SET);
				temp = (unsigned char *) Mem_Alloc(tempmempool, size);
				FS_Read(host_client->download_file, temp, size);
				crc = CRC_Block(temp, size);
				Mem_Free(temp);
				// calculated crc, send the file info to the client
				// (so that it can verify the data)
				SV_ClientCommands("\ncl_downloadfinished %i %i %s\n", size, crc, host_client->download_name);
				Con_DPrintf("Download of %s by %s has finished\n", host_client->download_name, host_client->name);
				FS_Close(host_client->download_file);
				host_client->download_file = NULL;
				host_client->download_name[0] = 0;
				host_client->download_expectedposition = 0;
				host_client->download_started = false;
			}
		}
		else
		{
			// a data block was lost, reset to the expected position
			// and resume sending from there
			FS_Seek(host_client->download_file, host_client->download_expectedposition, SEEK_SET);
		}
	}
}

static void Netmsg_clc_ackframe(protocol_t *protocol)
{
	int num;
	if (sv_message.badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	num = MSG_ReadLong(&sv_message);
	if (sv_message.badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	if (developer_networkentities.integer >= 10)
		Con_Printf("recv clc_ackframe %i\n", num);
	// if the client hasn't progressed through signons yet,
	// ignore any clc_ackframes we get (they're probably from the
	// previous level)
	if (host_client->begun && host_client->latestframenum < num)
	{
		int i;
		for (i = host_client->latestframenum + 1;i < num;i++)
			if (!SV_FrameLost(i))
				break;
		SV_FrameAck(num);
		host_client->latestframenum = num;
	}
}
