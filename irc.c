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
}

static int IRC_Connect(const char *addr)
{
	lhnetaddress_t address;

	IRC_Disconnect();

	if (!LHNETADDRESS_FromString(&address, addr, 6667))
	{
		Con_Printf("[IRC] Bad server address given: %s.\n", addr);
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
		Z_Free((void *) irc_socket);
		return 0;
	}

	Con_Print("[IRC] Connecting to the server...\n");
	return 1;
}

static void IRC_AddMessage(const char *message)
{
	size_t len = strlen(message);

	memcpy(irc_outgoing.data + irc_outgoing.len, message, sizeof (irc_outgoing.data) - irc_outgoing.len - 2);
	memcpy(irc_outgoing.data + min(irc_outgoing.len + len, sizeof (irc_outgoing.data) - 2), "\r\n", 2);

	irc_outgoing.len = min(irc_outgoing.len + len + 2, sizeof (irc_outgoing.data));

	Con_Printf("[IRC] %lu bytes waiting to be written\n", (unsigned long) irc_outgoing.len);
}

static ircmessage_t *IRC_AllocMessage(void)
{
	ircmessage_t *msg;

	if ((msg = Z_Malloc(sizeof (*msg))))
		memset(msg, 0, sizeof (*msg));

	return msg;
}

static void IRC_FreeMessage(ircmessage_t *msg)
{
	if (msg)
	{
		int i;

		if (msg->prefix) Z_Free(msg->prefix);
		if (msg->command) Z_Free(msg->command);

		for (i = 0; i < msg->args_num; i++)
			Z_Free(msg->args[i]);

		Z_Free(msg);
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

			*arg = Z_Malloc(len + 1);
			memcpy(*arg, args + 1, len);
			(*arg)[len] = 0;

			msg->args_num++;
			break;
		}
		else
		{
			len = strcspn(args, " ");

			*arg = Z_Malloc(len + 1);
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

		msg->prefix = Z_Malloc(len + 1);
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

	msg->command = Z_Malloc(len + 1);
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

	Con_Printf("[IRC] prefix : %s\n", msg->prefix ? msg->prefix : "");
	Con_Printf("[IRC] command: %s\n", msg->command);

	for (i = 0; i < msg->args_num; i++)
		Con_Printf("[IRC] arg %-3d: %s\n", i, msg->args[i]);
}

static void IRC_ProcessMessage(const char *line)
{
	ircmessage_t *msg;

	if ((msg = IRC_ParseMessage(line)))
	{
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
		Con_Printf("[IRC] Read %d bytes\n", read);
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
		Con_Printf("[IRC] Wrote %d bytes\n", written);
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
				Con_Print("[IRC] Failed to connect to the server.\n");
				IRC_Disconnect();
				break;

			case LHNETCONSTATUS_CONNECTED:
				IRC_WriteMessages();
				IRC_ReadMessages();
				break;
		}
	}
}

static const char *IRC_NickFromPlayerName(void)
{
	const char prefix[] = "[DP]";
	const size_t prefix_len = sizeof (prefix) - 1;
	char *nick;

	nick = Z_Malloc(prefix_len + strlen(cl_name.string) + 1);
	memcpy(nick, prefix, prefix_len + 1);
	SanitizeString(cl_name.string, nick + prefix_len);

	return nick;
}

static void IRC_Connect_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Print("ircconnect <address> : connect to an IRC server\n");
		return;
	}

	if (IRC_Connect(Cmd_Argv(1)))
	{
		const char *nick = IRC_NickFromPlayerName();

		IRC_AddMessage(va("NICK %s", nick));
		IRC_AddMessage(va("USER %s %s %s :%s", nick, nick, Cmd_Argv(1), nick));

		Z_Free((void *) nick);
	}
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
	Cmd_AddCommand("ircconnect", IRC_Connect_f, "connect to an IRC server");
	Cmd_AddCommand("ircdisconnect", IRC_Disconnect_f, "disconnect from an IRC server");
	Cmd_AddCommand("irc", IRC_IRC_f, "send raw messages to a connected IRC server");
}

void IRC_Shutdown(void)
{
	IRC_Disconnect();
}
