#ifndef _INDICATOR_H
#define _INDICATOR_H


typedef enum {
    IDKTstate_BOOTUP_SOLID_RED,//Bootup Process
    IDKTstate_UPLINK_BLINK_ORANGE,//Ready for uplink provisioning
    IDKTstate_WPSSTART_BLINK_BLUE,//WPS Processing
    IDKTstate_BINDING_BLINK_GREEN,//Uplink connected w/o binding
    IDKTstate_NORMAL_AUTO_SOLID_GREEN,//     Auto without Warnings
    IDKTstate_WARNING_AUTO_SOLID_ORANGE,//     Auto with Warnings
    IDKTstate_MANUAL_ALLOFF,    //     Manual Power off
    IDKTstate_FW_DOWNLOAD_BLINK_RED,    //User FW Download and Upgrade (OTA from Astra cloud)
    IDKTstate_FACTORY_RESET_SOLID_RED, //Being Factory Reset
    IDKTstate_HEALTH_CHK_LEDRUN, //Health Check
    IDKTstate_FW_WRITE_BLINK_SQUENCE,    //FW Upgrade Mode
    IDKTstate_MAX,
}IDKTstate;

typedef void (*IDKT_IOWrite)( int desc , char *buf , int buf_size );
typedef void (*IDKT_IORead)( int desc , char *buf , int buf_size );


void IDKT_Init(IDKT_IOWrite wr , IDKT_IORead rd );

#endif
