#include "swt.h"
#include "stdio.h"

#define memset os_memset
#define memcpy os_memcpy
#define strcmp os_strcmp
#define strncmp os_strncmp


#define infof( fmt, args...) \
    do {\
        os_printf( "(%s:%d): " fmt, __FILE__ , __LINE__ ,##args);\
    } while(0)

volatile static uint32_t LossTickNum;
static uint32_t AmountOfTask=0;
volatile static Swt swt_array[MAX_TASK];


void SWTplatform_WaitTick(){
    //block until time's up
}

void SWTplatform_Lock(){
    //mutex_SWTplatform_Lock
}

void SWTplatform_InitLock(){
    //mutex_SWTplatform_Lock
}


void SWTplatform_Unlock(){
    //mutex_SWTplatform_Unlock
}

void SWTplatform_AddTick()
{
    LossTickNum++;
}


/**
 * @brief SWT_GetTaskDelay
 *
 * Get a period of a task
 *
 * @param swt_handle get from SWT_AddTask
 * @return a unit of ms
 */
int SWT_GetTaskDelay( SwtHandle swt_handle )
{
    return swt_array[swt_handle].Delay;
}

/**
 * @brief SWT_GetTaskFlag
 * @param swt_handle get from SWT_AddTask
 * @return a status of a task
 */
int SWT_GetTaskFlag( SwtHandle swt_handle )
{
    return swt_array[swt_handle].Flag;
}

/**
 * @brief SWT_IsTaskSuspend
 * @param swt_handle get from SWT_AddTask
 * @return Is a task suspend,1:suspend,0:is working
 */
int SWT_IsTaskSuspend( SwtHandle swt_handle )
{
    return ( swt_array[swt_handle].Flag & SWTFlag_SUSPEND ) ? 1 : 0;
}


//--------------------------------------------------------
// Function Name : Set_TaskSuspend
// Purpose       : 讓task暫停
// Input         : 
// Output        : None
// return value  : >=0	: id
//                 <0	: fail
// Modified Date : 2008/01/21 by HawkHsieh
//--------------------------------------------------------
void Set_TaskSuspend( SwtHandle swt_handle )
{
    swt_array[swt_handle].Flag |= SWTFlag_SUSPEND;
}

//--------------------------------------------------------
// Function Name : Clr_TaskSuspend
// Purpose       : 讓task繼續跑
// Input         : 
// Output        : None
// return value  : >=0	: id
//                 <0	: fail
// Modified Date : 2007/01/21 by HawkHsieh
//--------------------------------------------------------
void Clr_TaskSuspend( SwtHandle swt_handle )
{
    swt_array[swt_handle].Flag &= ~SWTFlag_SUSPEND;
}

//--------------------------------------------------------
// Function Name : SWT_FindTask
// Purpose       : 尋找一個task
// Input         : 
// Output        : None
// return value  : 0 failed
//				   1 successed
// Modified Date : 2009/10/21 by HawkHsieh
//--------------------------------------------------------
static int SWT_FindTask( char *TaskName )
{
    int SchIndex;
    Swt *coTask;

    for ( SchIndex=0 ; SchIndex<MAX_TASK ; SchIndex++ )
    {
        coTask = (Swt *)&swt_array[SchIndex];
        if ( strncmp( coTask->Name ,TaskName , 3) == 0 )
            return SchIndex;
    }
    return -1;
}

//--------------------------------------------------------
// Function Name : SWT_Init
// Purpose       : 系統初始化呼叫一次的涵式
// Input         : 
// Output        : None
// return value  : >0	: timer id
//                 0	: fail
// Modified Date : 2007/04/25 by HawkHsieh
//--------------------------------------------------------
int SWT_Init( void )
{
    LossTickNum = 0;

    SWTplatform_InitLock();

    memset( (void *)swt_array , 0 , sizeof(swt_array) );

    return 1;
}

//--------------------------------------------------------
// Function Name : SWT_Stop
// Purpose       : 設定計時器停止動作
// Input         : 
// Output        : None
// return value  : 1	: success
//                 0	: fail
// Modified Date : 2009/04/06 by HawkHsieh
//--------------------------------------------------------
void SWT_Stop(void)
{
}


//--------------------------------------------------------
// Function Name : SWT_Sleep
// Purpose       : SWT_Sleep.
// Input         : None
// Output        : None
// return value  : None
// Modified Date : 2007/04/24 by HawkHsieh
//--------------------------------------------------------
void SWT_Sleep(void)
{

}

//--------------------------------------------------------
// Function Name : SWT_DeleteTask
// Purpose       : 刪除的動作統一在SWT_DispatchTask進行，此函式只是將這個task變為單擊，因此免去同步的loading
// Input         : 
//				   const uint8_t index		要刪去的id
// Output        : None
// return value  : 1	: success
//                 0	: fail
// Modified Date : 2010/02/25 by HawkHsieh
//--------------------------------------------------------
uint8_t SWT_DeleteTask( SwtHandle swt_handle )
{
    int ret = 0;
    SWTplatform_Lock();

    if ( (swt_handle >= 0) && swt_array[swt_handle].pfunc )
    {
        swt_array[swt_handle].Period = 0;
        swt_array[swt_handle].Flag |= SWTFlag_DELETE;
        ret = 1;
    }

    SWTplatform_Unlock();

    return ret;
}


//--------------------------------------------------------
// Function Name : SWT_PrintTaskStatus
// Purpose       : 印出全部task目前的狀態
// Input         : 
// Output        : None
// return value  :
// Modified Date : 2010/03/09 by HawkHsieh
//--------------------------------------------------------
void SWT_PrintTaskStatus( void )
{
    uint32_t			SchIndex;
    Swt	*coTask=0;
    infof("Total task is %d\n",MAX_TASK);
    for ( SchIndex=0 ; SchIndex<MAX_TASK ; SchIndex++ )
    {
        coTask = (Swt *)&swt_array[SchIndex];
        if ( coTask->pfunc )
        {
            if ( coTask->Flag & SWTFlag_WORKING )
                infof("\nThis task is working\n");

            if ( coTask->Flag & SWTFlag_DELETE )
                infof("\nThis task is going to be deleted\n");

            if ( coTask->Flag & SWTFlag_SUSPEND )
                infof("\nThis task is suspended\n");

            infof("Name:%s,",coTask->Name);
            infof("Delay:%d\t,",coTask->Delay);
            infof("Period:%d\t,",coTask->Period);
            infof("Data:0x%x\t,",(int)coTask->Data);
            infof("Flag:0x%x\t,",coTask->Flag);
            infof("LossTick:%d\t,",coTask->LossTick);
            infof("pfunc:%d,",(int)coTask->pfunc);
            infof("handle:0x%x\n",(int)coTask->handle);

            if ( coTask->Flag & SWTFlag_WORKING )
                infof("\n");
        }
    }
}


//--------------------------------------------------------
// Function Name : SWT_DispatchTask
// Purpose       : 放在無窮回圈中執行的函式，會在收到timer signal的時候通過sigsuspend然後執行時間到的工作，屬於cooperative scheduler
//				   如果此回圈lag長達1024個tick的話，計時將失準，因為跟系統申請的timer的signal可能會因為累積過久而被丟棄，而這也是real time signal的機制
//				   因此這個task不適合用在真實時間的計時排程，只適合在間隔計時的排程
// Input         : 
// Output        : None
// return value  : >0	: timer id
//                 0	: fail
// Modified Date : 2010/02/25 by HawkHsieh
//--------------------------------------------------------
void SWT_DispatchTask( void )
{
    uint32_t			SchIndex;
    Swt	*coTask=0;
    uint32_t			jitter_tick,IsTimeUp;

    SWT_Sleep();			//進入省電模式

    SWTplatform_WaitTick();

    jitter_tick = LossTickNum;	//signal發出後這邊要盡快去把值存起來，以免signal再度來臨而沒給到值。
    LossTickNum = 0;

    for ( SchIndex=0 ; SchIndex<MAX_TASK ; SchIndex++ )
    {
        coTask = (Swt *)&swt_array[SchIndex];

        if ( coTask->pfunc && ((coTask->Flag & SWTFlag_SUSPEND) == 0))	//如果task有被加入而且不在暫停狀態
        {
            coTask->LossTick = jitter_tick;								//計算漏掉了幾個tick，要給Task去估計實際時間的。Schedule不針對jitter做處理。

            coTask->Delay -= SWT_TICK_RESOLUTION;						//如果減完以後是0或負，就執行工作

            IsTimeUp = 0;
            if ( coTask->Delay <= 0)								//如果時間到了
            {
                IsTimeUp = 1;

                if ( coTask->Period > SWT_TICK_RESOLUTION )
                    coTask->Delay += coTask->Period;				//如果工作週期大於tick週期，將時間加回來，計算下一次計時的時間
                else
                    coTask->Delay = 0;								//如果工作週期小於tick週期，長時間會造成Delay累積，可能因此導致overflow讓coTask->Delay變成極大值，因此歸0。

                coTask->Flag |= SWTFlag_WORKING;

                if ( (coTask->Flag & SWTFlag_DELETE) == 0 )	//如果中途刪除，就不執行這次工作
                {

                    coTask->pfunc( swt_array[SchIndex].Data );			//執行工作
                }

                coTask->Flag &= ~SWTFlag_WORKING;

            }

            //如果是單擊的工作，就在時間到後刪除
            if ( (coTask->Period == 0) && (IsTimeUp == 1) )
            {
                //單擊的task要自動把task descriptor給初始化
                SWTplatform_Lock();

                INIT_TASK( *swt_array[SchIndex].handle );
                infof("SYSTEM SCH: Delete task %s\n", swt_array[SchIndex].Name );
                memset( coTask , 0 , sizeof(Swt));
                AmountOfTask--;

                SWTplatform_Unlock();
            }
        }
    }

}



//--------------------------------------------------------
// Function Name : SWT_AddTask
// Purpose       : 加入一個協同工作
// Input         : 
//				   SwtHandle *handle 使用指標是因為要區別唯一的工作
//				   void (*pfunc)(void *),	時間到了以後會執行的函式指標
//				   const int Delay,		一開始執行的毫秒數，設1就會馬上執行
//				   const int Period		更新的毫秒數
// Output        : None
// return value  : =1	: success
//                 =0	: fail
// Modified Date : 2010/03/16 by HawkHsieh
//--------------------------------------------------------
SwtHandle SWT_AddTask(SwtHandle *handle,
                 void (*pfunc)(void *param),
                 void *Data,
                 const int Delay,
                 const int Period,
                 const char *Name
                 )
{

    int SchIndex;

    SWTplatform_Lock();

    if ( *handle != INIT_TASK_HANDLE )
    {
        //尋找是否有已經插入過的Task
        for ( SchIndex=0 ; SchIndex<MAX_TASK ; SchIndex++)
        {
            if ( swt_array[SchIndex].pfunc )
            {
                if ( swt_array[SchIndex].handle == handle )
                {
                    break;
                }
            }
        }

        //如果找到有相等的Task，就傳回錯誤
        if ( SchIndex < MAX_TASK )
        {
            SWTplatform_Unlock();
            return 0;
        }
    }
    for ( SchIndex=0 ; SchIndex<MAX_TASK ; SchIndex++)
    {
        if ( swt_array[SchIndex].pfunc == 0 )
        {
            memset( (void *)&swt_array[SchIndex],0,sizeof(Swt));

            swt_array[SchIndex].Delay  = Delay;

            if ( (Period > 0) && (Period < SWT_TICK_RESOLUTION) )
            {
                swt_array[SchIndex].Period = SWT_TICK_RESOLUTION;
                infof("Your setting of Period:%d is too short to dispatch on time\n",Period);
                infof("Change period to %d\n",SWT_TICK_RESOLUTION);
            }
            else
                swt_array[SchIndex].Period = Period;

            swt_array[SchIndex].pfunc  = pfunc;
            swt_array[SchIndex].Data   = Data;
            swt_array[SchIndex].Name   = (char*)Name;
            swt_array[SchIndex].handle = handle;
            *handle=SchIndex;
            AmountOfTask++;
            break;
        }
    }
    SWTplatform_Unlock();
    if ( SchIndex < MAX_TASK )
    {
        infof("SYSTEM SCH: Number Of Cooperative Task=%d , name=%s , delay=%d , period=%d\n" , AmountOfTask , Name , Delay , Period);
        return SchIndex;
    }
    else
    {
        infof("SYSTEM SCH: The task slot is full, can't contain \"%s\" anymore\n", Name );
        return -1;
    }
}


//--------------------------------------------------------
// Function Name : SWT_RecountTask
// Purpose       : 重新計時，這個功能會造成race condition，當SWT_DispatchTask裡面：
//
//						sch->Delay -= SWT_TICK_RESOLUTION;
//						!!!切換到呼叫SWT_RecountTask的thread!!!
//						if ( sch->Delay <= 0)
//					
//				   這個時候假設sch->Delay -= SWT_TICK_RESOLUTION;，Delay是要小於0的話，照理說是要執行動作
//				   但是別的thread在判斷if ( sch->Delay <= 0)前改了sch->Delay，這樣子就不會執行這次的動作
//				   因此這個race condition的後果就是會少執行一遍，如果某個應用的Delay很長的話，下ㄧ次執行可能
//				   會拖得很久
//
//				   目前是因為都在cooperative schedule裡面呼叫，沒問題，如果其他thread有需要呼叫的話，就要考慮race condition的問題
// Input         : 
// Output        : None
// return value  : 1	: success
//                 0	: fail
// Modified Date : 2010/02/25 by HawkHsieh
//--------------------------------------------------------
int SWT_RecountTask( SwtHandle swt_handle , int Delay, int Period )
{
    int ret = 0;
    SWTplatform_Lock();

    if ( swt_handle > 0 )
    {
        if ( swt_array[swt_handle].pfunc )
        {
            swt_array[swt_handle].Delay = Delay;
            swt_array[swt_handle].Period = Period;
            ret = 1;
        }
    }

    SWTplatform_Unlock();

    return ret;
}


//--------------------------------------------------------
// Function Name : Swt_GetRemainTime
// Purpose       : 取得這個task的下次計時終了的剩餘時間
// Input         : None
// Output        : None
// return value  : None
// Modified Date : 2010/02/25 by HawkHsieh
//--------------------------------------------------------
int Swt_GetRemainTime( SwtHandle swt_handle )
{
    int ret = 0;
    SWTplatform_Lock();

    if ( swt_handle > 0 )
    {
        if ( swt_array[swt_handle].pfunc )
            ret = swt_array[swt_handle].Delay;
    }

    SWTplatform_Unlock();

    return ret;
}



//--------------------------------------------------------
// Function Name : Swt_GetTaskName
// Purpose       : 取得這個task的名子
// Input         : None
// Output        : None
// return value  : None
// Modified Date : 2010/02/25 by HawkHsieh
//--------------------------------------------------------
char *Swt_GetTaskName( SwtHandle swt_handle )
{
    char *ret = 0;
    SWTplatform_Lock();

    if ( swt_handle > 0 )
    {
        if ( swt_array[swt_handle].pfunc )
            ret = (char *)swt_array[swt_handle].Name;
    }else
        infof("Swt_GetTaskName:task invalid\n");


    SWTplatform_Unlock();

    return ret;
}


//--------------------------------------------------------
// Function Name : Swt_IsTaskAdd
// Purpose       : 此排程工作是否已被插入排程
// Input         : None
// Output        : None
// return value  : None
// Modified Date : 2010/02/25 by HawkHsieh
//--------------------------------------------------------
int Swt_IsTaskAdd(SwtHandle swt_handle)
{	
    int ret = 0;
    SWTplatform_Lock();

    if ( swt_handle > 0 )
    {
        if ( swt_handle == INIT_TASK_HANDLE ) return 0;
        ret = ( swt_array[swt_handle].pfunc ) ? 1 : 0;
    }

    SWTplatform_Unlock();

    return ret;

}


//--------------------------------------------------------
// Function Name : Swt_IsTaskWork
// Purpose       : 此排程工作是否正在執行中
// Input         : None
// Output        : None
// return value  : None
// Modified Date : 2010/02/25 by HawkHsieh
//--------------------------------------------------------
int Swt_IsTaskWork( SwtHandle swt_handle )
{
    int ret = 0;
    SWTplatform_Lock();

    if ( swt_handle > 0 )
    {
        ret = ( swt_array[swt_handle].Flag & SWTFlag_WORKING ) ? 1 : 0;
    }

    SWTplatform_Unlock();

    return ret;

}





//--------------------------------------------------------
// Function Name : Swt_GetLossTick
// Purpose       : 取得這個task漏失的tick數量
// Input         : 
// Output        : None
// return value  : >=0	: id
//                 <0	: fail
// Modified Date : 2010/02/25 by HawkHsieh
//--------------------------------------------------------
uint32_t Swt_GetLossTick( SwtHandle swt_handle )
{
    uint32_t ret = 0;
    SWTplatform_Lock();

    if ( swt_handle > 0 )
    {
        ret = swt_array[swt_handle].LossTick;
    }

    SWTplatform_Unlock();

    return ret;
}

