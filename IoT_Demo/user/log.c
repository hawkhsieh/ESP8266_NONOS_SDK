#include "log.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include <stdarg.h>

#include "espconn.h"


LOCAL struct espconn *syslogConn;


LOCAL void ICACHE_FLASH_ATTR
SyslogRecv(void *arg, char *pusrdata, unsigned short length)
{
}

void ICACHE_FLASH_ATTR
SyslogSendString( char *data )
{
    if ( syslogConn ){
        espconn_sendto(syslogConn, data , strlen(data));
    }
}

void ICACHE_FLASH_ATTR
SyslogSendBlock( char *data , int size )
{
    if ( syslogConn ){
        espconn_sendto(syslogConn, data , size);
    }
}

void ICACHE_FLASH_ATTR
SyslogSendChar( char data )
{
    if ( syslogConn ){
        espconn_sendto(syslogConn, &data , 1);
    }
}
/**
 * @brief SyslogDial
 *        使用UDP連線到參數ip:port，之後的log都會傳往這個syslog server
 * @param[in] ip
 * @param[in] port
 * @return 傳回Syslog指標，帶有這個udp連線的資訊
 */
int ICACHE_FLASH_ATTR
SyslogDial( const char *ip , int port )
{

    syslogConn = (struct espconn *)os_zalloc(sizeof(struct espconn));
    syslogConn->type = ESPCONN_UDP;
    syslogConn->state = ESPCONN_NONE;
    syslogConn->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));   
    syslogConn->proto.udp->local_port = espconn_port();

    syslogConn->proto.udp->remote_port = port;
    os_memcpy(syslogConn->proto.udp->remote_ip, ip, 4);

    uint8 ret;
    ret = espconn_regist_recvcb(syslogConn, SyslogRecv );
    os_printf("espconn_regist_recvcb ret=%d\n",ret);
    ret = espconn_create(syslogConn);
    os_printf("espconn_create ret=%d\n",ret);

    return 0;
}

