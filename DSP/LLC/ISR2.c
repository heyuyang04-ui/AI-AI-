
#include "ISR2.h"
#include "DSP28x_Project.h"
#include "Function.h"
#include "SCIcom.h"
#include "CANcom.h"

struct  _FLAG    DF={0};
struct  _Ctr_value  ID2DCtrValue={0};
/** ===================================================================
**     Funtion Name : __interrupt void ISR_200Hz(void)
**     Description :   中断优先级为Level2，执行电源状态机和慢速保护，获取电压参考值等
**     Parameters  :
**     Returns     :
** ===================================================================*/
__interrupt void ISR_200Hz(void)
{
    //实现中断嵌套，保证环路控制中断优先级最高
    IER = M_INT1;  // 开中断INT1
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
    __asm("  NOP");
    EINT;  // Enable Global interrupt INTM

    SlowP();  //慢速保护函数
    CanOutEn();//CAN输出开关机
    StateM();//电源状态机函数
    CANVrefGet();//CAN获取参考电压
    CANIrefGet();//CAN获取参考电流
    SciCom();//SCI通信
    LEDShow();  //状态显示灯函数

    CpuTimer1Regs.TCR.bit.TIF = 1;//清中断标志位
    return;
}
/** ===================================================================
**     Funtion Name :void CANVrefGet(void)
**     Description :   从上位机获取参考电压
**     Parameters  :
**     Returns     :
** ===================================================================*/
#define MAX_VREF 22290//输出最大参考电压*V  0.5V的余量
#define MIN_VREF 10800//最低电压参考值*V   0.5V的余量
#define VREF_K 100//递增或递减步长
void CANVrefGet(void)
{
    //缓慢递增或缓慢递减电压参考值
    if( ID2DCtrValue.CanVoref> ( ID2DCtrValue.Voref + VREF_K))
        ID2DCtrValue.Voref = ID2DCtrValue.Voref + VREF_K;
    else if( ID2DCtrValue.CanVoref < ( ID2DCtrValue.Voref - VREF_K ))
        ID2DCtrValue.Voref =ID2DCtrValue.Voref - VREF_K;
    else
        ID2DCtrValue.Voref = ID2DCtrValue.CanVoref ;

    //电压参考值最大最小值*V
     if( ID2DCtrValue.Voref >MAX_VREF )
         ID2DCtrValue.Voref =MAX_VREF;
     if( ID2DCtrValue.Voref <MIN_VREF )
         ID2DCtrValue.Voref =MIN_VREF;
}
/** ===================================================================
**     Funtion Name :void CANIrefGet(void)
**     Description :   从上位机获取参考电流
**     Parameters  :
**     Returns     :
** ===================================================================*/
#define MAX_IREF 14895//输出最大参考电流10A
#define MIN_IREF 0//最低电流参考值*A
#define IREF_K 100//递增或递减步长
void CANIrefGet(void)
{
    //缓慢递增或缓慢递减电压参考值
    if( ID2DCtrValue.CanIoref> ( ID2DCtrValue.Ioref + IREF_K))
        ID2DCtrValue.Ioref = ID2DCtrValue.Ioref + IREF_K;
    else if( ID2DCtrValue.CanIoref < ( ID2DCtrValue.Ioref - IREF_K ))
        ID2DCtrValue.Ioref =ID2DCtrValue.Ioref - IREF_K;
    else
        ID2DCtrValue.Ioref = ID2DCtrValue.CanIoref  ;

    //电流参考值最大值*A
    if( ID2DCtrValue.Ioref >MAX_IREF )
        ID2DCtrValue.Ioref =MAX_IREF;
    if( ID2DCtrValue.Ioref <MIN_IREF )
        ID2DCtrValue.Ioref =MIN_IREF;
}
/** ===================================================================
**     Funtion Name :void StateM()(void)
**     Description :   状态机函数-初始化状态/等外启动状态/启动状态/运行状态/故障状态
**     Parameters  :
**     Returns     :
** ===================================================================*/
void StateM(void)
{
    switch(DF.SMFlag)//判断状态类型
    {
        case  Init :StateMInit(); break;//初始化状态
        case  Wait :StateMWait(); break;//等待状态
        case  Rise :StateMRise(); break; //软启动状态
        case  Run :StateMRun();break;//运行状态
        case  Err :StateMErr();break;//故障状态
    }
}
/** ===================================================================
**     Funtion Name :void SlowP(void)
**     Description :  保护函数，执行输出过压保护，输出过流保护，器件过温保护
**     Parameters  :
**     Returns     :
** ===================================================================*/
void SlowP(void)
{
    if(DF.SMFlag!=Init)
    {
        SwOCP(); //输出软件过流函数
        SwOVP();//输出软件过压保护函数
        SwUVP();//输出欠压保护
        //CANLost();//CAN丢失保护
        SwOTP();//过温保护
    }
}
/** ===================================================================
**     Funtion Name :void LEDShow(void)
**     Description :  LED显示函数-初始化与等待启动状态，红黄绿全亮
**     启动状态，黄绿亮--运行状态，绿灯亮--故障状态，红灯亮
**     Parameters  :
**     Returns     :
** ===================================================================*/
//输出状态灯宏定义
 #define SET_LED_G()             GpioDataRegs.GPBSET.bit.GPIO32 = 1//绿灯亮
 #define SET_LED_Y()             GpioDataRegs.GPASET.bit.GPIO22 = 1//黄灯亮
 #define SET_LED_R()             GpioDataRegs.GPBSET.bit.GPIO33 = 1//红灯亮
 #define CLR_LED_G()             GpioDataRegs.GPBCLEAR.bit.GPIO32= 1//绿灯灭
 #define CLR_LED_Y()             GpioDataRegs.GPACLEAR.bit.GPIO22= 1//黄灯灭
 #define CLR_LED_R()             GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1//红灯灭
void LEDShow(void)
{
    CanDat.RecordCnt++;//CAN上报数据计时器
    CanDat.LostCnt++;//CAN通信丢失告警计数器

    switch(DF.SMFlag)
    {
        case  Init :        //初始化状态，红黄绿全亮
        {
            SET_LED_G();
            SET_LED_Y();
            SET_LED_R();
            break;
        }
        case  Wait :StateMWait();        //等待状态，红黄绿全亮
        {
            SET_LED_G();
            SET_LED_Y();
            SET_LED_R();
            break;
        }
        case  Rise :StateMRise();        //软启动状态，黄绿亮
        {
            SET_LED_G();
            SET_LED_Y();
            CLR_LED_R();
            break;
        }
        case  Run :StateMRun();        //运行状态，绿灯亮
        {
            SET_LED_G();
            CLR_LED_Y();
            CLR_LED_R();
            break;
        }
        case  Err :StateMErr();        //故障状态，红灯亮
        {
            CLR_LED_G();
            CLR_LED_Y();
            SET_LED_R();
            break;
        }
    }
}
