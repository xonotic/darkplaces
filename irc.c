#include "quakedef.h"
#include "lhnet.h"
#include "console.h"

#define IRC_MAX_ARGS 15
#define IRC_NET_BUFFER_LEN 1024

typedef struct ircmessage_s
{
	char *prefix;
	char *command;
	char *args[IRC_MAX_ARGS];
	int args_num;
}
ircmessage_t;

typedef struct ircnetbuffer_s
{
	size_t len;
	/* Don't add new members below this. */
	char data[IRC_NET_BUFFER_LEN];
}
ircnetbuffer_t;

static lhnetsocket_t *irc_socket;
static ircnetbuffer_t irc_incoming;
static ircnetbuffer_t irc_outgoing;

static qboolean irc_registered;

static cvar_t irc_nickname = { CVAR_SAVE, "irc_nickname", "", "nickname to use when connecting to IRC" };

static mempool_t *irc_mempool;

static void IRC_Disconnect(void)
{
	if (irc_socket)
	{
		Con_Print("[IRC] Closed connection.\n");
		LHNET_CloseSocket(irc_socket);
		irc_socket = NULL;
	}

	memset(&irc_incoming, 0, offsetof(ircnetbuffer_t, data));
	memset(&irc_outgoing, 0, offsetof(ircnetbuffer_t, data));

	irc_registered = false;
}

static int IRC_Connect(const char *addr)
{
	lhnetaddress_t address;

	IRC_Disconnect();

	if (!LHNETADDRESS_FromString(&address, addr, 6667))
	{
		Con_Printf("[IRC] Bad server address: %s.\n", addr);
		return 0;
	}

	if (!(irc_socket = LHNET_AllocSocket(&address)))
	{
		Con_Printf("[IRC] Couldn't allocate a socket.\n");
		return 0;
	}

	LHNET_OpenSocket_Connected(irc_socket);

	if (irc_socket->constatus == LHNETCONSTATUS_ERROR)
	{
		/* LHNET prints an error, so we don't have to. */
		Mem_Free((void *) irc_socket);
		return 0;
	}

	Con_Printf("[IRC] Connecting to %s...\n", addr);
	return 1;
}

static void IRC_AddMessage(const char *message)
{
	size_t len = strlen(message);

	if (irc_outgoing.len + len + 2 > sizeof (irc_outgoing.data))
	{
		Con_Print("[IRC] Output buffer overflow.\n");
		return;
	}

	memcpy(irc_outgoing.data + irc_outgoing.len, message, sizeof (irc_outgoing.data) - irc_outgoing.len - 2);
	memcpy(irc_outgoing.data + irc_outgoing.len + len, "\r\n", 2);

	irc_outgoing.len += len + 2;

	Con_DPrintf("[IRC] %lu bytes waiting to be written\n", (unsigned long) irc_outgoing.len);
}

static ircmessage_t *IRC_AllocMessage(void)
{
	ircmessage_t *msg;

	if ((msg = Mem_Alloc(irc_mempool, sizeof (*msg))))
		memset(msg, 0, sizeof (*msg));

	return msg;
}

static void IRC_FreeMessage(ircmessage_t *msg)
{
	if (msg)
	{
		int i;

		if (msg->prefix) Mem_Free(msg->prefix);
		if (msg->command) Mem_Free(msg->command);

		for (i = 0; i < msg->args_num; i++)
			Mem_Free(msg->args[i]);

		Mem_Free(msg);
	}
}

static void IRC_ParseArgs(ircmessage_t *msg, const char *args)
{
	msg->args_num = 0;

	while (msg->args_num < IRC_MAX_ARGS && args[0])
	{
		char **arg = msg->args + msg->args_num;
		size_t len;

		if (args[0] == ':')
		{
			len = strlen(args + 1);

			*arg = Mem_Alloc(irc_mempool, len + 1);
			memcpy(*arg, args + 1, len);
			(*arg)[len] = 0;

			msg->args_num++;
			break;
		}
		else
		{
			len = strcspn(args, " ");

			*arg = Mem_Alloc(irc_mempool, len + 1);
			memcpy(*arg, args, len);
			(*arg)[len] = 0;

			msg->args_num++;

			args += len;
			args += strspn(args, " ");
		}
	}
}

static ircmessage_t *IRC_ParseMessage(const char *line)
{
	const int line_len = strlen(line);
	const char *line_end = line + line_len;
	ircmessage_t *msg;
	size_t len;

	msg = IRC_AllocMessage();

	if (!msg)
		return NULL;

	if (line[0] == ':')
	{
		line += 1;

		len = strcspn(line, " ");

		msg->prefix = Mem_Alloc(irc_mempool, len + 1);
		memcpy(msg->prefix, line, len);
		msg->prefix[len] = 0;

		line += len;
		line += strspn(line, " ");
	}
	else
		msg->prefix = NULL;

	if (line == line_end)
	{
		IRC_FreeMessage(msg);
		return NULL;
	}

	len = strcspn(line, " ");

	msg->command = Mem_Alloc(irc_mempool, len + 1);
	memcpy(msg->command, line, len);
	msg->command[len] = 0;

	line += len;
	line += strspn(line, " ");

	if (line != line_end)
		IRC_ParseArgs(msg, line);

	return msg;
}

static void IRC_DumpMessage(const ircmessage_t *msg)
{
	int i;

	Con_DPrintf("[IRC] prefix : %s\n", msg->prefix ? msg->prefix : "");
	Con_DPrintf("[IRC] command: %s\n", msg->command);

	for (i = 0; i < msg->args_num; i++)
		Con_DPrintf("[IRC] arg %-3d: %s\n", i, msg->args[i]);
}

#define RPL_WELCOME 1
#define RPL_INVITING 341

static void IRC_ProcessMessage(const char *line)
{
	ircmessage_t *msg;

	if ((msg = IRC_ParseMessage(line)))
	{
		size_t nick_len = strcspn(msg->prefix, "!");
		long int reply;
		char *end;

		reply = strtol(msg->command, &end, 10);

		if (end != msg->command && *end == 0)
		{
			qboolean suppress = false;

			switch (reply)
			{
				case RPL_WELCOME:
					/* Update nickname in case it was truncated. */
					Cvar_SetQuick(&irc_nickname, msg->args[0]);
					irc_registered = true;
					break;

				case RPL_INVITING:
					Con_Printf("[IRC] You have invited %s to %s\n", msg->args[1], msg->args[2]);
					suppress = true;
					break;
			}

			if (!suppress && msg->args_num > 0)
				Con_Printf("[IRC] %s: %s\n", msg->prefix, msg->args[msg->args_num - 1]);
		}
		else if (strcmp("NICK", msg->command) == 0)
		{
			if (strlen(irc_nickname.string) == nick_len && strncmp(irc_nickname.string, msg->prefix, nick_len) == 0)
			{
				Cvar_SetQuick(&irc_nickname, msg->args[0]);
				Con_Printf("[IRC] You are now known as %s\n", irc_nickname.string);
			}
			else
			{
				/* Print only the nickname part of the prefix. */
				Con_Printf("[IRC] %.*s is now known as %s\n", (int) nick_len, msg->prefix, msg->args[0]);
			}
		}
		else if (strcmp("QUIT", msg->command) == 0)
		{
			if (msg->args_num > 0)
				Con_Printf("[IRC] %.*s has quit (%s)\n", (int) nick_len, msg->prefix, msg->args[0]);
			else
				Con_Printf("[IRC] %.*s has quit\n", (int) nick_len, msg->prefix);
		}
		else if (strcmp("JOIN", msg->command) == 0)
		{
			Con_Printf("[IRC] %.*s has joined %s\n", (int) nick_len, msg->prefix, msg->args[0]);
		}
		else if (strcmp("PART", msg->command) == 0)
		{
			if (msg->args_num > 1)
				Con_Printf("[IRC] %.*s has left %s (%s)\n", (int) nick_len, msg->prefix, msg->args[0], msg->args[1]);
			else
				Con_Printf("[IRC] %.*s has left %s\n", (int) nick_len, msg->prefix, msg->args[0]);
		}
		else if (strcmp("TOPIC", msg->command) == 0)
		{
			if (msg->args[1][0])
				Con_Printf("[IRC] %.*s changed %s topic to: %s\n", (int) nick_len, msg->prefix, msg->args[0], msg->args[1]);
			else
				Con_Printf("[IRC] %.*s deleted %s topic\n", (int) nick_len, msg->prefix, msg->args[0]);
		}
		else if (strcmp("INVITE", msg->command) == 0)
		{
			Con_Printf("[IRC] %.*s has invited you to %s\n", (int) nick_len, msg->prefix, msg->args[1]);
		}

		IRC_DumpMessage(msg);
		IRC_FreeMessage(msg);
	}
}

static void IRC_ProcessAllMessages(void)
{
	size_t remaining_len = irc_incoming.len;
	char *remaining = irc_incoming.data;

	while (remaining_len > 0)
	{
		char *nl;
		size_t len;

		nl = memchr(remaining, '\n', remaining_len);

		if (!nl)
		{
			if (remaining_len == irc_incoming.len && irc_incoming.len == sizeof (irc_incoming.data))
			{
				/* Full buffer, yet STILL no newline?  Flush it. */
				irc_incoming.len = 0;
				Con_Print("[IRC] Input buffer overflow.\n");
				return;
			}

			/* Probably incomplete message. */
			memmove(irc_incoming.data, remaining, remaining_len);
			break;
		}

		nl[0] = 0;

		if (nl != remaining && nl[-1] == '\r')
			nl[-1] = 0;

		IRC_ProcessMessage(remaining);

		len = (nl - remaining) + 1;
		remaining += len;
		remaining_len -= len;
	}

	irc_incoming.len = remaining_len;
}

static void IRC_ReadMessages(void)
{
	lhnetaddress_t dummyaddress;
	int read;

	read = LHNET_Read(irc_socket, irc_incoming.data + irc_incoming.len, sizeof (irc_incoming.data) - irc_incoming.len, &dummyaddress);

	if (read > 0)
	{
		Con_DPrintf("[IRC] Read %d bytes\n", read);
		irc_incoming.len += read;
		IRC_ProcessAllMessages();
	}
}

static void IRC_WriteMessages(void)
{
	lhnetaddress_t dummyaddress = irc_socket->address;
	int written;

	written = LHNET_Write(irc_socket, irc_outgoing.data, irc_outgoing.len, &dummyaddress);

	if (written > 0)
	{
		Con_DPrintf("[IRC] Wrote %d bytes\n", written);
		memmove(irc_outgoing.data, irc_outgoing.data + written, irc_outgoing.len - written);
		irc_outgoing.len -= written;
	}
}

void IRC_Frame(void)
{
	if (irc_socket)
	{
		if (irc_socket->constatus == LHNETCONSTATUS_INPROGRESS)
			LHNET_OpenSocket_Connected(irc_socket);

		switch (irc_socket->constatus)
		{
			case LHNETCONSTATUS_INPROGRESS:
				break;

			case LHNETCONSTATUS_DISCONNECTED:
				IRC_Disconnect();
				break;

			case LHNETCONSTATUS_ERROR:
				Con_Print("[IRC] Connection error.\n");
				IRC_Disconnect();
				break;

			case LHNETCONSTATUS_CONNECTED:
				IRC_WriteMessages();
				IRC_ReadMessages();
				break;
		}
	}
}

static char *IRC_NickFromPlayerName(void)
{
	char *nick;
	nick = Mem_Alloc(irc_mempool, strlen(cl_name.string) + 1);
	SanitizeString(cl_name.string, nick);
	return nick;
}

static void IRC_Register(void)
{
	char *nick = IRC_NickFromPlayerName();

	if (!irc_nickname.string[0])
		Cvar_SetQuick(&irc_nickname, nick);

	IRC_AddMessage(va("NICK %s", irc_nickname.string));
	IRC_AddMessage(va("USER %s optional optional :%s", irc_nickname.string, nick));

	Mem_Free(nick);
}

static void IRC_Connect_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Print("ircconnect <address> : connect to an IRC server\n");
		return;
	}

	if (IRC_Connect(Cmd_Argv(1)))
		IRC_Register();
}

static void IRC_Disconnect_f(void)
{
	IRC_Disconnect();
}

static void IRC_IRC_f(void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Print("irc <raw IRC message>\n");
		return;
	}

	if (irc_socket)
		IRC_AddMessage(Cmd_Args());
	else
		Con_Print("[IRC] Not connected to a server.\n");
}

void IRC_Init(void)
{
	irc_mempool = Mem_AllocPool("IRC", 0, NULL);

	Cvar_RegisterVariable(&irc_nickname);

	Cmd_AddCommand("ircconnect", IRC_Connect_f, "connect to an IRC server");
	Cmd_AddCommand("ircdisconnect", IRC_Disconnect_f, "disconnect from an IRC server");
	Cmd_AddCommand("irc", IRC_IRC_f, "send raw messages to a connected IRC server");
}

void IRC_Shutdown(void)
{
	IRC_Disconnect();

	Mem_FreePool(&irc_mempool);
}
