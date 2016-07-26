#ifndef LOGH 
#define LOGH

#include "mem.h"
#include "user_config.h"

int SyslogDial(const char *ip , int port);
void SyslogSendChar(char data );
void SyslogSendString( char *data );


#endif


