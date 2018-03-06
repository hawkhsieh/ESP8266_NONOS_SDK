/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_esp_platform.c
 *
 * Description: The client mode configration.
 *              Check your hardware connection with the host while use this mode.
 *
 * Modification history:
 *     2014/5/09, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "espconn.h"
#include "user_esp_platform.h"
#include "user_iot_version.h"
#include "upgrade.h"
#include "smartconfig.h"

#include "log.h"
#include "boot.h"
#include "swt.h"
#include "indicator.h"
#include "gpio16.h"
#include "version.h"

#define SMARTCONFIG
#if ESP_PLATFORM

#define ESP_DEBUG

#ifdef ESP_DEBUG

#define ESP_DBG( fmt, args...) \
    do {\
        os_printf( "(%s:%d): " fmt, __FILE__ , __LINE__ ,##args);\
    } while(0)
#else
#define ESP_DBG
#endif

#define ACTIVE_FRAME    "{\"nonce\": %d,\"path\": \"/v1/device/activate/\", \"method\": \"POST\", \"body\": {\"encrypt_method\": \"PLAIN\", \"token\": \"%s\", \"bssid\": \""MACSTR"\",\"rom_version\":\"%s\"}, \"meta\": {\"Authorization\": \"token %s\"}}\n"

#if PLUG_DEVICE
#include "user_plug.h"

#define RESPONSE_FRAME  "{\"status\": 200, \"datapoint\": {\"x\": %d}, \"nonce\": %d, \"deliver_to_device\": true}\n"
#define FIRST_FRAME     "{\"nonce\": %d, \"path\": \"/v1/device/identify\", \"method\": \"GET\",\"meta\": {\"Authorization\": \"token %s\"}}\n"

#elif LIGHT_DEVICE
#include "user_light.h"

#define RESPONSE_FRAME  "{\"status\": 200,\"nonce\": %d, \"datapoint\": {\"x\": %d,\"y\": %d,\"z\": %d,\"k\": %d,\"l\": %d},\"deliver_to_device\":true}\n"
#define FIRST_FRAME     "{\"nonce\": %d, \"path\": \"/v1/device/identify\", \"method\": \"GET\",\"meta\": {\"Authorization\": \"token %s\"}}\n"

#elif SENSOR_DEVICE
#include "user_sensor.h"

#if HUMITURE_SUB_DEVICE
#define UPLOAD_FRAME  "{\"nonce\": %d, \"path\": \"/v1/datastreams/tem_hum/datapoint/\", \"method\": \"POST\", \
\"body\": {\"datapoint\": {\"x\": %s%d.%02d,\"y\": %d.%02d}}, \"meta\": {\"Authorization\": \"token %s\"}}\n"
#elif FLAMMABLE_GAS_SUB_DEVICE
#define UPLOAD_FRAME  "{\"nonce\": %d, \"path\": \"/v1/datastreams/flammable_gas/datapoint/\", \"method\": \"POST\", \
\"body\": {\"datapoint\": {\"x\": %d.%03d}}, \"meta\": {\"Authorization\": \"token %s\"}}\n"
#endif

LOCAL uint32 count = 0;
#endif

#define UPGRADE_FRAME  "{\"path\": \"/v1/messages/\", \"method\": \"POST\", \"meta\": {\"Authorization\": \"token %s\"},\
\"get\":{\"action\":\"%s\"},\"body\":{\"pre_rom_version\":\"%s\",\"rom_version\":\"%s\"}}\n"

#if PLUG_DEVICE || LIGHT_DEVICE
#define BEACON_FRAME    "{\"path\": \"/v1/ping/\", \"method\": \"POST\",\"meta\": {\"Authorization\": \"token %s\"}}\n"
#define RPC_RESPONSE_FRAME  "{\"status\": 200, \"nonce\": %d, \"deliver_to_device\": true}\n"
#define TIMER_FRAME     "{\"body\": {}, \"get\":{\"is_humanize_format_simple\":\"true\"},\"meta\": {\"Authorization\": \"Token %s\"},\"path\": \"/v1/device/timers/\",\"post\":{},\"method\": \"GET\"}\n"
#define pheadbuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Authorization: token %s\r\n\
Accept-Encoding: gzip,deflate,sdch\r\n\
Accept-Language: zh-CN,zh;q=0.8\r\n\r\n"

LOCAL uint8 ping_status;
LOCAL os_timer_t beacon_timer;
#endif

#ifdef USE_DNS
ip_addr_t esp_server_ip;
#endif

LOCAL struct espconn user_conn;
LOCAL struct _esp_tcp user_tcp;
LOCAL os_timer_t client_timer;
 struct esp_platform_saved_param esp_param;
LOCAL uint8 device_status;
LOCAL uint8 device_recon_count = 0;
LOCAL uint32 active_nonce = 0;
LOCAL uint8 iot_version[20] = {0};
struct rst_info rtc_info;
void user_esp_platform_check_ip(uint8 reset_flag);
LOCAL os_timer_t boot_fail_timer;


void system_restart_delay(){

    os_delay_us(2000000);
    system_restart();
}
/******************************************************************************
 * FunctionName : user_esp_platform_get_token
 * Description  : get the espressif's device token
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_get_token(uint8_t *token)
{
    if (token == NULL) {
        return;
    }

    os_memcpy(token, esp_param.token, sizeof(esp_param.token));
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_token
 * Description  : save the token for the espressif's device
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_set_token(uint8_t *token)
{
    if (token == NULL) {
        return;
    }

    esp_param.activeflag = 0;
    os_memcpy(esp_param.token, token, os_strlen(token));

    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_active
 * Description  : set active flag
 * Parameters   : activeflag -- 0 or 1
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_set_active(uint8 activeflag)
{
    esp_param.activeflag = activeflag;

    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
}

void ICACHE_FLASH_ATTR
user_esp_platform_set_connect_status(uint8 status)
{
    device_status = status;
}

/******************************************************************************
 * FunctionName : user_esp_platform_get_connect_status
 * Description  : get each connection step's status
 * Parameters   : none
 * Returns      : status
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
user_esp_platform_get_connect_status(void)
{
    uint8 status = wifi_station_get_connect_status();

    if (status == STATION_GOT_IP) {
        status = (device_status == 0) ? DEVICE_CONNECTING : device_status;
    }

    ESP_DBG("status %d\n", status);
    return status;
}

/******************************************************************************
 * FunctionName : user_esp_platform_parse_nonce
 * Description  : parse the device nonce
 * Parameters   : pbuffer -- the recivce data point
 * Returns      : the nonce
*******************************************************************************/
int ICACHE_FLASH_ATTR
user_esp_platform_parse_nonce(char *pbuffer)
{
    char *pstr = NULL;
    char *pparse = NULL;
    char noncestr[11] = {0};
    int nonce = 0;
    pstr = (char *)os_strstr(pbuffer, "\"nonce\": ");

    if (pstr != NULL) {
        pstr += 9;
        pparse = (char *)os_strstr(pstr, ",");

        if (pparse != NULL) {
            os_memcpy(noncestr, pstr, pparse - pstr);
        } else {
            pparse = (char *)os_strstr(pstr, "}");

            if (pparse != NULL) {
                os_memcpy(noncestr, pstr, pparse - pstr);
            } else {
                pparse = (char *)os_strstr(pstr, "]");

                if (pparse != NULL) {
                    os_memcpy(noncestr, pstr, pparse - pstr);
                } else {
                    return 0;
                }
            }
        }

        nonce = atoi(noncestr);
    }

    return nonce;
}

/******************************************************************************
 * FunctionName : user_esp_platform_get_info
 * Description  : get and update the espressif's device status
 * Parameters   : pespconn -- the espconn used to connect with host
 *                pbuffer -- prossing the data point
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_get_info(struct espconn *pconn, uint8 *pbuffer)
{
    char *pbuf = NULL;
    int nonce = 0;

    pbuf = (char *)os_zalloc(packet_size);

    nonce = user_esp_platform_parse_nonce(pbuffer);

    if (pbuf != NULL) {
#if PLUG_DEVICE
        os_sprintf(pbuf, RESPONSE_FRAME, user_plug_get_status(), nonce);
#elif LIGHT_DEVICE
        uint32 white_val;
        white_val = (PWM_CHANNEL>LIGHT_COLD_WHITE?user_light_get_duty(LIGHT_COLD_WHITE):0);
        os_sprintf(pbuf, RESPONSE_FRAME, nonce, user_light_get_period(),
                   user_light_get_duty(LIGHT_RED), user_light_get_duty(LIGHT_GREEN),
                   user_light_get_duty(LIGHT_BLUE),white_val );//50);
#endif

        ESP_DBG("%s\n", pbuf);
#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pconn, pbuf, os_strlen(pbuf));
#endif
        os_free(pbuf);
        pbuf = NULL;
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_info
 * Description  : prossing the data and controling the espressif's device
 * Parameters   : pespconn -- the espconn used to connect with host
 *                pbuffer -- prossing the data point
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_set_info(struct espconn *pconn, uint8 *pbuffer)
{
#if PLUG_DEVICE
    char *pstr = NULL;
    pstr = (char *)os_strstr(pbuffer, "plug-status");

    if (pstr != NULL) {
        pstr = (char *)os_strstr(pbuffer, "body");

        if (pstr != NULL) {

            if (os_strncmp(pstr + 27, "1", 1) == 0) {
                user_plug_set_status(0x01);
            } else if (os_strncmp(pstr + 27, "0", 1) == 0) {
                user_plug_set_status(0x00);
            }
        }
    }

#elif LIGHT_DEVICE
    char *pstr = NULL;
    char *pdata = NULL;
    char *pbuf = NULL;
    char recvbuf[10];
    uint16 length = 0;
    uint32 data = 0;
    static uint32 rr,gg,bb,cw,ww,period;
    ww=0;
    cw=0;
    extern uint8 light_sleep_flg;
    pstr = (char *)os_strstr(pbuffer, "\"path\": \"/v1/datastreams/light/datapoint/\"");

    if (pstr != NULL) {
        pstr = (char *)os_strstr(pbuffer, "{\"datapoint\": ");

        if (pstr != NULL) {
            pbuf = (char *)os_strstr(pbuffer, "}}");
            length = pbuf - pstr;
            length += 2;
            pdata = (char *)os_zalloc(length + 1);
            os_memcpy(pdata, pstr, length);

            pstr = (char *)os_strchr(pdata, 'x');

            if (pstr != NULL) {
                pstr += 4;
                pbuf = (char *)os_strchr(pstr, ',');

                if (pbuf != NULL) {
                    length = pbuf - pstr;
                    os_memset(recvbuf, 0, 10);
                    os_memcpy(recvbuf, pstr, length);
                    data = atoi(recvbuf);
                    period = data;
                    //user_light_set_period(data);
                }
            }

            pstr = (char *)os_strchr(pdata, 'y');

            if (pstr != NULL) {
                pstr += 4;
                pbuf = (char *)os_strchr(pstr, ',');

                if (pbuf != NULL) {
                    length = pbuf - pstr;
                    os_memset(recvbuf, 0, 10);
                    os_memcpy(recvbuf, pstr, length);
                    data = atoi(recvbuf);
                    rr=data;
                    os_printf("r: %d\r\n",rr);
                    //user_light_set_duty(data, 0);
                }
            }

            pstr = (char *)os_strchr(pdata, 'z');

            if (pstr != NULL) {
                pstr += 4;
                pbuf = (char *)os_strchr(pstr, ',');

                if (pbuf != NULL) {
                    length = pbuf - pstr;
                    os_memset(recvbuf, 0, 10);
                    os_memcpy(recvbuf, pstr, length);
                    data = atoi(recvbuf);
                    gg=data;
                    os_printf("g: %d\r\n",gg);
                    //user_light_set_duty(data, 1);
                }
            }

            pstr = (char *)os_strchr(pdata, 'k');

            if (pstr != NULL) {
                pstr += 4;;
                pbuf = (char *)os_strchr(pstr, ',');

                if (pbuf != NULL) {
                    length = pbuf - pstr;
                    os_memset(recvbuf, 0, 10);
                    os_memcpy(recvbuf, pstr, length);
                    data = atoi(recvbuf);
                    bb=data;
                    os_printf("b: %d\r\n",bb);
                    //user_light_set_duty(data, 2);
                }
            }

            pstr = (char *)os_strchr(pdata, 'l');

            if (pstr != NULL) {
                pstr += 4;;
                pbuf = (char *)os_strchr(pstr, ',');

                if (pbuf != NULL) {
                    length = pbuf - pstr;
                    os_memset(recvbuf, 0, 10);
                    os_memcpy(recvbuf, pstr, length);
                    data = atoi(recvbuf);
                    cw=data;
		      ww=data;
                    os_printf("cw: %d\r\n",cw);
		      os_printf("ww:%d\r\n",ww);   //chg
                    //user_light_set_duty(data, 2);
                }
            }

            os_free(pdata);
        }
    }
    
    if((rr|gg|bb|cw|ww) == 0){
        if(light_sleep_flg==0){

        }
        
    }else{
        if(light_sleep_flg==1){
            os_printf("modem sleep en\r\n");
            wifi_set_sleep_type(MODEM_SLEEP_T);
            light_sleep_flg =0;
        }
    }

    light_set_aim(rr,gg,bb,cw,ww,period);
    //user_light_restart();

#endif

    user_esp_platform_get_info(pconn, pbuffer);
}

/******************************************************************************
 * FunctionName : user_esp_platform_reconnect
 * Description  : reconnect with host after get ip
 * Parameters   : pespconn -- the espconn used to reconnect with host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_reconnect(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_reconnect\n");

    user_esp_platform_check_ip(0);
}

/******************************************************************************
 * FunctionName : user_esp_platform_discon_cb
 * Description  : disconnect successfully with the host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_discon_cb(void *arg)
{
    struct espconn *pespconn = arg;
    struct ip_info ipconfig;
	struct dhcp_client_info dhcp_info;
    ESP_DBG("user_esp_platform_discon_cb\n");

#if (PLUG_DEVICE || LIGHT_DEVICE)
    os_timer_disarm(&beacon_timer);
#endif

    if (pespconn == NULL) {
        return;
    }

    pespconn->proto.tcp->local_port = espconn_port();

#if (PLUG_DEVICE || SENSOR_DEVICE)
    user_link_led_output(1);
#endif

#if SENSOR_DEVICE
#ifdef SENSOR_DEEP_SLEEP

    if (wifi_get_opmode() == STATION_MODE) {
    	/***add by tzx for saving ip_info to avoid dhcp_client start****/
    	wifi_get_ip_info(STATION_IF, &ipconfig);

    	dhcp_info.ip_addr = ipconfig.ip;
    	dhcp_info.netmask = ipconfig.netmask;
    	dhcp_info.gw = ipconfig.gw ;
    	dhcp_info.flag = 0x01;
    	os_printf("dhcp_info.ip_addr = %d\n",dhcp_info.ip_addr);
    	system_rtc_mem_write(64,&dhcp_info,sizeof(struct dhcp_client_info));
        user_sensor_deep_sleep_enter();
    } else {
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
        os_timer_arm(&client_timer, SENSOR_DEEP_SLEEP_TIME / 1000, 0);
    }

#else
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
    os_timer_arm(&client_timer, 1000, 0);
#endif
#else
    user_esp_platform_reconnect(pespconn);
#endif
}

/******************************************************************************
 * FunctionName : user_esp_platform_discon
 * Description  : A new incoming connection has been disconnected.
 * Parameters   : espconn -- the espconn used to disconnect with host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_discon(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_discon\n");

#if (PLUG_DEVICE || SENSOR_DEVICE)
    user_link_led_output(1);
#endif

#ifdef CLIENT_SSL_ENABLE
    espconn_secure_disconnect(pespconn);
#else
    espconn_disconnect(pespconn);
#endif
}

/******************************************************************************
 * FunctionName : user_esp_platform_sent_cb
 * Description  : Data has been sent successfully and acknowledged by the remote host.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_sent_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_sent_cb\n");
}

/******************************************************************************
 * FunctionName : user_esp_platform_sent
 * Description  : Processing the application data and sending it to the host
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_sent(struct espconn *pespconn)
{
    uint8 devkey[token_size] = {0};
    uint32 nonce;
    char *pbuf = (char *)os_zalloc(packet_size);

    os_memcpy(devkey, esp_param.devkey, 40);

    if (esp_param.activeflag == 0xFF) {
        esp_param.activeflag = 0;
    }

    if (pbuf != NULL) {
        if (esp_param.activeflag == 0) {
            uint8 token[token_size] = {0};
            uint8 bssid[6];
            active_nonce = os_random() & 0x7FFFFFFF;

            os_memcpy(token, esp_param.token, 40);

            wifi_get_macaddr(STATION_IF, bssid);

            os_sprintf(pbuf, ACTIVE_FRAME, active_nonce, token, MAC2STR(bssid),iot_version, devkey);
        }

#if SENSOR_DEVICE
#if HUMITURE_SUB_DEVICE
        else {
#if 0
            uint16 tp, rh;
            uint8 data[4];

            if (user_mvh3004_read_th(data)) {
                rh = data[0] << 8 | data[1];
                tp = data[2] << 8 | data[3];
            }

#else
            uint16 tp, rh;
            uint8 *data;
            uint32 tp_t, rh_t;
            data = (uint8 *)user_mvh3004_get_poweron_th();

            rh = data[0] << 8 | data[1];
            tp = data[2] << 8 | data[3];
#endif
            tp_t = (tp >> 2) * 165 * 100 / (16384 - 1);
            rh_t = (rh & 0x3fff) * 100 * 100 / (16384 - 1);

            if (tp_t >= 4000) {
                os_sprintf(pbuf, UPLOAD_FRAME, count, "", tp_t / 100 - 40, tp_t % 100, rh_t / 100, rh_t % 100, devkey);
            } else {
                tp_t = 4000 - tp_t;
                os_sprintf(pbuf, UPLOAD_FRAME, count, "-", tp_t / 100, tp_t % 100, rh_t / 100, rh_t % 100, devkey);
            }
        }

#elif FLAMMABLE_GAS_SUB_DEVICE
        else {
            uint32 adc_value = system_adc_read();

            os_sprintf(pbuf, UPLOAD_FRAME, count, adc_value / 1024, adc_value * 1000 / 1024, devkey);
        }

#endif
#else
        else {
            nonce = os_random() & 0x7FFFFFFF;
            os_sprintf(pbuf, FIRST_FRAME, nonce , devkey);
        }

#endif
        ESP_DBG("%s\n", pbuf);

#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif

        os_free(pbuf);
    }
}

#if PLUG_DEVICE || LIGHT_DEVICE
/******************************************************************************
 * FunctionName : user_esp_platform_sent_beacon
 * Description  : sent beacon frame for connection with the host is activate
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_sent_beacon(struct espconn *pespconn)
{
    if (pespconn == NULL) {
        return;
    }

    if (pespconn->state == ESPCONN_CONNECT) {
        if (esp_param.activeflag == 0) {
            ESP_DBG("please check device is activated.\n");
            user_esp_platform_sent(pespconn);
        } else {
            uint8 devkey[token_size] = {0};
            os_memcpy(devkey, esp_param.devkey, 40);

            ESP_DBG("user_esp_platform_sent_beacon %u\n", system_get_time());

            if (ping_status == 0) {
                ESP_DBG("user_esp_platform_sent_beacon sent fail!\n");
                user_esp_platform_discon(pespconn);
            } else {
                char *pbuf = (char *)os_zalloc(packet_size);

                if (pbuf != NULL) {
                    os_sprintf(pbuf, BEACON_FRAME, devkey);

#ifdef CLIENT_SSL_ENABLE
                    espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
                    espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif

                    ping_status = 0;
                    os_timer_arm(&beacon_timer, BEACON_TIME, 0);
                    os_free(pbuf);
                }
            }
        }
    } else {
        ESP_DBG("user_esp_platform_sent_beacon sent fail!\n");
        user_esp_platform_discon(pespconn);
    }
}

/******************************************************************************
 * FunctionName : user_platform_rpc_set_rsp
 * Description  : response the message to server to show setting info is received
 * Parameters   : pespconn -- the espconn used to connetion with the host
 *                nonce -- mark the message received from server
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_platform_rpc_set_rsp(struct espconn *pespconn, int nonce)
{
    char *pbuf = (char *)os_zalloc(packet_size);

    if (pespconn == NULL) {
        return;
    }

    os_sprintf(pbuf, RPC_RESPONSE_FRAME, nonce);
    ESP_DBG("%s\n", pbuf);
#ifdef CLIENT_SSL_ENABLE
    espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
    espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif
    os_free(pbuf);
}

/******************************************************************************
 * FunctionName : user_platform_timer_get
 * Description  : get the timers from server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_platform_timer_get(struct espconn *pespconn)
{
    uint8 devkey[token_size] = {0};
    char *pbuf = (char *)os_zalloc(packet_size);
    os_memcpy(devkey, esp_param.devkey, 40);

    if (pespconn == NULL) {
        return;
    }

    os_sprintf(pbuf, TIMER_FRAME, devkey);
    ESP_DBG("%s\n", pbuf);
#ifdef CLIENT_SSL_ENABLE
    espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
    espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif
    os_free(pbuf);
}

/******************************************************************************
 * FunctionName : user_esp_platform_upgrade_cb
 * Description  : Processing the downloaded data from the server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_upgrade_rsp(void *arg)
{
    struct upgrade_server_info *server = arg;
    struct espconn *pespconn = server->pespconn;
    uint8 devkey[41] = {0};
    uint8 *pbuf = NULL;
    char *action = NULL;

    os_memcpy(devkey, esp_param.devkey, 40);
    pbuf = (char *)os_zalloc(packet_size);

    if (server->upgrade_flag == true) {
        ESP_DBG("user_esp_platform_upgarde_successfully\n");
        action = "device_upgrade_success";
        os_sprintf(pbuf, UPGRADE_FRAME, devkey, action, server->pre_version, server->upgrade_version);
        ESP_DBG("%s\n",pbuf);

#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif

        if (pbuf != NULL) {
            os_free(pbuf);
            pbuf = NULL;
        }
    } else {
        ESP_DBG("user_esp_platform_upgrade_failed\n");
        action = "device_upgrade_failed";
        os_sprintf(pbuf, UPGRADE_FRAME, devkey, action,server->pre_version, server->upgrade_version);
        ESP_DBG("%s\n",pbuf);

#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif

        if (pbuf != NULL) {
            os_free(pbuf);
            pbuf = NULL;
        }
    }

    os_free(server->url);
    server->url = NULL;
    os_free(server);
    server = NULL;
}

/******************************************************************************
 * FunctionName : user_esp_platform_upgrade_begin
 * Description  : Processing the received data from the server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 *                server -- upgrade param
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_upgrade_begin(struct espconn *pespconn, struct upgrade_server_info *server)
{
    uint8 user_bin[9] = {0};
    uint8 devkey[41] = {0};

    server->pespconn = pespconn;

    os_memcpy(devkey, esp_param.devkey, 40);
    os_memcpy(server->ip, pespconn->proto.tcp->remote_ip, 4);

#ifdef UPGRADE_SSL_ENABLE
    server->port = 443;
#else
    server->port = 80;
#endif

    server->check_cb = user_esp_platform_upgrade_rsp;
    server->check_times = 120000;

    if (server->url == NULL) {
        server->url = (uint8 *)os_zalloc(512);
    }

    if (system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
        os_memcpy(user_bin, "user2.bin", 10);
    } else if (system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
        os_memcpy(user_bin, "user1.bin", 10);
    }

    os_sprintf(server->url, "GET /v1/device/rom/?action=download_rom&version=%s&filename=%s HTTP/1.0\r\nHost: "IPSTR":%d\r\n"pheadbuffer"",
               server->upgrade_version, user_bin, IP2STR(server->ip),
               server->port, devkey);
    ESP_DBG("%s\n",server->url);

#ifdef UPGRADE_SSL_ENABLE

    if (system_upgrade_start_ssl(server) == false) {
#else

    if (system_upgrade_start(server) == false) {
#endif
        ESP_DBG("upgrade is already started\n");
    }
}
#endif

/******************************************************************************
 * FunctionName : user_esp_platform_recv_cb
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
    char *pstr = NULL;
    LOCAL char pbuffer[1024 * 2] = {0};
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_recv_cb %s\n", pusrdata);

#if (PLUG_DEVICE || LIGHT_DEVICE)
    os_timer_disarm(&beacon_timer);
#endif

    if (length == 1460) {
        os_memcpy(pbuffer, pusrdata, length);
    } else {
        struct espconn *pespconn = (struct espconn *)arg;

        os_memcpy(pbuffer + os_strlen(pbuffer), pusrdata, length);

        if ((pstr = (char *)os_strstr(pbuffer, "\"activate_status\": ")) != NULL &&
                user_esp_platform_parse_nonce(pbuffer) == active_nonce) {
            if (os_strncmp(pstr + 19, "1", 1) == 0) {
                ESP_DBG("device activates successful.\n");

                device_status = DEVICE_ACTIVE_DONE;
                esp_param.activeflag = 1;
                system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
                user_esp_platform_sent(pespconn);
		  if(LIGHT_DEVICE){
                    system_restart_delay();
                }
            } else {
                ESP_DBG("device activates failed.\n");
                device_status = DEVICE_ACTIVE_FAIL;
            }
        }

#if (PLUG_DEVICE || LIGHT_DEVICE)
        else if ((pstr = (char *)os_strstr(pbuffer, "\"action\": \"sys_upgrade\"")) != NULL) {
            if ((pstr = (char *)os_strstr(pbuffer, "\"version\":")) != NULL) {
                struct upgrade_server_info *server = NULL;
                int nonce = user_esp_platform_parse_nonce(pbuffer);
                user_platform_rpc_set_rsp(pespconn, nonce);

                server = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));
                os_memcpy(server->upgrade_version, pstr + 12, 16);
                server->upgrade_version[15] = '\0';
                os_sprintf(server->pre_version,"%s%d.%d.%dt%d(%s)",VERSION_TYPE,IOT_VERSION_MAJOR,\
                    	IOT_VERSION_MINOR,IOT_VERSION_REVISION,device_type,UPGRADE_FALG);
                user_esp_platform_upgrade_begin(pespconn, server);
            }
        } else if ((pstr = (char *)os_strstr(pbuffer, "\"action\": \"sys_reboot\"")) != NULL) {
            os_timer_disarm(&client_timer);
            os_timer_setfn(&client_timer, (os_timer_func_t *)system_upgrade_reboot, NULL);
            os_timer_arm(&client_timer, 1000, 0);
        } else if ((pstr = (char *)os_strstr(pbuffer, "/v1/device/timers/")) != NULL) {
            int nonce = user_esp_platform_parse_nonce(pbuffer);
            user_platform_rpc_set_rsp(pespconn, nonce);
            os_timer_disarm(&client_timer);
            os_timer_setfn(&client_timer, (os_timer_func_t *)user_platform_timer_get, pespconn);
            os_timer_arm(&client_timer, 2000, 0);
        } else if ((pstr = (char *)os_strstr(pbuffer, "\"method\": ")) != NULL) {
            if (os_strncmp(pstr + 11, "GET", 3) == 0) {
                user_esp_platform_get_info(pespconn, pbuffer);
            } else if (os_strncmp(pstr + 11, "POST", 4) == 0) {
                user_esp_platform_set_info(pespconn, pbuffer);
            }
        } else if ((pstr = (char *)os_strstr(pbuffer, "ping success")) != NULL) {
            ESP_DBG("ping success\n");
            ping_status = 1;
        } else if ((pstr = (char *)os_strstr(pbuffer, "send message success")) != NULL) {
        } else if ((pstr = (char *)os_strstr(pbuffer, "timers")) != NULL) {
            user_platform_timer_start(pusrdata , pespconn);
        }

#elif SENSOR_DEVICE
        else if ((pstr = (char *)os_strstr(pbuffer, "\"status\":")) != NULL) {
            if (os_strncmp(pstr + 10, "200", 3) != 0) {
                ESP_DBG("message upload failed.\n");
            } else {
                count++;
                ESP_DBG("message upload sucessful.\n");
            }

            os_timer_disarm(&client_timer);
            os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_discon, pespconn);
            os_timer_arm(&client_timer, 10, 0);
        }

#endif
        else if ((pstr = (char *)os_strstr(pbuffer, "device")) != NULL) {
#if PLUG_DEVICE || LIGHT_DEVICE
            user_platform_timer_get(pespconn);
#elif SENSOR_DEVICE

#endif
        }

        os_memset(pbuffer, 0, sizeof(pbuffer));
    }

#if (PLUG_DEVICE || LIGHT_DEVICE)
    os_timer_arm(&beacon_timer, BEACON_TIME, 0);
#endif
}

#if AP_CACHE
/******************************************************************************
 * FunctionName : user_esp_platform_ap_change
 * Description  : add the user interface for changing to next ap ID.
 * Parameters   :
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_ap_change(void)
{
    uint8 current_id;
    uint8 i = 0;
    ESP_DBG("user_esp_platform_ap_is_changing\n");

    current_id = wifi_station_get_current_ap_id();
    ESP_DBG("current ap id =%d\n", current_id);

    if (current_id == AP_CACHE_NUMBER - 1) {
       i = 0;
    } else {
       i = current_id + 1;
    }
    while (wifi_station_ap_change(i) != true) {
       i++;
       if (i == AP_CACHE_NUMBER - 1) {
    	   i = 0;
       }
    }

    /* just need to re-check ip while change AP */
    device_recon_count = 0;
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
    os_timer_arm(&client_timer, 100, 0);
}
#endif


#if 0   //日星與河嶽合併，不需要了
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
#endif


typedef enum {
    WifiState_SmartConfigIdle,
    WifiState_SmartConfig,
    WifiState_SmartConfigAP

}WifiState;

WifiState wifi_state;


void ICACHE_FLASH_ATTR
smartconfig_done_rma(sc_status status, void *pdata)
{
    static struct station_config keepConfig;
    switch(status) {
    case SC_STATUS_LINK:
        os_printf("SC_STATUS_LINK\n");
        struct station_config *sta_conf = pdata;
        keepConfig = *sta_conf;
        wifi_station_set_config(sta_conf);
        wifi_station_disconnect();
        wifi_station_connect();
        wifi_state=WifiState_SmartConfigAP;
        break;
    }
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
            IDKT_SetState(IDKTstate_WIFISTART_FBLINK_GREEN);
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
            wifi_state=WifiState_SmartConfigAP;
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
            rboot_set_current_fw(FIRMWARE_RTOS);
            system_restart_delay();

            break;
    }

}


LOCAL bool ICACHE_FLASH_ATTR
user_esp_platform_reset_mode(void)
{
    switch(wifi_state){

    case WifiState_SmartConfigIdle:
        os_timer_disarm(&boot_fail_timer);
        //只要沒連上AP，就開啟smartconfig
        ESP_DBG("wifi_state=WifiState_SmartConfigIdle\n");
        wifi_set_opmode( STATION_MODE );
        smartconfig_set_type(SC_TYPE_ESPTOUCH);
        smartconfig_start(smartconfig_done);
        IDKT_SetState(IDKTstate_UPLINK_BLINK_BLUE);
        wifi_state=WifiState_SmartConfig;
        break;
    case WifiState_SmartConfig:
        ESP_DBG("wifi_state=WifiState_SmartConfig\n");

        break;
    case WifiState_SmartConfigAP:
    {
        smartconfig_stop();
        wifi_station_disconnect();
        ESP_DBG("wifi_state=WifiState_SmartConfigAP,rwifi_station_disconnect\n");
        wifi_state=WifiState_SmartConfigIdle;
        break;

    }
    }
#if 0 //user_esp_platform_ap_change call了會掛掉
#if AP_CACHE
    /* delay 5s to change AP */
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_ap_change, NULL);
    os_timer_arm(&client_timer, 5000, 0);

    return true;
#endif
#endif

    return false;
}

/******************************************************************************
 * FunctionName : user_esp_platform_recon_cb
 * Description  : The connection had an error and is already deallocated.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_recon_cb(void *arg, sint8 err)
{
    struct espconn *pespconn = (struct espconn *)arg;

    ESP_DBG("user_esp_platform_recon_cb\n");

#if (PLUG_DEVICE || LIGHT_DEVICE)
    os_timer_disarm(&beacon_timer);
#endif

#if (PLUG_DEVICE || SENSOR_DEVICE)
    user_link_led_output(1);
#endif

    if (++device_recon_count == 5) {
        device_status = DEVICE_CONNECT_SERVER_FAIL;

        if (user_esp_platform_reset_mode()) {
            return;
        }
    }

#if SENSOR_DEVICE
#ifdef SENSOR_DEEP_SLEEP

    if (wifi_get_opmode() == STATION_MODE) {
        user_esp_platform_reset_mode();

        //user_sensor_deep_sleep_enter();
    } else {
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
        os_timer_arm(&client_timer, 1000, 0);
    }

#else
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
    os_timer_arm(&client_timer, 1000, 0);
#endif
#else
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
    os_timer_arm(&client_timer, 1000, 0);
#endif
}

/******************************************************************************
 * FunctionName : user_esp_platform_connect_cb
 * Description  : A new incoming connection has been connected.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_connect_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_connect_cb\n");
    if (wifi_get_opmode() ==  STATIONAP_MODE ) {
        os_printf("%s:%d\n",__FILE__,__LINE__);
        wifi_set_opmode(STATION_MODE);
    }

#if (PLUG_DEVICE || SENSOR_DEVICE)
    user_link_led_timer_done();
#endif
    device_recon_count = 0;
    espconn_regist_recvcb(pespconn, user_esp_platform_recv_cb);
    espconn_regist_sentcb(pespconn, user_esp_platform_sent_cb);
    user_esp_platform_sent(pespconn);
}

/******************************************************************************
 * FunctionName : user_esp_platform_connect
 * Description  : The function given as the connect with the host
 * Parameters   : espconn -- the espconn used to connect the connection
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_connect(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_connect\n");

#ifdef CLIENT_SSL_ENABLE
    espconn_secure_connect(pespconn);
#else
    espconn_connect(pespconn);
#endif
}

#ifdef USE_DNS
/******************************************************************************
 * FunctionName : user_esp_platform_dns_found
 * Description  : dns found callback
 * Parameters   : name -- pointer to the name that was looked up.
 *                ipaddr -- pointer to an ip_addr_t containing the IP address of
 *                the hostname, or NULL if the name could not be found (or on any
 *                other error).
 *                callback_arg -- a user-specified callback argument passed to
 *                dns_gethostbyname
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;

    if (ipaddr == NULL) {
        ESP_DBG("user_esp_platform_dns_found NULL\n");

        if (++device_recon_count == 5) {
            device_status = DEVICE_CONNECT_SERVER_FAIL;

            user_esp_platform_reset_mode();
        }

        return;
    }

    ESP_DBG("user_esp_platform_dns_found %d.%d.%d.%d\n",
            *((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
            *((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));

    if (esp_server_ip.addr == 0 && ipaddr->addr != 0) {
        os_timer_disarm(&client_timer);
        esp_server_ip.addr = ipaddr->addr;
        os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);

        pespconn->proto.tcp->local_port = espconn_port();

#ifdef CLIENT_SSL_ENABLE
        pespconn->proto.tcp->remote_port = 8443;
#else
        pespconn->proto.tcp->remote_port = 8000;
#endif

#if (PLUG_DEVICE || LIGHT_DEVICE)
        ping_status = 1;
#endif

        espconn_regist_connectcb(pespconn, user_esp_platform_connect_cb);
        espconn_regist_disconcb(pespconn, user_esp_platform_discon_cb);
        espconn_regist_reconcb(pespconn, user_esp_platform_recon_cb);
        user_esp_platform_connect(pespconn);
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_dns_check_cb
 * Description  : 1s time callback to check dns found
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_dns_check_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_dns_check_cb\n");

    espconn_gethostbyname(pespconn, ESP_DOMAIN, &esp_server_ip, user_esp_platform_dns_found);

    os_timer_arm(&client_timer, 1000, 0);
}

LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_start_dns(struct espconn *pespconn)
{
    IDKT_SetState(IDKTstate_UPLINK_FAST_BLINK_BLUE );

    esp_server_ip.addr = 0;
    espconn_gethostbyname(pespconn, ESP_DOMAIN, &esp_server_ip, user_esp_platform_dns_found);

    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_dns_check_cb, pespconn);
    os_timer_arm(&client_timer, 1000, 0);
}
#endif


#if LIGHT_DEVICE
void user_mdns_conf()
{

struct ip_info ipconfig;
wifi_get_ip_info(STATION_IF, &ipconfig);

struct mdns_info *info = (struct mdns_info *)os_zalloc(sizeof(struct mdns_info));
info->host_name = "espressif_light_demo";
info->ipAddr= ipconfig.ip.addr; //sation ip
info->server_name = "espLight";
info->server_port = 80;
info->txt_data[0] = "version = 1.0.1";
espconn_mdns_init(info);


}
#endif

void ICACHE_FLASH_ATTR
boot_failed_alarm_func(uint8 reset_flag)
{
    ESP_DBG("Time's up tuen on RED light\n");
    IDKT_SetState(IDKTstate_BOOTUP_SOLID_RED);
    os_timer_disarm(&boot_fail_timer);
}

/******************************************************************************
 * FunctionName : user_esp_platform_check_ip
 * Description  : espconn struct parame init when get ip addr
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_check_ip(uint8 reset_flag)
{
    struct ip_info ipconfig;

    char rma;
    int last_rom;

    rboot_get_rma(&rma,&last_rom);

    ESP_DBG("ROM=%d,RMA=%c,version=%s-%s\n",last_rom,rma, TAG_VERSION ,HASH_VERSION);

    //ESP_DBG("reset_flag=%d\n",reset_flag);
    os_timer_disarm(&client_timer);

    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
#if (PLUG_DEVICE || SENSOR_DEVICE)
        user_link_led_timer_init();
#endif

//***************************
#if LIGHT_DEVICE
	user_mdns_conf();
#endif
//***************************
        user_conn.proto.tcp = &user_tcp;
        user_conn.type = ESPCONN_TCP;
        user_conn.state = ESPCONN_NONE;

        device_status = DEVICE_CONNECTING;

        if (reset_flag) {
            device_recon_count = 0;
        }

#if 0
#if (PLUG_DEVICE || LIGHT_DEVICE)
        os_timer_disarm(&beacon_timer);
        os_timer_setfn(&beacon_timer, (os_timer_func_t *)user_esp_platform_sent_beacon, &user_conn);
#endif
#endif


#ifdef USE_DNS
        user_esp_platform_start_dns(&user_conn);
#else
        const char esp_server_ip[4] = {114, 215, 177, 97};

        os_memcpy(user_conn.proto.tcp->remote_ip, esp_server_ip, 4);
        user_conn.proto.tcp->local_port = espconn_port();

#ifdef CLIENT_SSL_ENABLE
        user_conn.proto.tcp->remote_port = 8443;
#else
        user_conn.proto.tcp->remote_port = 8000;
#endif

        espconn_regist_connectcb(&user_conn, user_esp_platform_connect_cb);
        espconn_regist_reconcb(&user_conn, user_esp_platform_recon_cb);
        user_esp_platform_connect(&user_conn);
#endif

        rboot_set_current_fw(FIRMWARE_RTOS);
        system_restart_delay();

    } else {

        /* if there are wrong while connecting to some AP, then reset mode */
        if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
             wifi_station_get_connect_status() == STATION_IDLE ||
             wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
             wifi_station_get_connect_status() == STATION_CONNECT_FAIL )) {
            user_esp_platform_reset_mode();
        }

        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
        os_timer_arm(&client_timer, 1000, 0);

    }
}



LOCAL void ICACHE_FLASH_ATTR
user_wps_status_cb(int status)
{
    ESP_DBG("user_wps_status_cb(status:%d)\n",status);


    switch (status) {
    case WPS_CB_ST_SUCCESS:
        wifi_wps_disable();
        wifi_station_connect();
        break;
    case WPS_CB_ST_FAILED:
    case WPS_CB_ST_TIMEOUT:
    default:
        os_timer_disarm(&boot_fail_timer);
        smartconfig_start(smartconfig_done);
        IDKT_SetState(IDKTstate_UPLINK_BLINK_BLUE);
        //            wifi_wps_start();
        break;
    }
}

LOCAL void ICACHE_FLASH_ATTR
user_wps_key_short_press(void)
{
    ESP_DBG("user_wps_key_short_press\n");
}

LOCAL void ICACHE_FLASH_ATTR
user_wps_key_long_press(void)
{
    IDKT_SetState(IDKTstate_WIFISTART_FBLINK_GREEN);
    smartconfig_stop();
    ESP_DBG("user_wps_key_long_press\n");
    wifi_wps_disable();
    wifi_wps_enable(WPS_TYPE_PBC);
    wifi_set_wps_cb(user_wps_status_cb);
    wifi_wps_start();

}


LOCAL void ICACHE_FLASH_ATTR
factory_reset(void)
{
    spi_flash_erase_sector( CONFIG_ADDR );
    spi_flash_erase_sector( UARTKEY_ADDR );
    system_restore();
    system_restart_delay();
}


LOCAL void ICACHE_FLASH_ATTR
doRma(void *timer_arg)
{
    IDKT_SetState(IDKTstate_MANUAL_ALLOFF);

    static int rotate;
    ESP_DBG("doRma rotate=%d\n",rotate);

    if ( GPIO_INPUT_GET(GPIO_ID_PIN(WPS_IO_NUM)) == 0 ){

        rboot_set_rma('O');
        rboot_set_current_fw(FIRMWARE_RTOS);
        system_restart_delay();
     //   ESP_DBG("[RMA] factory reset\n");  //Reset不就不能ota了嗎
    //    factory_reset();
    }

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);

    switch(rotate){
    case 1:GPIO_OUTPUT_SET( IDKTgpio_RED , 0 ); break;
    case 2:GPIO_OUTPUT_SET( IDKTgpio_RED , 1 ); break;
    case 3:GPIO_OUTPUT_SET( IDKTgpio_GREEN , 0 ); break;
    case 4:GPIO_OUTPUT_SET( IDKTgpio_GREEN , 1 );break;
    case 5:GPIO_OUTPUT_SET( IDKTgpio_BLUE , 0 );break;
    case 6:GPIO_OUTPUT_SET( IDKTgpio_BLUE , 1 );break;
    case 7:GPIO_OUTPUT_SET( GPIO_ID_PIN(12),1); break;
    case 8:GPIO_OUTPUT_SET( GPIO_ID_PIN(12),0 );break;
    case 9: GPIO_OUTPUT_SET(GPIO_ID_PIN(14),1); break;
    case 10:GPIO_OUTPUT_SET(GPIO_ID_PIN(14),0 );break;
    case 11:GPIO_OUTPUT_SET(GPIO_ID_PIN(15),1); break;
    case 12:GPIO_OUTPUT_SET(GPIO_ID_PIN(15),0 );break;
    case 13:gpio16_output_set(1); break;
    case 14:gpio16_output_set(0);rotate=0 ;break;

    }

    rotate++;
}


LOCAL void ICACHE_FLASH_ATTR
reset_press(void)
{
    ESP_DBG("Press!!\n");

    static SwtHandle handlerma=SWT_TASK_INITIALIZER;
    if ( 0 == GPIO_INPUT_GET(GPIO_ID_PIN(WPS_IO_NUM)) ) {
        rboot_set_rma('Y');
        ESP_DBG("Enter RMA,Do self test\n",RESET_IO_NUM);
        SWT_AddTask(&handlerma,doRma,0,500,500,"rma");
        while(0 == GPIO_INPUT_GET(GPIO_ID_PIN(WPS_IO_NUM)));
    }

    ESP_DBG("Do factory reset\n");
    factory_reset();
}

LOCAL void ICACHE_FLASH_ATTR
reset_short_press(void)
{
    ESP_DBG("Do factory reset\n");
    factory_reset();
}

LOCAL void ICACHE_FLASH_ATTR
reset_long_press(void)
{
    ESP_DBG("reset_long_press\n");
}

#ifdef SWTIMER
LOCAL os_timer_t swt_timer;

LOCAL void ICACHE_FLASH_ATTR
tick_for_swt(void *timer_arg)
{
    ESP_DBG("receive tick\n");
    SWTplatform_AddTick();
    SWT_DispatchTask();
    os_timer_arm(&swt_timer, SWT_TICK_RESOLUTION, 0);
}

LOCAL void ICACHE_FLASH_ATTR
Init_SWT(void)
{
    os_timer_disarm(&swt_timer);
    os_timer_setfn(&swt_timer, (os_timer_func_t *)tick_for_swt, 0);
    os_timer_arm(&swt_timer, SWT_TICK_RESOLUTION, 0);
}
#else

#include "os_type.h"

#define swtTaskPrio    0
#define swtQueueLen    3
os_event_t    swtTaskQueue[swtQueueLen];

LOCAL void ICACHE_FLASH_ATTR ///////
swtTask(os_event_t *events)
{
    SWTplatform_AddTick();
    SWT_DispatchTask();
}

void tick_for_swt(void *timer_arg)
{
//    ESP_DBG("receive tick\n");
    system_os_post(swtTaskPrio, 0, 0);

}

typedef enum {
    FRC1_SOURCE = 0,
    NMI_SOURCE = 1,
} FRC1_TIMER_SOURCE_TYPE;

LOCAL void ICACHE_FLASH_ATTR
Init_SWT(void)
{
#define AUTO_RELOAD 1
    hw_timer_init(FRC1_SOURCE,AUTO_RELOAD);
    hw_timer_set_func(tick_for_swt);
    hw_timer_arm(SWT_TICK_RESOLUTION*1000);
    system_os_task(swtTask, swtTaskPrio, swtTaskQueue, swtQueueLen);

}


#endif
void write_gpio( int desc , char *buf , int buf_size ){
    GPIO_OUTPUT_SET(desc,*buf);
}

void read_gpio( int desc , char *buf , int buf_size ){
    char status=GPIO_INPUT_GET(desc);
    buf[0]=status;
}

void putout( void )
{
    os_printf("putout all led\n");
    GPIO_OUTPUT_SET( IDKTgpio_RED , 1 );
    GPIO_OUTPUT_SET( IDKTgpio_BLUE , 1 );
    GPIO_OUTPUT_SET( IDKTgpio_GREEN , 1 );
}

LOCAL char g_mac_str[16];


void wifiConnected(void){

    os_printf("finish rma_test\n");

static int runonce=0;
    if(runonce==0){
        rboot_set_rma('O');
        char rma;
        int rom;
        rboot_get_rma(&rma,&rom);
        os_printf("rma:%c,rom:%d\n",rma,rom);
        runonce=1;
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, 1);
        os_timer_arm(&client_timer, 100, 0);
    }

}

void ICACHE_FLASH_ATTR
needOTA(void){
    os_printf("need ota!!!!!!!\n");
    rboot_set_rma('O');
    system_restart_delay();
}

#define BTN_NUM            2
LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key[BTN_NUM];
/******************************************************************************
 * FunctionName : user_esp_platform_init
 * Description  : device parame init based on espressif platform
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_init(void)
{    
	os_sprintf(iot_version,"%s%d.%d.%dt%d(%s)",VERSION_TYPE,IOT_VERSION_MAJOR,\
	IOT_VERSION_MINOR,IOT_VERSION_REVISION,device_type,UPGRADE_FALG);
	os_printf("IOT VERSION = %s\n",iot_version);

    struct rst_info *rtc_info = system_get_rst_info();
    ESP_DBG("reset reason: %x\n", rtc_info->reason);

    if (rtc_info->reason == REASON_WDT_RST ||
        rtc_info->reason == REASON_EXCEPTION_RST ||
        rtc_info->reason == REASON_SOFT_WDT_RST) {
        if (rtc_info->reason == REASON_EXCEPTION_RST) {
            ESP_DBG("Fatal exception (%d):\n", rtc_info->exccause);
        }
        ESP_DBG("epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x\n",
                rtc_info->epc1, rtc_info->epc2, rtc_info->epc3, rtc_info->excvaddr, rtc_info->depc);
    }

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
    GPIO_OUTPUT_SET( GPIO_ID_PIN(RESET_IO_NUM) , 1 );


    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);


    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);

    GPIO_OUTPUT_SET( GPIO_ID_PIN(12),0);
    GPIO_OUTPUT_SET( GPIO_ID_PIN(14),0);
    GPIO_OUTPUT_SET( GPIO_ID_PIN(15),0);

    gpio16_output_conf();
    gpio16_output_set(1);

    Init_SWT();
    char rma;
    int rom;
    rboot_get_rma(&rma,&rom);
    if ( rma == 'Y' ){
        rboot_set_rma('N');
        os_printf("do test\n");

#define THIRDPARTY_TEST
#ifdef THIRDPARTY_TEST
        char errmsg[128];
        if (rma_test(errmsg,needOTA)){
            os_printf("errmsg:%s\n",errmsg);
        }

#else
        static SwtHandle handlerma;
        ESP_DBG("Enter RMA,Do self test\n",RESET_IO_NUM);
        SWT_AddTask(&handlerma,doRma,0,500,500,"rma");

        //之後改成外包測試這邊要去掉，由外包來做上網動作
        wifi_set_opmode(STATION_MODE);
        smartconfig_start(smartconfig_done_rma);
#endif
        return;
    }

    IDKT_Init( write_gpio , read_gpio , putout );

    os_timer_disarm(&boot_fail_timer);
    os_timer_setfn(&boot_fail_timer, (os_timer_func_t *)boot_failed_alarm_func, 1);
    os_timer_arm(&boot_fail_timer, 10000, 0);

    wifi_set_opmode(STATION_MODE);

    ESP_DBG("wifi_get_opmode=%d\n", wifi_get_opmode());
    ESP_DBG("version:%s-%s\n",TAG_VERSION ,HASH_VERSION);
    if (wifi_get_opmode() != SOFTAP_MODE) {
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, 1);
        os_timer_arm(&client_timer, 100, 0);
    }


    single_key[0] = key_init_single(WPS_IO_NUM, WPS_IO_MUX, WPS_IO_FUNC,
                                    user_wps_key_long_press, user_wps_key_short_press,0);

    single_key[1] = key_init_single(RESET_IO_NUM, RESET_IO_MUX, RESET_IO_FUNC,
                                    reset_long_press, reset_short_press,reset_press);
    keys.key_num = BTN_NUM;
    keys.single_key = single_key;
    key_init(&keys);



}

#endif
