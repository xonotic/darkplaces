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
static void printmsg_chat(void* msg) {
	Con_MaskPrint(CON_MASK_CHAT,(char*)msg);
}
static void printmsg(void* msg) {
	Con_Print((char*)msg);
}

static void* syncmtx;
static void* dispatchMsg = 0;
static void(*dispatchFunc)(void*) = 0;
static void* syncevt;
static void runOnMainThread(void(*func)(void*),void* message) {
	Thread_LockMutex(syncmtx);
	dispatchMsg = message;
	dispatchFunc = func;

	Thread_CondWait(syncevt,syncmtx);
	Thread_UnlockMutex(syncmtx);
}
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
		//Con_Printf("Connect failed (code %i).\n",status);
		runOnMainThread(printmsg,"Connect failed.");
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



void lobby_Loop() {
Thread_LockMutex(syncmtx);
if(dispatchFunc !=0) {

	dispatchFunc(dispatchMsg);
	dispatchFunc = 0;
	Thread_CondSignal(syncevt);
}
Thread_UnlockMutex(syncmtx);
}
static void msgloop(int sock) {
	unsigned char buffer[2048];
	while(recv(sock,buffer,sizeof(buffer),0)>0) {
		unsigned char* ptr = buffer;
		if(*ptr == 0) {
			runOnMainThread(printmsg,"Server found!");
			//TODO: Connect to the server
		}else {
			if(*ptr == 1) {
				//Chat message (process)
				ptr++;
				runOnMainThread(printmsg_chat,(char*)ptr);
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
runOnMainThread(printmsg,"Connected to backend! Scanning for servers....\n");
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
	//This is safe when called from main thread
	Con_Print("Contacting server...\n");
	TCPConnect("127.0.0.1",1090,onConnected);
}
static void getversion() {
	//This is safe when called from main thread
	Con_Print("IDWMaster Protocol version 0.1 Alpha\n");
}
void lobby_Init() {
	syncevt = Thread_CreateCond();
	syncmtx = Thread_CreateMutex();

	idwpool = Mem_AllocPool("IDWMaster",0,NULL);
	Cmd_AddCommand("dsay",dosay,"Says something");
	Cmd_AddCommand("idwversion",getversion,"Gets the version of the IDWMaster protocol");
	Cmd_AddCommand("idwfind",findroom,"Finds a game");
}
