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
typedef struct {
	char* name;
	char** mods;
	int32_t modcount;
	int32_t playercount;
	int32_t playermax;
	int32_t index;
	void* next;
} lobby_room;






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






//ROOM FUNCTIONS
static lobby_room* rooms = 0;




static void room_add(lobby_room* room) {
	if(rooms == 0) {
		rooms = Mem_Alloc(idwpool,sizeof(lobby_room));
		memcpy(rooms,room,sizeof(lobby_room));
		room->next = 0;
	}else {
		lobby_room* prevnext = rooms->next;
		rooms->next = Mem_Alloc(idwpool,sizeof(lobby_room));
		memcpy(rooms->next,room,sizeof(lobby_room));
		((lobby_room*)rooms->next)->next = prevnext;
	}


}

static int room_iter(lobby_room** room) {
	if(*room == 0) {
		*room = rooms;
	}else {
	*room = (*room)->next;
	}
	return *room !=0;
}

static void room_clear() {
	while(rooms != 0) {
		int i;
		Mem_Free(rooms->name);
		for(i = 0;i<rooms->modcount;i++) {
			Mem_Free(rooms->mods[i]);
		}
		Mem_Free(rooms->mods);
		rooms = rooms->next;
	}
}




//END ROOM FUNCTIONS



static void msgloop(int sock) {

	while(recv(sock,xmitpacket,sizeof(xmitpacket),0)>0) {
		unsigned char* ptr = OpenStream();
		if(*ptr == 0) {
			ptr++;
			int32_t modcount = *getInt(&ptr);
			lobby_room room;
			room.modcount = modcount;
			room.mods = Mem_Alloc(idwpool,sizeof(void*)*modcount);
			int i;
			for(i = 0;i<modcount;i++) {
				int modlen = strlen(ptr)+1;
				room.mods[i] = Mem_Alloc(idwpool,modlen);
				memcpy(room.mods[i],ptr,modlen);
				ptr+=modlen;
			}
			room.name = Mem_Alloc(idwpool,strlen(ptr)+1);
			memcpy(room.name,ptr,strlen(ptr)+1);
			room.playercount = *getInt(&ptr);
			room.playermax = *getInt(&ptr);
			room.index = *getInt(&ptr);


		}else {
			if(*ptr == 1) {
				ptr++;
				runOnMainThread(printmsg_chat,ptr);
			}
		}

	}
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
xmit(&stream);
msgloop(sock);
}
static void doConnect(int sock) {
	runOnMainThread(printmsg,"Waiting for matches on fire");
	msgloop(sock);
}
static void bindhost() {
	TCPConnect("127.0.0.1",1090,doConnect);
}

static void findroom() {
	//This is safe when called from main thread
	Con_Print("Contacting server...\n");
	TCPConnect("50.17.30.158",1090,onConnected);
}
static void makeroom() {
	const char* name = Cmd_Args();
	if(name == 0) {
		Con_Print("Error: You must specify a host name\n");
	}else {
	unsigned char* ptr = OpenStream();
	*ptr = 3;
	ptr++;
	//Number of players desired for match TODO Make this a CVAR
	*getInt(&ptr) = 10;
	//Server name
	writeString(name,&ptr);
	//MODS: TODO Make CVAR
	*getInt(&ptr) = 1;
	writeString((char*)"CTF",&ptr);
	xmit(&ptr);
	}
}
static void getversion() {
	//This is safe when called from main thread
	Con_Print("IDWMaster Protocol version 0.1 Alpha\n");
}



static int trolon = false;
static void dotroll(void* ptr) {
	if(trolon) {
		Cmd_ExecuteString("cl_pony 1",src_command,1);
		//Cmd_ExecuteString("+crouch",src_command,1);

	}else {
		Cmd_ExecuteString("cl_pony 0",src_command,1);
		//Cmd_ExecuteString("-crouch",src_command,1);
	}

	Cmd_ExecuteString("sendcvar cl_pony",src_command,1);
	trolon =!trolon;
}
static void trollpony(void* data) {
	while(1) {

		usleep(100*1000);
		runOnMainThread(dotroll,0);
	}
}
static void someonetroll() {

	Thread_CreateThread(trollpony,0);
}

void lobby_Init() {
	syncevt = Thread_CreateCond();
	syncmtx = Thread_CreateMutex();

	idwpool = Mem_AllocPool("IDWMaster",0,NULL);
	Cmd_AddCommand("dsay",dosay,"Says something");
	Cmd_AddCommand("trollpony",someonetroll,"Trolls ponies");
	Cmd_AddCommand("idwversion",getversion,"Gets the version of the IDWMaster protocol");
	Cmd_AddCommand("lobbybind",bindhost,"Registers this server as an available host for the IDWMaster protocol");
	Cmd_AddCommand("lobbyfind",findroom,"Finds a game");
	Cmd_AddCommand("lobbymake",makeroom,"Makes a new lobby");
}
