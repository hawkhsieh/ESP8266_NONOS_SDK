#include "indicator.h"
#include "swt.h"
#include "osapi.h"

#define infof( fmt, args...) \
    do {\
        os_printf( "(%s:%d): " fmt, __FILE__ , __LINE__ ,##args);\
    } while(0)

static IDKT_IOWrite write_func;   //set a single gpio
static IDKT_IORead read_func;     //read a signle gpio
static IDKT_IOPutout putout_func; //Put out all of indicator


LOCAL void ICACHE_FLASH_ATTR
gpioBlink(void *timer_arg)
{
    IDKTgpio gpio=(IDKTgpio)timer_arg;
    char bit;

    read_func(gpio,&bit,1);

    if ( bit == IDKTled_On){
        bit=IDKTled_Off;
        write_func(gpio,&bit,1);
    }else{
        bit=IDKTled_On;
        write_func(gpio,&bit,1);
    }
}

typedef struct {
    IDKTgpio *gpioArray;
    int gpioIdx;
}SeqBlink;

LOCAL void ICACHE_FLASH_ATTR
seqGpioBlink(void *timer_arg)
{
    SeqBlink *sb=(SeqBlink*)timer_arg;
    char bit;
    read_func(sb->gpioArray[sb->gpioIdx],&bit,1);
    if ( bit == IDKTled_On ){
        bit=IDKTled_Off;
        write_func(sb->gpioArray[sb->gpioIdx],&bit,1);
    }else{
        bit=IDKTled_On;
        write_func(sb->gpioArray[sb->gpioIdx],&bit,1);
    }
}


LOCAL void ICACHE_FLASH_ATTR
gpioSequence(void *timer_arg)
{
    SeqBlink *sb=(SeqBlink*)timer_arg;

    if ( sb->gpioArray[sb->gpioIdx] == IDKTgpio_EOF ){
        sb->gpioIdx = 0;
    }else{
        sb->gpioIdx++;
    }
}

void ICACHE_FLASH_ATTR
IDKT_Init(IDKT_IOWrite wr , IDKT_IORead rd , IDKT_IOPutout putout ){

    write_func = wr;
    read_func = rd;
    putout_func = putout;
    SWT_Init();

    if (putout_func)
        putout_func();


}

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define BLINK_PERIOD_MS 500
#define FAST_BLINK_PERIOD_100MS 100

LOCAL void ICACHE_FLASH_ATTR
controlSBlinkTask( SwtHandle *handle100ms , SwtHandle *handle1s , SeqBlink *sb ,int reset ,char *name ){

    if (reset){
        infof("Stop sequence blink\n");
        SWT_DeleteTask(*handle100ms);
        SWT_DeleteTask(*handle1s);
    }else{
        infof("Start sequence blink\n");
        SWT_AddTask(handle100ms,seqGpioBlink,(void*)sb,100,100,"sb100");
        SWT_AddTask(handle1s,gpioSequence,(void*)sb,1000,1000,"sb1000");
    }
}

LOCAL void ICACHE_FLASH_ATTR
controlBlinkTask( SwtHandle *handle , int gpio , int reset ,char *name ){
    if (reset){
        infof("Stop blink gpio[%d]\n",gpio);
        SWT_DeleteTask(*handle);
    }else{
        putout_func();
        infof("Start blink gpio[%d]\n",gpio);
         SWT_AddTask(handle,gpioBlink,(void*)gpio,BLINK_PERIOD_MS,BLINK_PERIOD_MS,name);
    }
}

void controlFastBlinkTask( SwtHandle *handle , int gpio , int reset ,char *name ){
    if (reset){
        infof("Stop fast blink gpio[%d]\n",gpio);
        SWT_DeleteTask(*handle);
    }else{
        infof("Start fast blink gpio[%d]\n",gpio);
        SWT_AddTask(handle,gpioBlink,(void*)gpio,FAST_BLINK_PERIOD_100MS,FAST_BLINK_PERIOD_100MS,name);
    }
}


LOCAL void ICACHE_FLASH_ATTR
controlSolidLED( int resetState,int gpio ){
    char off=IDKTled_Off;
    char on=IDKTled_On;

    char bit;
    read_func(gpio,&bit,1);


    if ( resetState ){
        if (bit==IDKTled_Off) return;
        infof("Turn off gpio[%d]\n",gpio);

        write_func(gpio,&off,1);
    }
    else{
        infof("Turn on gpio[%d]\n",gpio);
        write_func(gpio,&on,1);
    }
}

LOCAL void ICACHE_FLASH_ATTR
setState( IDKTstate state ,int resetState ){

    static IDKTstate lastState;

    if ( resetState != 1 && lastState != state && lastState != IDKTstate_Init ){
        setState( lastState  , 1 );
    }

    lastState=state;


    switch( state ){

    case IDKTstate_BOOTUP_SOLID_RED:
        controlSolidLED(resetState,IDKTgpio_RED);
    break;

    case IDKTstate_UPLINK_BLINK_BLUE:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        IDKTgpio gpio=IDKTgpio_BLUE;
        controlBlinkTask(&handle,gpio,resetState,"B-BLUE");
    }
    break;

    case IDKTstate_UPLINK_FAST_BLINK_BLUE:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        controlFastBlinkTask(&handle,IDKTgpio_BLUE,resetState,"FB-BLUE");
    }
    break;

    case IDKTstate_WIFISTART_FBLINK_GREEN:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        controlFastBlinkTask(&handle,IDKTgpio_GREEN,resetState,"FB-GREEN");
    }
    break;

    case IDKTstate_BINDING_BLINK_GREEN:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        controlBlinkTask(&handle,IDKTgpio_GREEN,resetState,"B-GREEN");
    }
    break;

    case IDKTstate_NORMAL_AUTO_SOLID_GREEN:
        controlSolidLED(resetState,IDKTgpio_GREEN);
    break;

    case IDKTstate_WARNING_AUTO_SOLID_BLUE:
        controlSolidLED(resetState,IDKTgpio_BLUE);
    break;

    case IDKTstate_MANUAL_ALLOFF:
        if (putout_func)
            putout_func();
    break;

    case IDKTstate_FW_DOWNLOAD_BLINK_RED:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        controlBlinkTask(&handle,IDKTgpio_RED,resetState,"B-RED");
    }
    break;

    case IDKTstate_FACTORY_RESET_SOLID_RED:
        controlSolidLED(resetState,IDKTgpio_RED);
    break;

    case IDKTstate_HEALTH_CHK_LEDRUN:
    break;

    case IDKTstate_FW_WRITE_BLINK_SQUENCE:
    {
        static SwtHandle handle100ms=SWT_TASK_INITIALIZER;
        static SwtHandle handle1s=SWT_TASK_INITIALIZER;
        static SeqBlink sb;

        if (sb.gpioArray == 0) {
            sb.gpioArray = (IDKTgpio []){IDKTgpio_RED,IDKTgpio_BLUE,IDKTgpio_GREEN,IDKTgpio_EOF};
        }

        controlSBlinkTask(&handle100ms,&handle1s,&sb,resetState,"SBlink");
    }
    break;
    }

    if (resetState && putout_func ){
        putout_func();
    }

}


void ICACHE_FLASH_ATTR
IDKT_SetState( IDKTstate state ){

    setState(state,0);
}

void ICACHE_FLASH_ATTR
IDKT_ResetState( IDKTstate state ){
    setState(state,1);
}
