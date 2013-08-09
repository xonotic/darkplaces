/*
 * idwmaster_gameroom.c
 *
 *  Created on: Aug 7, 2013
 *      Author: IDWMaster
 */


#include "quakedef.h"
#include "lhnet.h"
#include "console.h"
#include <sys/socket.h>
#include <sys/types.h>
 #include <arpa/inet.h>
#include <netinet/in.h>
#include "thread.h"
typedef struct {
	char* hostname;
	int port;
	void(*connectDgate)(int);
} hostinfo;

static mempool_t* idwpool;
static int callback(void* ptr) {
	hostinfo* ifo = (hostinfo*)ptr;
	struct sockaddr_in client;

	memset(&client,0,sizeof(client));
	client.sin_addr.s_addr = inet_addr(ifo->hostname);
	client.sin_port = htons(ifo->port);
	client.sin_family = AF_INET;
	int sock = socket(AF_INET,SOCK_STREAM,0);
	int status;
	if((status = connect(sock,(struct sockaddr_t*)&client,sizeof(client))) <0) {
		Con_Printf("Connect failed (code %i).\n",status);
	}else {
		ifo->connectDgate(sock);
	}

	Mem_Free(ifo->hostname);
	Mem_Free(ifo);
	return 0;
}

static void TCPConnect(const char* hostname, int port, void(*connectDgate)(int)) {
	hostinfo* ptr = (hostinfo*)Mem_Alloc(idwpool,sizeof(hostinfo));
	ptr->connectDgate = connectDgate;
	ptr->hostname = Mem_Alloc(idwpool,strlen(hostname)+1);
	ptr->port = port;
	memcpy(ptr->hostname,hostname,strlen(hostname)+1);
	Thread_CreateThread(callback,ptr);
}
void IDWMaster_Loop() {

}
static void msgloop(int sock) {
	unsigned char buffer[2048];
	while(recv(sock,buffer,sizeof(buffer),0)>0) {
		unsigned char* ptr = buffer;
		if(*ptr == 0) {
			Con_DPrint("Server found!");
		}else {
			if(*ptr == 1) {
				//Chat message (process)
				ptr++;
				Con_MaskPrint(CON_MASK_CHAT,(char*)ptr);
			}
		}

	}
}
static unsigned char xmitpacket[2048];
static unsigned char* OpenStream() {
	return xmitpacket;
}
static int32_t* getInt(unsigned char** stream) {
	int32_t* retval = (int32_t*)*stream;
	(*stream)+=sizeof(int32_t);
	return retval;
}
static void writeString(const char* data,unsigned char** stream) {
	size_t sz = strlen(data);
	memcpy(*stream,data,sz+1);
	(*stream)+=sz+1;

}
static int _sock;
static void xmit(unsigned char** stream) {
	size_t sz = (*stream-xmitpacket);
	send(_sock,xmitpacket,sz,0);
}

static void dosay() {
	unsigned char* stream = OpenStream();
	if(Cmd_Argc() >0) {
		*stream = 1;
		stream++;
		writeString(Cmd_Args(),&stream);
		xmit(&stream);
	}
}
//TODO: Also reference mvm_cmds.c to see how builtins work
static void onConnected(int sock) {
_sock = sock;
Con_Print("Connected to backend! Scanning for servers....\n");

Con_MaskPrint(CON_MASK_CHAT,"Welcome to the lobbyist group.\n");
unsigned char* stream = OpenStream();
*stream = 0;
stream++;
//4 players total
*getInt(&stream) = 4;
//CTF mod
*getInt(&stream) = 1;
writeString("CTF",&stream);
xmit(&stream);
msgloop(sock);
}
static void findroom() {
	Con_Print("Contacting server...\n");
	TCPConnect("127.0.0.1",1090,onConnected);
}
static void getversion() {
	Con_Print("IDWMaster Protocol version 0.1 Alpha\n");
}
void IDWMaster_Init() {

	idwpool = Mem_AllocPool("IDWMaster",0,NULL);
	Cmd_AddCommand("dsay",dosay,"Says something");
	Cmd_AddCommand("idwversion",getversion,"Gets the version of the IDWMaster protocol");
	Cmd_AddCommand("idwfind",findroom,"Finds a game");
}
