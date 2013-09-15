/*
 * idwmaster_gameroom.c
 *
 *  Created on: Aug 7, 2013
 *      Author: IDWMaster
 */


#include "quakedef.h"
#include "lhnet.h"
#include "console.h"
//#include <sys/socket.h>
//#include <sys/types.h>
// #include <arpa/inet.h>
//#include <netinet/in.h>























/*
 * graphitemaster -- Suggested compression functions
 */
/*
// step 1: network buffer management that doesn't suck
typedef struct {
    unsigned char *buffer;
    int            len;
    int            maxlen;
} netbuff_t;

netbuff_t *buffer_create(unsigned char *buffer, int maxlen) {
    netbuff_t *n = Mem_Alloc(pool, sizeof(netbuff_t));
    n->buffer = buffer;
    n->maxlen = maxlen;
    n->len    = 0;
    return n;
}

unsigned char buffer_get(netbuff_t *n) {
    static unsigned char overread = 0;
    if (n->len < n->maxlen)
        return n->buffer[n->len++];
    return overread;
}

void buffer_put(netbuff_t *n, const unsigned char value) {
    if (n->len < n->maxlen)
        n->buffer[n->len++] = value;
}

// step 2: a simple compression scheme to save on network traffic
void net_putint(netbuff_t *n, int v) {
    // only need a byte if we only need a byte
    if (v < 128 && n > -127) {
        buffer_put(n, (unsigned char)v);
    } else if (v < 0x8000 && v >= -0x8000) {
        // three bytes is better than 4
        buffer_put(n, 0x80);
        buffer_put(n, (unsigned char)(v));
        buffer_put(n, (unsigned char)(v >> 8));
    } else {
        buffer_put(n, 0x81);
        buffer_put(n, (unsigned char)(v));
        buffer_put(n, (unsigned char)(v>>8));
        buffer_put(n, (unsigned char)(v>>16));
        buffer_put(n, (unsigned char)(v>>24));
    }
}

// get int is even easier
int net_getint(netbuff_t *n) {
    int v = (char)buffer_get(n);
    int p = 0;
    if (v == -128) {
        p  = buffer_get(n);
        p |= ((char)buffer_get(n)) << 8;
        return p;
    } else if (c == -127) {
        p  = buffer_get(n);
        p |= buffer_get(n) << 8;
        p |= buffer_get(n) << 16;
        p |= buffer_get(n) << 24;

        return p;
    }
    return c;
}

*/


/*
 * End compression
 */












#include "thread.h"
typedef struct {
	char* hostname;
	int port;
	void(*connectDgate)(lhnetsocket_t*);
} hostinfo;
typedef struct {
	char* name;
	char** mods;
	unsigned int modcount;
	unsigned int playercount;
	unsigned int playermax;
	unsigned int index;
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
	lhnetaddress_t vh;
	LHNETADDRESS_FromString(&vh,ifo->hostname,1090);
	lhnetsocket_t* s = LHNET_OpenSocket(&vh,&vh,1,1,1);
if(s !=0) {
		ifo->connectDgate(s);
}
	Mem_Free(ifo->hostname);
	Mem_Free(ifo);
	return 0;
}
static void TCPConnect(const char* host, void(*connectDgate)(lhnetsocket_t*)) {

	hostinfo* ptr = (hostinfo*)Mem_Alloc(idwpool,sizeof(hostinfo));


	ptr->connectDgate = connectDgate;
	ptr->hostname = strdup(host);
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
static unsigned int* getInt(unsigned char** stream) {
	unsigned int* retval = (unsigned int*)*stream;
	(*stream)+=sizeof(unsigned int);
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



static void msgloop(lhnetsocket_t* sock) {
	lhnetaddress_t addr;
	while(LHNET_Read(sock,xmitpacket,sizeof(xmitpacket),&addr)) {
		unsigned char* ptr = OpenStream();
		if(*ptr == 0) {
			ptr++;
			unsigned int modcount = *getInt(&ptr);
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
			ptr+=strlen(ptr)+1;
			room.playercount = *getInt(&ptr);
			room.playermax = *getInt(&ptr);
			room.index = *getInt(&ptr);
			char bigbufferbill[1024*5];
			memset(bigbufferbill,0,sizeof(bigbufferbill));
			sprintf(bigbufferbill,"Found room called %s with %i players out of %i available slots, at index %i.\n",room.name,room.playercount,room.playermax,room.index);
			runOnMainThread(printmsg,bigbufferbill);
		}else {
			if(*ptr == 1) {
				ptr++;
				runOnMainThread(printmsg_chat,ptr);
			}
		}

	}
}

static lhnetsocket_t* _sock;
static void xmit(unsigned char** stream) {
	size_t sz = (*stream-xmitpacket);
	LHNET_Write(_sock,xmitpacket,sz+1,0);
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
static void onConnected(lhnetsocket_t* sock) {
_sock = sock;
runOnMainThread(printmsg,"Connected to backend! Scanning for servers....\n");
unsigned char* stream = OpenStream();
*stream = 0;
xmit(&stream);
msgloop(sock);
}
static void doConnect(lhnetsocket_t* sock) {
	runOnMainThread(printmsg,"Waiting for matches on fire");
	msgloop(sock);
}
static void bindhost() {
	TCPConnect("127.0.0.1",doConnect);
}

static void findroom() {
	//This is safe when called from main thread
	Con_Print("Contacting server...\n");
	TCPConnect("50.17.215.71",onConnected);
}
static void lobbycon() {
	if(Cmd_Argv(0) !=0) {
	int val = atoi(Cmd_Argv(0));
	unsigned char* ptr = OpenStream();
	*ptr = 4;
	ptr++;
	*getInt(&ptr) = val;
	xmit(&ptr);
	}else {
		Con_Print("You must specify a valid index");
	}

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
	writeString("CTF",&ptr);
	xmit(&ptr);
	}
}
static void getversion() {
	//This is safe when called from main thread
	Con_Print("IDWMaster Protocol version 0.1 Alpha\n");
}



static int trolon = 0;
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
	Cmd_AddCommand("lobbyconnect",lobbycon,"Connects to a specified lobby based on index");
	Cmd_AddCommand("lobbymake",makeroom,"Makes a new lobby");
}
