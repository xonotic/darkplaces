#ifndef IRC_H
#define IRC_H

void IRC_Init(void);
void IRC_Frame(void);
void IRC_Shutdown(void);

int IRC_Connect(void);
void IRC_Disconnect(void);
void IRC_AddMessage(const char* message);

#endif
