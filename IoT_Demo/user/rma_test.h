
#define  ROUTE_AP_SSID		"Aquamutant"
#define  ROUTE_AP_PWD		"12345678"

//#define  ROUTE_AP_SSID		"Extern_HiWiFi"
//#define  ROUTE_AP_PWD		"520520520"


#define  LONG_KEY_TIMER_VALUE1	 100
#define  LONG_KEY_TIMER_VALUE2	 500
#define  SHORT_KEY_TIMER_VALUE	 50

#define  LOW_KEY_LEVEL		0
#define  HIGH_KEY_LEVEL	1

#define  USER_KEY1_INDEX	0
#define  USER_KEY2_INDEX	1
#define  USER_KEYS_NUM	2

/* -reset key1 - */
#define KEY1_IO_MUX 	PERIPHS_IO_MUX_MTCK_U
#define KEY1_IO_NUM 	13
#define KEY1_IO_FUNC 	FUNC_GPIO13

/* -power key2 - */
#define KEY2_IO_MUX 	PERIPHS_IO_MUX_GPIO5_U
#define KEY2_IO_NUM 	5
#define KEY2_IO_FUNC 	FUNC_GPIO5

#define INDICATOR_LED_ON   0
#define INDICATOR_LED_OFF  1

/* -shake led mode - */
#define 	MODE_IDLE_KEEP			0
#define 	MODE_GREEN_SOILDON	1  //ÂÌµÆ³£ÁÁ
#define 	MODE_ORANGE_SOILDON	2  //³ÈÉ«µÆ³£ÁÁ
#define 	MODE_RED_SHAKE			3	//ºìµÆÉÁË¸
#define 	MODE_GREEN_SHAKE		4  //ÂÌµÆÉÁË¸
#define 	MODE_ORANGE_SHAKE	5  //³ÈÉ«µÆÉÁË¸

#define 	MODE_CYCLE_MOVE		6  //R->G->O(Loop 3 times) 
	#define 	SUB_MODE_R			0  //R->G->O(Loop 3 times) 
	#define 	SUB_MODE_G			1  //R->G->O(Loop 3 times) 
	#define 	SUB_MODE_O			2  //R->G->O(Loop 3 times) 
	#define 	SUB_MODE_O_KEEP	3  //R->G->O(Loop 3 times) 

/* -Wifi State- */
#define	WIFI_STATE_DISCONNECT	0
#define	WIFI_STATE_CONNECT		1

/* -Test Mode- */
#define	NORMAL_MODE		0
#define	IDLE_TEST_MODE		1
#define	TEST_1_MODE			2
#define	TEST_2_MODE			3

/* -Relay vary mode- */
#define	NO_KK_MODE		0
#define	KK1_ON_MODE		1
#define	KK2_ON_MODE		2
#define	KK3_ON_MODE		3
#define	KK4_ON_MODE		4
#define	KK1_OFF_MODE	5
#define	KK2_OFF_MODE	6
#define	KK3_OFF_MODE	7
#define	KK4_OFF_MODE	8
#define	ALL_KKON_MODE	9
#define	ALL_KKOFF_MODE	10

#define	ALL_KKSHAKE_MODE	11
#define	NEXT_TEST1_MODE	12
#define	NEXT_TEST2_MODE	13


/* -LED1- */
#define RED_LED1_IO_MUX 	PERIPHS_IO_MUX_GPIO4_U
#define RED_LED1_IO_NUM 	4
#define RED_LED1_IO_FUNC 	FUNC_GPIO4

/* -LED2- */
#define GREEN_LED2_IO_MUX 	PERIPHS_IO_MUX_GPIO0_U
#define GREEN_LED2_IO_NUM 	0
#define GREEN_LED2_IO_FUNC 	FUNC_GPIO0

#define RELAY_TOTAL_NUM		4
#define ALL_KK_OFF_INDEX		0
#define USB_INDEX				1
#define KK1_INDEX				2
#define KK2_INDEX				3
#define KK3_INDEX				4
#define KK4_INDEX				5
#define ALL_KK_ON_INDEX		0xFF

#define POWER_STRIP_ONE_OFF	0x00
#define POWER_STRIP_ONE_ON	0x01
#define SHAKE_SLOW_100MS		100
#define SHAKE_SLOW_200MS		200
#define SHAKE_SLOW_500MS		500
#define SHAKE_SLOW_1S			1000
#define  RELAY_LEVEL_ON 		1
#define  RELAY_LEVEL_OFF		0

/* -KK1 USE GPIO12- */
#define KK1_RELAY_IO_MUX 	PERIPHS_IO_MUX_MTDI_U
#define KK1_RELAY_IO_NUM 	12
#define KK1_RELAY_IO_FUNC FUNC_GPIO12

/* -KK2 USE GPIO14- */
#define KK2_RELAY_IO_MUX 	PERIPHS_IO_MUX_MTMS_U
#define KK2_RELAY_IO_NUM 	14
#define KK2_RELAY_IO_FUNC FUNC_GPIO14

/* -KK3 USE GPIO15- */
#define KK3_RELAY_IO_MUX 	PERIPHS_IO_MUX_MTDO_U
#define KK3_RELAY_IO_NUM 	15
#define KK3_RELAY_IO_FUNC FUNC_GPIO15

/* -KK4 USE GPIO16/GPIO5- */
#define KK4_RELAY_IO_MUX  PERIPHS_IO_MUX_GPIO5_U
#define KK4_RELAY_IO_NUM 	 5
#define KK4_RELAY_IO_FUNC  FUNC_GPIO5

typedef void (*key_fn) (void);

/* -KEY FLG- */
#define	LONG_SHORT_MUTEX			0x01 
#define	KEY_LEVEL_NOT_SURE			(0x01<<1)
#define	KEY_ANALOG_UP_INT			(0x01<<2)

struct single_key_param {
	uint8 key_flg;
    uint8 key_level;						
    uint8 gpio_id;
    uint8 gpio_func;
    uint32 gpio_name;
	uint32 long_time;
    os_timer_t key_timer;
	key_fn start_fun;
	key_fn short_fun;
	key_fn long_fun;
};

struct user_key_param{
	uint8 key_num;
	struct single_key_param user_keys[USER_KEYS_NUM];
};


extern int rma_test(char * errmsg);

