#include "indicator.h"
#include "swt.h"
#include "osapi.h"


static IDKT_IOWrite write_func;
static IDKT_IORead read_func;


LOCAL void ICACHE_FLASH_ATTR
gpio_blink(void *timer_arg)
{
    int gpio=(int)timer_arg;
    char bit;
    read_func(gpio,&bit,1);
    if ( bit ){
        bit=0;
        write_func(gpio,&bit,1);
    }else{
        bit=1;
        write_func(gpio,&bit,1);
    }
}

static SwtHandle swt1s = SWT_TASK_INITIALIZER;
static SwtHandle swt100ms = SWT_TASK_INITIALIZER;

void ICACHE_FLASH_ATTR
IDKT_Init(IDKT_IOWrite wr , IDKT_IORead rd ){

    write_func = wr;
    read_func = rd;

    SWT_Init();
    SWT_AddTask(&swt1s,gpio_blink,(void*)13,1000,1000,"1s");
    SWT_AddTask(&swt100ms,gpio_blink,(void*)15,100,100,"100");

}


LOCAL void ICACHE_FLASH_ATTR
IDKT_StartBlink( SwtHandle *handle , int gpio ){

    SWT_AddTask( handle ,gpio_blink,(void*)gpio,100,100,"100");
}

LOCAL void ICACHE_FLASH_ATTR
IDKT_StopBlink( SwtHandle *handle ){

    SWT_DeleteTask( handle );
}


void IDKT_SetState( IDKTstate state ,int gpio ){

    char bit_high=1;
    char bit_log=0;
    switch(){

    case IDKTstate_BOOTUP_SOLID_RED:
            write_func(gpio,&bit_high,1);
    break;

    case IDKTstate_UPLINK_BLINK_ORANGE:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        SWT_AddTask(&handle,gpio_blink,(void*)gpio,100,100,"100");
    }
    break;

    case IDKTstate_WPSSTART_BLINK_BLUE:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        SWT_AddTask(&handle,gpio_blink,(void*)gpio,100,100,"100");
    }
    break;

    case IDKTstate_BINDING_BLINK_GREEN:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        SWT_AddTask(&handle,gpio_blink,(void*)gpio,100,100,"100");
    }
    break;

    case IDKTstate_NORMAL_AUTO_SOLID_GREEN:
        write_func(gpio,&bit_high,1);
    break;

    case IDKTstate_WARNING_AUTO_SOLID_ORANGE:
        write_func(gpio,&bit_high,1);
    break;

    case IDKTstate_MANUAL_ALLOFF:
    break;

    case IDKTstate_FW_DOWNLOAD_BLINK_RED:
    {
        static SwtHandle handle=SWT_TASK_INITIALIZER;
        SWT_AddTask(&handle,gpio_blink,(void*)gpio,100,100,"100");
    }
    break;

    case IDKTstate_FACTORY_RESET_SOLID_RED:
            write_func(gpio,&bit_high,1);
    break;

    case IDKTstate_HEALTH_CHK_LEDRUN:
    break;

    case IDKTstate_FW_WRITE_BLINK_SQUENCE:
    break;
    }

}


