#ifndef _SWT_H
#define _SWT_H

#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"


#define MAX_TASK	4
//-----------------------------------------------------------------------------
// Types and defines:
//-----------------------------------------------------------------------------
//
#define INIT_TASK_HANDLE					-1	//如果Task沒被加入
//==========給 Swt flag用的==============

typedef  enum{

    SWTFlag_STATIC_SPACE	=1,	//設定的話，所使用的*Data為靜態的空間
    SWTFlag_SUSPEND			=2,	//設定的話，代表task處於暫停狀態，否則是處於動作的狀態，差別是沒有計時與有計時
    SWTFlag_WORKING			=4,	//設定的話，代表task處於工作狀態
    SWTFlag_DELETE			=8	//設定的話，代表task處於刪除狀態

}SWTFlag;


//=============================================

#define SWT_TICK_RESOLUTION				30	//在PC上可以設到4ms

/*Setup the control variable */
#define UP					1
#define DOWN				2
#define PERIODIC			4
#define SINGLE				8

/*The status flag in swt node*/
#define SCH_IDEL			1
#define SCH_WAIT_TIMEOUT	2
#define SCH_TASK_START		3


typedef int32_t SwtHandle;
#define INIT_TASK(h) h=(SwtHandle)-1;
#define SWT_TASK_INITIALIZER -1

typedef struct _Swt
{
    void	*Data;						//指向附帶的資訊
    void	(*pfunc)(void *context);	//工作函式，會將自己的SCHEDULER結構傳入
    int32_t	Delay;						//task加入後，經過delay ms才會執行
    int32_t	Period;						//執行週期
    SWTFlag	Flag;						//參照SCH_FLAG_   開頭定義
    uint32_t	LossTick;					//以SWT_TICK_RESOLUTION為單位，看Loss了幾個SWT_TICK_RESOLUTION
    char    *Name;
    SwtHandle *handle;					//儲存從外部傳入的task description，用來在單擊的時候自動將它初始化

} Swt;


int SWT_Init( void );
SwtHandle SWT_AddTask(SwtHandle *handle,
                 void (*pfunc)(void *param),
                 void *Data,
                 const int Delay,
                 const int Period,
                 const char *Name
                 );

uint8_t SWT_DeleteTask( SwtHandle swt_handle );


#endif
