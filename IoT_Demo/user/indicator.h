#ifndef _INDICATOR_H
#define _INDICATOR_H


typedef enum{
    IDKTled_Off=1,
    IDKTled_On=0
}IDKTled;

typedef enum {
    IDKTgpio_RED=4,
    IDKTgpio_BLUE=2,
    IDKTgpio_GREEN=0,
    IDKTgpio_EOF=-1

}IDKTgpio;

typedef enum {
    IDKTstate_Init,//interal use
    IDKTstate_BOOTUP_SOLID_RED,//Bootup Process
    IDKTstate_UPLINK_BLINK_BLUE,//Ready for uplink provisioning

    IDKTstate_UPLINK_FAST_BLINK_BLUE,//resolve DNS


    IDKTstate_WIFISTART_FBLINK_GREEN,//WPS Processing
    IDKTstate_BINDING_BLINK_GREEN,//Uplink connected w/o binding
    IDKTstate_NORMAL_AUTO_SOLID_GREEN,//     Auto without Warnings
    IDKTstate_WARNING_AUTO_SOLID_BLUE,//     Auto with Warnings
    IDKTstate_MANUAL_ALLOFF,    //     Manual Power off
    IDKTstate_FW_DOWNLOAD_BLINK_RED,    //User FW Download and Upgrade (OTA from Astra cloud)
    IDKTstate_FACTORY_RESET_SOLID_RED, //Being Factory Reset
    IDKTstate_HEALTH_CHK_LEDRUN, //Health Check
    IDKTstate_FW_WRITE_BLINK_SQUENCE,    //FW Upgrade Mode
    IDKTstate_MAX,
}IDKTstate;

typedef void (*IDKT_IOWrite)( int desc , char *buf , int buf_size );
typedef void (*IDKT_IORead)( int desc , char *buf , int buf_size );
typedef void (*IDKT_IOPutout)( void );


void IDKT_Init(IDKT_IOWrite wr , IDKT_IORead rd , IDKT_IOPutout putout );

#endif
