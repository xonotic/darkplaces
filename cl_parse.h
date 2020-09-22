#include "quakedef.h"

void CL_ParseBaseline (entity_t *ent, int large);
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