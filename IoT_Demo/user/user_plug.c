/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_plug.c
 *
 * Description: plug demo's function realization
 *
 * Modification history:
 *     2014/5/1, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "mem.h"
#include "user_interface.h"

#include "smartconfig.h"
#include "user_plug.h"

#if PLUG_DEVICE

LOCAL struct plug_saved_param plug_param;
LOCAL os_timer_t link_led_timer;
LOCAL uint8 link_led_level = 0;

/******************************************************************************
 * FunctionName : user_plug_get_status
 * Description  : get plug's status, 0x00 or 0x01
 * Parameters   : none
 * Returns      : uint8 - plug's status
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
user_plug_get_status(void)
{
    return plug_param.status;
}

/******************************************************************************
 * FunctionName : user_plug_set_status
 * Description  : set plug's status, 0x00 or 0x01
 * Parameters   : uint8 - status
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_plug_set_status(bool status)
{
    if (status != plug_param.status) {
        if (status > 1) {
            os_printf("error status input!\n");
            return;
        }

        plug_param.status = status;
        PLUG_STATUS_OUTPUT(PLUG_RELAY_LED_IO_NUM, status);
    }
}

LOCAL void ICACHE_FLASH_ATTR
user_link_led_init(void)
{
    PIN_FUNC_SELECT(PLUG_LINK_LED_IO_MUX, PLUG_LINK_LED_IO_FUNC);
}

void ICACHE_FLASH_ATTR
user_link_led_output(uint8 level)
{
    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), level);
}

LOCAL void ICACHE_FLASH_ATTR
user_link_led_timer_cb(void)
{
    link_led_level = (~link_led_level) & 0x01;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), link_led_level);
}

void ICACHE_FLASH_ATTR
user_link_led_timer_init(void)
{
    os_timer_disarm(&link_led_timer);
    os_timer_setfn(&link_led_timer, (os_timer_func_t *)user_link_led_timer_cb, NULL);
    os_timer_arm(&link_led_timer, 50, 1);
    link_led_level = 0;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), link_led_level);
}

void ICACHE_FLASH_ATTR
user_link_led_timer_done(void)
{
    os_timer_disarm(&link_led_timer);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(PLUG_LINK_LED_IO_NUM), 1);
}

/******************************************************************************
 * FunctionName : user_plug_init
 * Description  : init plug's key function and relay output
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_plug_init(void)
{
    user_link_led_init();

    wifi_status_led_install(PLUG_WIFI_LED_IO_NUM, PLUG_WIFI_LED_IO_MUX, PLUG_WIFI_LED_IO_FUNC);


    spi_flash_read((PRIV_PARAM_START_SEC + PRIV_PARAM_SAVE) * SPI_FLASH_SEC_SIZE,
        		(uint32 *)&plug_param, sizeof(struct plug_saved_param));

    PIN_FUNC_SELECT(PLUG_RELAY_LED_IO_MUX, PLUG_RELAY_LED_IO_FUNC);

    // no used SPI Flash
    if (plug_param.status == 0xff) {
        plug_param.status = 1;
    }

//    PLUG_STATUS_OUTPUT(PLUG_RELAY_LED_IO_NUM, plug_param.status);

    PLUG_STATUS_OUTPUT(PLUG_WIFI_LED_IO_NUM ,0);
    PLUG_STATUS_OUTPUT(PLUG_RELAY_LED_IO_NUM ,0);

}
#endif

