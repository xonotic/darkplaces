#ifndef CL_PARSE_H
#define CL_PARSE_H

#include "qtypes.h"
#include "cvar.h"
struct entity_s;

extern struct cvar_s qport;

void CL_Parse_Init(void);
void CL_Parse_Shutdown(void);
void CL_ParseServerMessage(void);
void CL_Parse_DumpPacket(void);
void CL_Parse_ErrorCleanUp(void);
void QW_CL_StartUpload(unsigned char *data, int size);
void CL_KeepaliveMessage(qbool readmessages); // call this during loading of large content
void CL_ParseBaseline (struct entity_s *ent, int large);
void CL_ParseClientdata (void);
void CL_ParseStatic (int large);
void CL_ParseStaticSound (int large);
void CL_ParseEffect (void);
void CL_ParseEffect2 (void);
void CL_ParseServerInfo (void);
qbool CL_ExaminePrintString(const char *text);
void CL_ParseStartSoundPacket(int largesoundindex);
void CL_ParseTempEntity(void);
void QW_CL_UpdateUserInfo(void);
void QW_CL_SetInfo(void);
void QW_CL_ServerInfo(void);
void QW_CL_ParseDownload(void);
void QW_CL_ParseModelList(void);
void QW_CL_ParseNails(void);
void CL_UpdateItemsAndWeapon(void);
void QW_CL_ParseSoundList(void);
void CL_SignonReply (void);
void CL_NetworkTimeReceived(double newtime);
void CL_ParseDownload(void);
void CL_ParseTrailParticles(void);
void CL_ParsePointParticles(void);
void CL_ParsePointParticles1(void);

#endif
