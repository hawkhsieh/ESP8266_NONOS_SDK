
/***************************************************************************\
Copyright: 		2015-2017, LinGan Tech. Co., Ltd.
File name: 		rma_test.c
Description: 	排插测试源码

Author: 			刘辉
Version: 		ver0.0.1
Date: 			2017-04-22
History: 		none
\****************************************************************************/
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "mem.h"

#include "gpio.h"
#include "gpio16.h"
#include "rma_test.h"

static struct user_key_param mUsrKeysInfo;
static uint8 mRelayState;
static uint8 mLedMode;
static uint8 mLedSubMode;
static uint8 mLedSubModeCnt;
static uint8 mWifiState;

static uint8 mRelayMode;
static uint8 mRelayModeCnt;

static os_timer_t led_timer;
static os_timer_t relay_timer;
static os_timer_t check_timer;
static os_timer_t key_check_timer;

static uint8 mTestMode;
struct espconn	mUserUdp;
static char mDevInfo[40];

void ICACHE_FLASH_ATTR SetIndicatorLed(void);
void ICACHE_FLASH_ATTR VaryRelay( void );
int ICACHE_FLASH_ATTR rma_init( void );

/*==========================================================*
* name		:  InitUserLeds
* param		:  none
* return		:  none
* fuction	:  
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR SetWifiLed( void )
{
	if( mLedMode == MODE_CYCLE_MOVE) return;

	if( mWifiState == WIFI_STATE_CONNECT)
	{
		mLedMode = MODE_GREEN_SHAKE;
	}else{
		mLedMode = MODE_RED_SHAKE;
	}
	SetIndicatorLed();
}
/*==========================================================*
* name		:  key_long_cb
* param		:  struct single_key_param *
* return		:  none
* fuction	:  按键长按回调
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
LOCAL void key_long_cb(struct single_key_param *single_key)
{
    os_timer_disarm(&single_key->key_timer);
	
    if ( LOW_KEY_LEVEL == single_key->key_level ) 
    {
    	single_key->key_flg |= LONG_SHORT_MUTEX;
		if( single_key->long_fun)
			single_key->long_fun();
    }
}

/*==========================================================*
* name		:  key_short_cb
* param		:  struct single_key_param *
* return		:  none
* fuction	:  按键弹起(短按)回调函数
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
LOCAL void key_short_cb(struct single_key_param *single_key)
{
    os_timer_disarm(&single_key->key_timer);

    // high, then key is up
    if (HIGH_KEY_LEVEL == single_key->key_level) {
		single_key->key_flg &=~KEY_ANALOG_UP_INT;
		if( ( single_key->short_fun) && ( LONG_SHORT_MUTEX !=(LONG_SHORT_MUTEX &single_key->key_flg)))
		{
			single_key->short_fun();
		}
		single_key->key_flg &=(~LONG_SHORT_MUTEX);
	} else {
		single_key->key_flg |= KEY_ANALOG_UP_INT;
    }
}

#if 0
/*==========================================================*
* name		:  key_intr_handler
* param		:  sCustomKeyParam
* return		:  NONE
* fuction	:  按键中断服务程序
* author		:  刘辉
* date		:  2017-04-05
*===========================================================*/
LOCAL void key_intr_handler(void)
{
    uint8 i;
    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	struct single_key_param * key_param;

    for (i = 0; i < mUsrKeysInfo.key_num; i++) {
		key_param = (struct single_key_param *)&mUsrKeysInfo.user_keys[i];
        if (gpio_status & BIT(key_param->gpio_id)) {
            gpio_pin_intr_state_set(GPIO_ID_PIN(key_param->gpio_id), GPIO_PIN_INTR_DISABLE);
            GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(key_param->gpio_id));
			os_timer_disarm(&key_param->key_timer);
            if (key_param->key_level == HIGH_KEY_LEVEL) {
                os_timer_setfn(&key_param->key_timer, (os_timer_func_t *)key_long_cb, key_param);
                os_timer_arm(&key_param->key_timer, key_param->long_time, 0);
                key_param->key_level = LOW_KEY_LEVEL;
                gpio_pin_intr_state_set(GPIO_ID_PIN(key_param->gpio_id), GPIO_PIN_INTR_POSEDGE);
				if( key_param->start_fun){
					key_param->start_fun();
				}
            } else {
                os_timer_setfn(&key_param->key_timer, (os_timer_func_t *)key_short_cb, key_param);
                os_timer_arm(&key_param->key_timer, SHORT_KEY_TIMER_VALUE, 0);
            }
        }
    }
}
#endif

/*==========================================================*
* name		:  CheckBothKeys
* param		:  none
* return		:  none
* fuction	:  init user keys
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR CheckBothKeys(void)
{
	uint8 key_value1 = GPIO_INPUT_GET(GPIO_ID_PIN( KEY1_IO_NUM ));
	uint8 key_value2 = GPIO_INPUT_GET(GPIO_ID_PIN( KEY2_IO_NUM ));

	if( (LOW_KEY_LEVEL != key_value1) || (LOW_KEY_LEVEL != key_value2))
		return;
	
	os_printf("\n===> enter rma test mode!");

	rma_init( );
	mLedMode = MODE_CYCLE_MOVE;
	mLedSubMode = SUB_MODE_R;
	SetIndicatorLed();
}

/*==========================================================*
* name		:  TestSystemMode1
* param		:  none
* return		:  none
* fuction	:  
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR TestSystemMode1(void)
{
	if( mTestMode != IDLE_TEST_MODE) return;
	mTestMode =	TEST_1_MODE;
	os_printf("\n===>start test 1!");

	/* -绿灯常亮- */
	mLedMode =	MODE_GREEN_SOILDON;
	SetIndicatorLed();

	/* -继电器逐个变化- */
	mRelayMode = KK1_ON_MODE;
	mRelayModeCnt = 0;
	VaryRelay();
}

/*==========================================================*
* name		:  SystemMode1Fail
* param		:  none
* return		:  none
* fuction	:  
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR SystemMode1Fail(void)
{
	if( mTestMode ==  TEST_1_MODE) return;
	mTestMode =	TEST_1_MODE;
	os_printf("\n===>test 1 Fail!");
	
	/* -橙灯闪烁- */
	mLedMode =	MODE_ORANGE_SHAKE;
	SetIndicatorLed();

	/* -继电器逐个变化- */
	mRelayMode = KK1_ON_MODE;
	mRelayModeCnt = 0;
	VaryRelay();
}

/*==========================================================*
* name		:  TestSystemMode2
* param		:  none
* return		:  none
* fuction	:  
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR TestSystemMode2(void)
{
	if( mTestMode !=  IDLE_TEST_MODE) return;
	mTestMode =	TEST_2_MODE;
	os_printf("\n===>start test 2!");

	/* -绿灯常亮- */
	//mLedMode =	MODE_GREEN_SOILDON;
	//SetIndicatorLed();

	/* -继电器逐个变化- */
	mRelayMode = ALL_KKSHAKE_MODE;
	mRelayModeCnt = 0;
	VaryRelay();
}

/*==========================================================*
* name		:  SystemMode2Fail
* param		:  none
* return		:  none
* fuction	:  
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR SystemMode2Fail(void)
{
	if( mTestMode ==  TEST_2_MODE) return;
	mTestMode =	TEST_2_MODE;
	os_printf("\n===>test 2 fail!");
	
	/* -继电器逐个变化- */
	mRelayMode = ALL_KKOFF_MODE;
	VaryRelay();
}

/*==========================================================*
* name		:  key_analog_intr_handler
* param		:  
* return		:  NONE
* fuction	:  按键中断服务程序
* author		:  刘辉
* date		:  2017-04-05
*===========================================================*/
LOCAL void ICACHE_FLASH_ATTR key_analog_intr_handler( struct single_key_param * key_param )
{
	os_timer_disarm(&key_param->key_timer);
    if (key_param->key_level == LOW_KEY_LEVEL) {
		os_timer_setfn(&key_param->key_timer, (os_timer_func_t *)key_long_cb, key_param);
        os_timer_arm(&key_param->key_timer, key_param->long_time, 0);
		key_param->key_flg |= KEY_ANALOG_UP_INT;
		if( key_param->start_fun){
			key_param->start_fun();
		}
    }else {
	    os_timer_setfn(&key_param->key_timer, (os_timer_func_t *)key_short_cb, key_param);
	    os_timer_arm(&key_param->key_timer, SHORT_KEY_TIMER_VALUE, 0);
    }
}

/*==========================================================*
* name		:  key_press_check
* param		:  none
* return		:  none
* fuction	:  key_press_check
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR key_press_check(void)
{
    uint8 i;
	struct single_key_param * key_param;
	for( i =0; i < mUsrKeysInfo.key_num; i++ )
	{
		key_param = (struct single_key_param *)&mUsrKeysInfo.user_keys[i];

		uint8 level = GPIO_INPUT_GET(GPIO_ID_PIN( key_param->gpio_id ));

		if( KEY_LEVEL_NOT_SURE ==(KEY_LEVEL_NOT_SURE &key_param->key_flg))
		{
			key_param->key_flg &= ~KEY_LEVEL_NOT_SURE;
			if( key_param->key_level == level )
			{
				if( (KEY_ANALOG_UP_INT ==(KEY_ANALOG_UP_INT &key_param->key_flg)) && \
					( HIGH_KEY_LEVEL == level))
				{/* -上升沿- */
					key_analog_intr_handler(key_param);
					os_printf("\n===> KEY (%d) Up!", key_param->gpio_id);
				}else if( (KEY_ANALOG_UP_INT !=(KEY_ANALOG_UP_INT &key_param->key_flg)) && \
					( LOW_KEY_LEVEL == level))
				{/* -下降沿- */
					key_analog_intr_handler(key_param);
					os_printf("\n===> KEY (%d) Press!", key_param->gpio_id);
				}
			}else{
				key_param->key_level = level;
			}
		}else{
			if( key_param->key_level != level ){
				key_param->key_flg |= KEY_LEVEL_NOT_SURE;
				key_param->key_level = level;
			}
		}
    }
}

/*==========================================================*
* name		:  InitUserKeys
* param		:  none
* return		:  none
* fuction	:  init user keys
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR InitUserKeys(void)
{
	mUsrKeysInfo.key_num = USER_KEYS_NUM;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].key_flg		= 0;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].gpio_id 		= KEY1_IO_NUM;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].gpio_name = KEY1_IO_MUX;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].gpio_func	= KEY1_IO_FUNC;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].start_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].short_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].long_fun	= CheckBothKeys;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].long_time	= LONG_KEY_TIMER_VALUE1;

	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].key_flg 		= 0;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].gpio_id 		= KEY2_IO_NUM;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].gpio_name = KEY2_IO_MUX;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].gpio_func	= KEY2_IO_FUNC;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].start_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].short_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].long_fun	= CheckBothKeys;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].long_time	= LONG_KEY_TIMER_VALUE1;

    uint8 i;
	struct single_key_param * key_param;
	for( i =0; i < mUsrKeysInfo.key_num; i++ )
	{
		key_param = (struct single_key_param *)&mUsrKeysInfo.user_keys[i];
        PIN_FUNC_SELECT(key_param->gpio_name, key_param->gpio_func);
        gpio_output_set(0, 0, 0, GPIO_ID_PIN(key_param->gpio_id));
		key_param->key_level = GPIO_INPUT_GET(GPIO_ID_PIN( key_param->gpio_id ));
    }

	os_timer_disarm(&key_check_timer);
	os_timer_setfn(&key_check_timer, (os_timer_func_t *)key_press_check, NULL);
	os_timer_arm(&key_check_timer, 80, 1);
}

#if 0
/*==========================================================*
* name		:  InitUserKeys
* param		:  none
* return		:  none
* fuction	:  init user keys
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR InitUserKeys(void)
{
    ETS_GPIO_INTR_ATTACH(key_intr_handler, NULL);
    ETS_GPIO_INTR_DISABLE();

	mUsrKeysInfo.key_num = USER_KEYS_NUM;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].key_flg		= 0;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].gpio_id 		= KEY1_IO_NUM;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].gpio_name = KEY1_IO_MUX;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].gpio_func	= KEY1_IO_FUNC;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].start_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].short_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].long_fun	= CheckBothKeys;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].long_time	= LONG_KEY_TIMER_VALUE1;

	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].key_flg 		= 0;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].gpio_id 		= KEY2_IO_NUM;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].gpio_name = KEY2_IO_MUX;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].gpio_func	= KEY2_IO_FUNC;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].start_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].short_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].long_fun	= CheckBothKeys;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].long_time	= LONG_KEY_TIMER_VALUE1;

    uint8 i;
	struct single_key_param * key_param;
	for( i =0; i < mUsrKeysInfo.key_num; i++ )
	{
		key_param = (struct single_key_param *)&mUsrKeysInfo.user_keys[i];
		key_param->key_level = HIGH_KEY_LEVEL;
        PIN_FUNC_SELECT(key_param->gpio_name, key_param->gpio_func);
        gpio_output_set(0, 0, 0, GPIO_ID_PIN(key_param->gpio_id));
        gpio_register_set(GPIO_PIN_ADDR(key_param->gpio_id), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
                          | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
                          | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
        //clear gpio14 status
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(key_param->gpio_id));

        //enable interrupt
        gpio_pin_intr_state_set(GPIO_ID_PIN(key_param->gpio_id), GPIO_PIN_INTR_NEGEDGE);
    }
    ETS_GPIO_INTR_ENABLE();
}
#endif

/*==========================================================*
* name		:  InitRmaTestKeys
* param		:  none
* return		:  none
* fuction	:  
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR InitRmaTestKeys(void)
{
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].start_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].short_fun	= TestSystemMode1;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].long_fun	= SystemMode1Fail;
	mUsrKeysInfo.user_keys[USER_KEY1_INDEX].long_time	= LONG_KEY_TIMER_VALUE2;

	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].start_fun	= NULL;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].short_fun	= TestSystemMode2;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].long_fun	= SystemMode2Fail;
	mUsrKeysInfo.user_keys[USER_KEY2_INDEX].long_time	= LONG_KEY_TIMER_VALUE2;

}

/*==========================================================*
* name		:  InitUserLeds
* param		:  none
* return		:  none
* fuction	:  
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR ShowLedSubMode(void)
{
	switch( mLedSubMode )
	{
		case SUB_MODE_R:
			mLedSubMode++;
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), INDICATOR_LED_ON);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), INDICATOR_LED_OFF);
			os_timer_setfn(&led_timer, (os_timer_func_t *)SetIndicatorLed, NULL);
			os_timer_arm(&led_timer, SHAKE_SLOW_500MS, 0);
			os_printf("\n===> SUB_MODE_R");
			break;

		case SUB_MODE_G:
			mLedSubMode++;
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), INDICATOR_LED_OFF);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), INDICATOR_LED_ON);
			os_timer_setfn(&led_timer, (os_timer_func_t *)SetIndicatorLed, NULL);
			os_timer_arm(&led_timer, SHAKE_SLOW_500MS, 0);
			os_printf("\n===> SUB_MODE_G");
			break;

		case SUB_MODE_O:
			//if( mLedSubModeCnt < 2){
				mLedSubMode= SUB_MODE_R;
			//	mLedSubModeCnt++;
			//}else{
			//	mLedSubMode++;
			//	mLedSubModeCnt = 0;
			//}
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), INDICATOR_LED_ON);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), INDICATOR_LED_ON);
			os_timer_setfn(&led_timer, (os_timer_func_t *)SetIndicatorLed, NULL);
			os_timer_arm(&led_timer, SHAKE_SLOW_500MS, 0);
			os_printf("\n===> SUB_MODE_O");
			break;

		case SUB_MODE_O_KEEP:
			mLedMode = MODE_IDLE_KEEP;
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), INDICATOR_LED_ON);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), INDICATOR_LED_ON);
			//SetWifiLed();
			os_printf("\n===> SUB_MODE_O_KEEP");
			break;

	}
}
/*==========================================================*
* name		:  InitUserLeds
* param		:  none
* return		:  none
* fuction	:  init user led gpio
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
void ICACHE_FLASH_ATTR SetIndicatorLed(void)
{
	uint8 temp;
	os_timer_disarm(&led_timer);
	switch( mLedMode )
	{
		/* -绿灯常亮- */
		case MODE_GREEN_SOILDON:
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), INDICATOR_LED_OFF);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), INDICATOR_LED_ON);
			break;

		/* -橙灯常亮- */
		case MODE_ORANGE_SOILDON:
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), INDICATOR_LED_ON);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), INDICATOR_LED_ON);
			break;
	
		/* -红灯闪烁- */
		case MODE_RED_SHAKE:
			temp = GPIO_INPUT_GET( GPIO_ID_PIN( RED_LED1_IO_NUM ));
			temp = ( temp == INDICATOR_LED_OFF) ? INDICATOR_LED_ON: INDICATOR_LED_OFF;
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), temp);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), INDICATOR_LED_OFF);
			os_timer_setfn(&led_timer, (os_timer_func_t *)SetIndicatorLed, NULL);
			os_timer_arm(&led_timer, SHAKE_SLOW_1S, 0);
			break;

		/* -绿灯闪烁- */
		case MODE_GREEN_SHAKE:
			temp = GPIO_INPUT_GET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ));
			temp = ( temp == INDICATOR_LED_OFF) ? INDICATOR_LED_ON: INDICATOR_LED_OFF;
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), INDICATOR_LED_OFF);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), temp);
			os_timer_setfn(&led_timer, (os_timer_func_t *)SetIndicatorLed, NULL);
			os_timer_arm(&led_timer, SHAKE_SLOW_1S, 0);
			break;

		/* -橙灯闪烁- */
		case MODE_ORANGE_SHAKE:
			temp = GPIO_INPUT_GET( GPIO_ID_PIN( RED_LED1_IO_NUM ));
			temp = ( temp == INDICATOR_LED_OFF) ? INDICATOR_LED_ON: INDICATOR_LED_OFF;
			GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), temp);
			GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), temp);
			os_timer_setfn(&led_timer, (os_timer_func_t *)SetIndicatorLed, NULL);
			os_timer_arm(&led_timer, SHAKE_SLOW_1S, 0);
			break;

		/* -R->G->O --3-->O- */
		case MODE_CYCLE_MOVE:
			os_printf("\n===> MODE_CYCLE_MOVE");
			ShowLedSubMode();
			break;
	}
}
/*==========================================================*
* name		:  InitUserLeds
* param		:  none
* return		:  none
* fuction	:  init user led gpio
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR InitUserLeds(void)
{
	mLedSubMode = SUB_MODE_R;
	mLedSubModeCnt = 0;
	
	PIN_FUNC_SELECT( RED_LED1_IO_MUX, RED_LED1_IO_FUNC); 
	GPIO_OUTPUT_SET( GPIO_ID_PIN( RED_LED1_IO_NUM ), INDICATOR_LED_OFF);

	PIN_FUNC_SELECT( GREEN_LED2_IO_MUX, GREEN_LED2_IO_FUNC); 
	GPIO_OUTPUT_SET( GPIO_ID_PIN( GREEN_LED2_IO_NUM ), INDICATOR_LED_OFF);
}

/*==========================================================*
* name		:	CtrlRelayIO
* param 		:	ioNum - IO口，state - 开关状态
* return		:	NONE
* fuction	:	继电器IO口控制
* author		:	刘辉
* date		:	2017-03-07
*===========================================================*/
static void ICACHE_FLASH_ATTR CtrlRelayIO(uint8 ioNum,uint8 state)
{
	state = ( state != POWER_STRIP_ONE_OFF) ? RELAY_LEVEL_ON : RELAY_LEVEL_OFF;
	GPIO_OUTPUT_SET( GPIO_ID_PIN(ioNum), state);
}

static void ICACHE_FLASH_ATTR CtrlGpio16(uint8 state)
{
	state = ( state != POWER_STRIP_ONE_OFF) ? RELAY_LEVEL_ON : RELAY_LEVEL_OFF;
	gpio16_output_set(state);
}

/*==========================================================*
* name		:	CtrlRelayDriver
* param 		:	index - 索引号，state - 开关状态
* return		: 	NONE
* fuction	: 	驱动指定继电器开关动作(索引号从1开始)
		  		  	特殊值: 0 - 全关， 0xFF - 全关
		  		  	state: bit 0 - 7, 每一孔对一位
* author		:	刘辉
* date		:	2017-03-07
*===========================================================*/
static void ICACHE_FLASH_ATTR CtrlRelayDriver(uint8 index,uint8 state)
{
	os_printf("\n Relay index %d,Sate:%d",index,state);
	switch(index)
	{
		case KK1_INDEX:
			state = (state>>(KK1_INDEX -1))&0x01;
			CtrlRelayIO(KK1_RELAY_IO_NUM,state);
			break;

		case KK2_INDEX:
			state = (state>>(KK2_INDEX -1))&0x01;
			CtrlRelayIO(KK2_RELAY_IO_NUM,state);
			break;
			
		case KK3_INDEX:
			state = (state>>(KK3_INDEX -1))&0x01;
			CtrlRelayIO(KK3_RELAY_IO_NUM,state);
			break;
			
		case KK4_INDEX:// 4 --- > Gpio16
			state = (state>>(KK4_INDEX -1))&0x01;
			CtrlGpio16(state);
			break;

		//case ALL_KK_ON_INDEX:
		//	break;

		//case ALL_KK_OFF_INDEX:
		//	break;	

		default:
			os_printf("\n Relay index is illegal(%d,max:%d)!",index,RELAY_TOTAL_NUM);
			break;
	}
}

static void ICACHE_FLASH_ATTR 
BrocastRecvCallBack(struct espconn *pConn, char *pdata, unsigned short len) 
{


}

void ICACHE_FLASH_ATTR CheckWiFiStatus(void)
{
	uint8 mac[6];
	os_timer_disarm(&check_timer);
	if( STATION_GOT_IP == wifi_station_get_connect_status())
	{
		if( mWifiState != WIFI_STATE_CONNECT)
		{
			mWifiState = WIFI_STATE_CONNECT;
			mLedMode =	MODE_GREEN_SHAKE;
			SetIndicatorLed();
			wifi_get_macaddr( STATION_IF, mac);
			os_sprintf( mDevInfo,"\r\nAQM100-PS,A1,%02X:%02X:%02X:%02X:%02X:%02X",\
				mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]  );
		}
		espconn_send(&mUserUdp,(uint8 *)mDevInfo, 32);
		
		os_timer_setfn(&check_timer, (os_timer_func_t *)CheckWiFiStatus, NULL);
		os_timer_arm(&check_timer, SHAKE_SLOW_1S, 0);	
	}else{

		if( mWifiState != WIFI_STATE_DISCONNECT)
		{
			mWifiState = WIFI_STATE_DISCONNECT;
			mLedMode =	MODE_RED_SHAKE;
			SetIndicatorLed();
		}
		os_timer_setfn(&check_timer, (os_timer_func_t *)CheckWiFiStatus, NULL);
		os_timer_arm(&check_timer, SHAKE_SLOW_100MS, 0);	
	}
}

static void ICACHE_FLASH_ATTR InitUserUdp(void)
{
	esp_udp* udp_info = NULL;
	wifi_set_broadcast_if(STATION_MODE);
	
	mUserUdp.type = ESPCONN_UDP;
	mUserUdp.state = ESPCONN_NONE;
	mUserUdp.proto.udp = (esp_udp*) os_zalloc(sizeof(esp_udp));
	mUserUdp.reverse = NULL;

	udp_info = mUserUdp.proto.udp;
	udp_info->local_port = espconn_port();
	udp_info->remote_port = 8080;
	udp_info->remote_ip[0] = 255;
	udp_info->remote_ip[1] = 255;
	udp_info->remote_ip[2] = 255;
	udp_info->remote_ip[3] = 255;
	espconn_regist_recvcb(&mUserUdp,(espconn_recv_callback)BrocastRecvCallBack);
	espconn_create(&mUserUdp);
	os_printf("\n Init User Udp Path!");
}

void ICACHE_FLASH_ATTR InitCheckWiFi(void)
{
	os_timer_disarm(&check_timer);
	wifi_station_disconnect();
	os_printf("\n===> Start Check Wifi \n");
	
	wifi_set_opmode(STATION_MODE);
	struct station_config config;
	os_memcpy((uint8 *)config.ssid,(uint8 *)ROUTE_AP_SSID,os_strlen(ROUTE_AP_SSID));
	config.ssid[os_strlen(ROUTE_AP_SSID)] = '\0';
	os_memcpy((uint8 *)config.password,(uint8 *)ROUTE_AP_PWD,os_strlen(ROUTE_AP_PWD));
	config.password[os_strlen(ROUTE_AP_PWD)] = '\0';
	config.bssid_set = 0;
	wifi_station_set_config(&config);
	mWifiState = WIFI_STATE_DISCONNECT;
	mLedMode =	MODE_RED_SHAKE;
	SetIndicatorLed();
	wifi_station_connect();
	InitUserUdp();
	os_timer_setfn(&check_timer, (os_timer_func_t *)CheckWiFiStatus, NULL);
	os_timer_arm(&check_timer, SHAKE_SLOW_100MS, 0);	
}

/*==========================================================*
* name		:	VaryRelay
* param 		:	
* return		:	none
* fuction	:  
* author		:	刘辉
* date		:	2017-04-22
*===========================================================*/
void ICACHE_FLASH_ATTR VaryRelay( void )
{
	uint16 temp;
	os_timer_disarm(&relay_timer);
	switch( mRelayMode )
	{
		case KK1_ON_MODE:
			mRelayMode++;
			mRelayState = 0x02;
			CtrlRelayDriver( KK1_INDEX, mRelayState);
			CtrlRelayDriver( KK2_INDEX, mRelayState);
			CtrlRelayDriver( KK3_INDEX, mRelayState);
			CtrlRelayDriver( KK4_INDEX, mRelayState);

			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);
			break;
		
		case KK2_ON_MODE:
			mRelayMode++;
			mRelayState = 0x06;
			CtrlRelayDriver( KK2_INDEX, mRelayState);
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);
			break;

		case KK3_ON_MODE:
			mRelayMode++;
			mRelayState = 0x0E;
			CtrlRelayDriver( KK3_INDEX, mRelayState);
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);
			break;
			
		case KK4_ON_MODE:
			mRelayMode++;
			mRelayState = 0x1E;
			CtrlRelayDriver( KK4_INDEX, mRelayState);
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);
			break;
		
		case KK1_OFF_MODE:
			mRelayMode++;
			mRelayState = 0x1C;
			CtrlRelayDriver( KK1_INDEX, mRelayState);
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);
			break;
				
		case KK2_OFF_MODE:
			mRelayMode++;
			mRelayState = 0x18;
			CtrlRelayDriver( KK2_INDEX, mRelayState);
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);		
			break;
		
		case KK3_OFF_MODE:
			mRelayMode++;
			mRelayState = 0x10;
			CtrlRelayDriver( KK3_INDEX, mRelayState);
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);		
			break;
					
		case KK4_OFF_MODE:
			mRelayState = 0x00;
			CtrlRelayDriver( KK4_INDEX, mRelayState);
			
			if( mRelayModeCnt< 2){
				mRelayModeCnt++;
				mRelayMode = KK1_ON_MODE;
			}else{
				mRelayModeCnt = 0;
				mRelayMode = ALL_KKON_MODE;
			}
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);	
			break;
			
		case ALL_KKON_MODE:
			mRelayMode = NEXT_TEST1_MODE;
			mRelayState = 0x1E;
			CtrlRelayDriver( KK1_INDEX, mRelayState);
			CtrlRelayDriver( KK2_INDEX, mRelayState);
			CtrlRelayDriver( KK3_INDEX, mRelayState);
			CtrlRelayDriver( KK4_INDEX, mRelayState);
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_200MS, 0);	
			break;

		case ALL_KKOFF_MODE:
			mRelayState = 0x0;
			CtrlRelayDriver( KK1_INDEX, mRelayState);
			CtrlRelayDriver( KK2_INDEX, mRelayState);
			CtrlRelayDriver( KK3_INDEX, mRelayState);
			CtrlRelayDriver( KK4_INDEX, mRelayState);
			break;

		case ALL_KKSHAKE_MODE:
			if( mRelayModeCnt< 5){
				mRelayModeCnt++;
			}else{
				mRelayModeCnt = 0;
				mRelayMode = NEXT_TEST2_MODE;
			}
			if( (mRelayState != 0) && (mRelayState != 0x1E))
			{
				mRelayState = 0;
			}
			mRelayState = (mRelayState != 0) ?  0:0x1E;
			CtrlRelayDriver( KK1_INDEX, mRelayState);
			CtrlRelayDriver( KK2_INDEX, mRelayState);
			CtrlRelayDriver( KK3_INDEX, mRelayState);
			CtrlRelayDriver( KK4_INDEX, mRelayState);
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_500MS, 0);	
			break;

		case NEXT_TEST1_MODE:
			
			/* -check ADC- */
			temp = system_adc_read();
			os_printf("\n system_adc_read- %d!",temp);

#define   MIN_WARN_LEVEL	461	//0.45
#define   MAX_WARN_LEVEL	550	//0.55

			if ( ( temp < MIN_WARN_LEVEL ) || (temp > MAX_WARN_LEVEL) )
			{
				mRelayState = 0x0;
			}else{
				/* -在报警范围- */
				mRelayState = 0x1E;
			}	
			CtrlRelayDriver( KK1_INDEX, mRelayState);
			CtrlRelayDriver( KK2_INDEX, mRelayState);
			CtrlRelayDriver( KK3_INDEX, mRelayState);
			CtrlRelayDriver( KK4_INDEX, mRelayState);

			mTestMode = IDLE_TEST_MODE;
			os_timer_setfn(&relay_timer, (os_timer_func_t *)VaryRelay, NULL);
			os_timer_arm(&relay_timer, SHAKE_SLOW_500MS, 0);	
			break;

		case NEXT_TEST2_MODE:
			
			/* -check WIFI- */
			mRelayState = 0x1E;
			CtrlRelayDriver( KK1_INDEX, mRelayState);
			CtrlRelayDriver( KK2_INDEX, mRelayState);
			CtrlRelayDriver( KK3_INDEX, mRelayState);
			CtrlRelayDriver( KK4_INDEX, mRelayState);
			InitCheckWiFi();
			mTestMode = IDLE_TEST_MODE;
			break;
	}
}

/*==========================================================*
* name		:	InitRelayIO
* param 		:	state - 开关状态
* return		:	none
* fuction	:  初始化继电器的电平
* author		:	刘辉
* date		:	2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR InitRelayIO(uint8 state)
{
	PIN_FUNC_SELECT(KK1_RELAY_IO_MUX,KK1_RELAY_IO_FUNC);
	CtrlRelayDriver(KK1_INDEX,state);//KK1
	PIN_FUNC_SELECT(KK2_RELAY_IO_MUX,KK2_RELAY_IO_FUNC);
	CtrlRelayDriver(KK2_INDEX,state);//KK2
	PIN_FUNC_SELECT(KK3_RELAY_IO_MUX,KK3_RELAY_IO_FUNC);
	CtrlRelayDriver(KK3_INDEX,state);//KK3
	gpio16_output_conf();
	CtrlRelayDriver(KK4_INDEX,state);//KK4
}

/*==========================================================*
* name		:  InitRmaParam
* param		:  
* return		:  
* fuction	:  
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
static void ICACHE_FLASH_ATTR InitRmaParam( void )
{
	mWifiState = WIFI_STATE_DISCONNECT;
	mRelayMode = NO_KK_MODE;
	mTestMode = IDLE_TEST_MODE;
	mRelayState = 0;
}

/*==========================================================*
* name		:  rma_test
* param		:  errmsg
* return		:  0 -success. -1 - fail (see errmsg)
* fuction	:  智能排插测试计划
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
int ICACHE_FLASH_ATTR rma_init( void )
{
	InitRmaParam( );
	InitRmaTestKeys( );
	InitUserLeds( );
	InitRelayIO(mRelayState);
    return 0; //success
}

/*==========================================================*
* name		:  rma_test
* param		:  errmsg
* return		:  0 -success. -1 - fail (see errmsg)
* fuction	:  智能排插测试计划
* author		:  刘辉
* date		:  2017-04-22
*===========================================================*/
int ICACHE_FLASH_ATTR rma_test( char *errmsg )
{
	InitUserKeys( );
	return 0;
}
