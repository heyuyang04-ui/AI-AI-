
#include "Function.h"
#include "DSP28x_Project.h"
#include "ISR1.h"
#include "SCIcom.h"
#include "CANcom.h"

const int Temper[512] =
{
        125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,123,120,119,117,115,113,111,110,109,107,105,104,103,102,101,100,99, 98, 97, 95, 94,
        93, 92, 91, 91, 90, 89, 88, 87, 86, 86, 85, 85, 84, 84, 83, 82, 81, 81, 80, 79,79, 78, 78, 77, 77, 76, 76, 75, 75, 74, 74, 73, 73, 72, 72, 71, 71, 70, 70, 69,
        69, 68, 68, 67, 67, 67, 66, 66, 65, 65, 65, 64, 64, 64, 63, 63, 63, 62, 62, 61,61, 61, 60, 60, 60, 59, 59, 59, 58, 58, 58, 58, 57, 57, 57, 56, 56, 56, 55, 55,
        55, 54, 54, 54, 54, 53, 53, 53, 52, 52, 52, 51, 51, 51, 51, 50, 50, 50, 50, 49,49, 49, 49, 48, 48, 48, 48, 47, 47, 47, 47, 46, 46, 46, 46, 45, 45, 45, 45, 44,
        44, 44, 44, 43, 43, 43, 43, 43, 42, 42, 42, 42, 42, 41, 41, 41, 41, 40, 40, 40,40, 40, 39, 39, 39, 39, 39, 38, 38, 38, 38, 38, 37, 37, 37, 37, 36, 36, 36, 36,
        36, 35, 35, 35, 35, 35, 34, 34, 34, 34, 34, 33, 33, 33, 33, 33, 33, 32, 32, 32,32, 32, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29, 29, 29, 29, 29, 29, 28, 28,
        28, 28, 28, 27, 27, 27, 27, 27, 26, 26, 26, 26, 26, 26, 25, 25, 25, 25, 25, 24,24, 24, 24, 24, 24, 23, 23, 23, 23, 23, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21,
        21, 21, 20, 20, 20, 20, 20, 19, 19, 19, 19, 19, 19, 18, 18, 18, 18, 18, 17, 17,17, 17, 17, 17, 16, 16, 16, 16, 16, 16, 15, 15, 15, 15, 15, 14, 14, 14, 14, 14,
        14, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 10, 10,10, 10, 10, 9,  9,  9,  9,  9,  9,  8,  8,  8,  8,  8,  7,  7,  7,  7,  7,  6,
        6,  6,  6,  6,  6,  5,  5,  5,  5,  5,  4,  4,  4,  4,  4,  3,  3,  3,  3,  3,2,  2,  2,  2,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  -1, -1,-1,  -1, -2, -2,
        -2, -2, -2, -3, -3, -3, -3, -4, -4, -4, -4, -5, -5, -5, -5, -6, -6, -6, -6, -7,-7, -7, -7, -8, -8, -8, -8, -9, -9, -9, -9, -10,-10,-10,-10,-11,-11,-11,-12,-12,
        -12,-13,-13,-13,-13,-14,-14,-14,-15,-15,-15,-16,-16,-16,-17,-17,-17,-18,-18,-19,-19,-19,-20,-20,-20,-21,-21,-22,-22,-23,-23,-23,-24,-24,-25,-25,-26,-26,-27,-27,
        -28,-28,-29,-29,-30,-31,-31,-32,-33,-34,-34,-35,-36,-37,-37,-38,-39,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40,-40
};
//软启动状态标志位
SState_M 	STState = SSInit ;

unsigned  char RsingInitFlag=0;
/** ===================================================================
**     Funtion Name :void StateMInit(void)
**     Description :   初始化状态函数，参数函数初始化
**     Parameters  :
**     Returns     :
** ===================================================================*/
void StateMInit(void)
{
    static char FirstFlag=0;

    if(FirstFlag==0)
    {
        FirstFlag=1;
        //相关参数初始化
        ValInit();
        //SCI通信初始化
        SCIValueInit();
    }
    //电流采样偏置采样求平均已经完成，则跳转状态
    if(IoutOffsetCal()==1)
        DF.SMFlag  = Wait;    //状态机跳转至等待软启状态
}
/*
** ===================================================================
**     Funtion Name :char IoutOffsetCal(void)
**     Description :电流采样偏置
**     Parameters  : none
**     Returns     : 计算完成返回1，否则返回0
** ===================================================================
*/
char IoutOffsetCal(void)
{
    //计算电流零偏计数变量
    static int Cnt=0;
    //零偏矫正结束标志位
    static  char Flag=0;
    //滑动平均和
    static long IoutOffsetSum=0;

    //采样累加
    IoutOffsetSum += AdcResult.ADCRESULT1<<3;
    //零偏累加
    Cnt++;
    //累加256次
    if(Cnt>=256)
    {
        //累加计时清零
        Cnt=0;
        //求R相电流采样OFFSET 的256次平均
        SADC.IoutOffset = IoutOffsetSum>>8;
        Flag=1;
    }
    //返回零偏矫正结束标志位置位
    return Flag;
}
/** ===================================================================
**     Funtion Name :void ValInit(void)
**     Description :   相关参数初始化函数
**     Parameters  :无
**     Returns     :无
** ===================================================================*/
void ValInit(void)
{
    //关闭PWM
    PWMDis();
    //禁止Burst功能
    clrRegBits(DF.CtrFlag, F_BURST);
    //清除故障标志位
    DF.ErrFlag=0;
    //控制标志位
    DF.CtrFlag=0;
    //初始化电压参考量*V
    ID2DCtrValue.Voref=0;
    ID2DCtrValue.CanVoref =22060;//预设最低48V电压
    ID2DCtrValue.Vlimit =0;
    //初始化电流参考量*A
    ID2DCtrValue.Ioref = 0;
    ID2DCtrValue.CanIoref =14895;//预设10A电流
}
/** ===================================================================
**     Funtion Name :void StateMWait(void)
**     Description :   等待状态机，等待*S后无故障则软启
**     Parameters  :无
**     Returns     :无
** ===================================================================*/
void StateMWait(void)
{
   //计数器定义
    static unsigned int CntS = 0;

   //等待*秒
   if(CntS>1000)
   {
       CntS=1000;
       //在DCDC和PFC无故障情况下启动，且进入正常运行状态，且PFC准备好后，则进入启动状态
       if((DF.ErrFlag==F_NOERR)&&(PFCData.Err==0)&&(PFCData.State==Run)&&(getRegBits(DF.CtrFlag, F_PFC_OK))&&(getRegBits(DF.CtrFlag, F_OUT_EN)))
       {
           //计数器清0
           CntS=0;
           //状态标志位跳转至等待状态
            DF.SMFlag  = Rise;
            //软启动子状态跳转至初始化状态
            STState = SSInit;
       }
   }
   else
       CntS++;
}
/*
** ===================================================================
**     Funtion Name : void StateMRise(void)
**     Description :软启阶段
**     软启初始化
**     软启等待
**     开始软启
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define I_PRI_OCP_DSP 672//原边过流保护点 *A
#define MAX_SSCNT 20//等待*毫秒
#define MAX_PD  857//最低频限制（周期值最大）*kHz
void StateMRise(void)
{
    //计时器
    static  unsigned int  Cnt = 0;

    //判断软启状态
    switch(STState)
     {
         //初始化状态
        case    SSInit:
         {
             //启动标志位清0
             RsingInitFlag=0;
             //禁止Burst功能
             clrRegBits(DF.CtrFlag, F_BURST);
             //关闭PWM
             PWMDis();
             EALLOW;
             //设置原边过流保护值
             Comp1Regs.DACVAL.bit.DACVAL =I_PRI_OCP_DSP;
             EDIS;
             //软启中将运行最大周期值（最低频率）设定在最小值，即高频小占空比启动
             ID2DCtrValue.MaxPD  = MIN_BURST+1;
             //小Duty启动,大死区
             ID2DCtrValue.DT = MAX_DT;
             //从一半开始启动
             ID2DCtrValue.Ioref = ID2DCtrValue.CanIoref >>1;
             //跳转至软启等待状态
             STState = SSWait;
             break;
         }
         //等待软启动状态
         case    SSWait:
         {
             //计数器累加
             Cnt++;
             //等待100ms
             if(Cnt> MAX_SSCNT)
             {
                 //计数器清0
                 Cnt = 0;
                 ID2DCtrValue.PD=MIN_BURST+1;
                 //更新寄存器
                 RegReflash();
                //使能Burst模式
                 setRegBits(DF.CtrFlag, F_BURST);
                 //打开PWM
                 PWMEn();
                 //跳转至软启状态
                 STState = SSRun;
             }
             break;
         }
         //软启动状态
         case    SSRun:
         {
                 //启动标志位置1
                 RsingInitFlag=1;
                 //运行最低频限制 逐渐增加
                 ID2DCtrValue.MaxPD = ID2DCtrValue.MaxPD+ 5;
                 //死区逐渐减小，占空比逐渐增大
                 ID2DCtrValue.DT = ID2DCtrValue.DT-1;
                 //运行最低频限制 最大值限制
                 if(ID2DCtrValue.MaxPD > MAX_PD)
                     ID2DCtrValue.MaxPD  = MAX_PD;
                 if(ID2DCtrValue.DT<MIN_DT)
                      ID2DCtrValue.DT = MIN_DT;
               //当运行最低频限制跑到最大值时，软启结束
              if(  (ID2DCtrValue.MaxPD  == MAX_PD)&&(ID2DCtrValue.DT == MIN_DT) )
             {
                  //状态机跳转至运行状态
                  DF.SMFlag  = Run;
                  //软启动子状态跳转至初始化状态
                  STState = SSInit;
               }
             break;
         }
         default:
         break;
     }
}
/*
** ===================================================================
**     Funtion Name :void StateMRun(void)
**     Description :正常运行（空），主函数进程在中断中运行
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void StateMRun(void)
{

}
/*
** ===================================================================
**     Funtion Name :void StateMErr(void)
**     Description :故障状态
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
unsigned char   OppFlag=0;
void StateMErr(void)
{
    //禁止Burst功能
    clrRegBits(DF.CtrFlag, F_BURST);
    //关闭PWM
    PWMDis();
    //若故障消除跳转至等待重新软启
    if(DF.ErrFlag==F_NOERR)
    {
        OppFlag=0;
        EALLOW;
        //清寄存器
        EPwm1Regs.TZCLR.bit.OST=1;
        EDIS;
        DF.SMFlag  = Wait;
    }
}
/*
** ===================================================================
**     Funtion Name :void CanOutEn(void)
**     Description :CAN控制输出开关机
**
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void CanOutEn(void)
{
      //状态机在RUNNING状态下处理关机命令
    if((!getRegBits(DF.CtrFlag, F_OUT_EN))&& ((DF.SMFlag == Rise) || (DF.SMFlag == Run)))
    {
        //关Burst
        clrRegBits(DF.CtrFlag, F_BURST);
        //关闭PWM
        PWMDis();
        //跳转到空闲状态等待PFC ok
        DF.SMFlag = Wait;
    }
}
/*
** ===================================================================
**     Funtion Name :void PFCokCheck(void)
**     Description :只有PFC为OK的时候DCDC才允许启动
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define PFCok GpioDataRegs.GPADAT.bit.GPIO24
void PFCokCheck(void)
{
    if(PFCok)//PFC已经准备好
        setRegBits(DF.CtrFlag, F_PFC_OK);//置标志位
    else
        clrRegBits(DF.CtrFlag, F_PFC_OK);//清标志位

    //当DCDC正在启动或者运行过程中，若PFC突然进入不OK状态，则DCDC关闭，跳转到空闲状态等待
    if((!getRegBits(DF.CtrFlag, F_PFC_OK)) && ((DF.SMFlag ==Rise)||(DF.SMFlag ==Run)))
    {
        //关Burst
        clrRegBits(DF.CtrFlag, F_BURST);
        //关闭PWM
        PWMDis();
        //跳转到空闲状态等待PFC ok
        DF.SMFlag = Wait;
    }
}
/*
** ===================================================================
**     Funtion Name :void HwOpp(void)
**     Description :原边电流过流保护，单周期封波，Peak电流保护
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void HwOpp(void)
{
   // 比较器翻转，发生原边电流过大封波
   if(( EPwm1Regs.TZFLG.bit.OST==1)&&(OppFlag ==0))
   {
       //关Burst
       clrRegBits(DF.CtrFlag, F_BURST);
       //关闭PWM
       PWMDis();
       //故障标志位
       OppFlag=1;
       //计入故障状态
        DF.SMFlag =Err;
        //置LLC原边过流标志位
        setRegBits(DF.ErrFlag,F_LLC_OCP);
   }
}
/*
** ===================================================================
**     Funtion Name :void SwOCP(void)
**     Description :软件过流保护，关机后等待若干时间后清楚故障，模块重启
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define IO_OCP_DSP 17873//*A，过流保护点
void SwOCP(void)
{
   //过流保护判据保持计数器定义
    static  unsigned int  OCPCnt=0;
    //故障清楚保持计数器定义
    static  unsigned int  RSCnt=0;
    //保留保护重启计数器
    static  unsigned int  RSNum=0;

    //当输出电流大于*A，且保持*毫秒
    if((SADC.IAvg > IO_OCP_DSP)&&(DF.SMFlag  ==Run))
    {
        //条件保持计时
        OCPCnt++;
        //条件保持*毫秒，则认为过流发生
        if(OCPCnt > 20)
        {
            //计数器清0
            OCPCnt  = 0;
            //禁止Burst功能
            clrRegBits(DF.CtrFlag, F_BURST);
            //关闭PWM
            PWMDis();
            //故障标志位
            setRegBits(DF.ErrFlag,F_SW_OCP);
            //跳转至故障状态
             DF.SMFlag  =Err;
        }
    }
    else
        //计数器清0
        OCPCnt  = 0;

    //输出过流后恢复
    //当发生输出软件过流保护，关机后等待*秒后清除故障信息，进入等待状态等待重启
    if(getRegBits(DF.ErrFlag,F_SW_OCP))
    {
        //等待故障清楚计数器累加
        RSCnt++;
        //等待*秒
        if(RSCnt > 800)
        {
            //计数器清零
            RSCnt=0;
            //过流重启计数器累加
            RSNum++;
            //过流重启只重启*次，*次后不重启，防止输出严重故障
            if(RSNum > 10 )
            {
                //确保不清除故障，不重启
                RSNum =11;
                //禁止Burst功能
                clrRegBits(DF.CtrFlag, F_BURST);
                //关闭PWM
                PWMDis();
            }
            else
            {
               //清除过流保护故障标志位
                clrRegBits(DF.ErrFlag,F_SW_OCP);
            }
        }
    }
}
/*
** ===================================================================
**     Funtion Name :void SwOVP(void)
**     Description :软件过压保护，不重启
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define VO_OVP_DSP 23163//*V过压保护
void SwOVP(void)
{
    //过压保护判据保持计数器定义
    static  unsigned int  OVPCnt=0;

    //当输出电流大于*V，且保持*毫秒
    if (SADC.Vout>VO_OVP_DSP)
    {
        //条件保持计时
        OVPCnt++;
        //条件保持*毫秒
        if(OVPCnt > 2)
        {
            //计时器清零
            OVPCnt=0;
            //禁止Burst功能
            clrRegBits(DF.CtrFlag, F_BURST);
            //关闭PWM
            PWMDis();
            //故障标志位
            setRegBits(DF.ErrFlag,F_SW_OVP);
            //跳转至故障状态
             DF.SMFlag  =Err;
        }
    }
    else
        OVPCnt = 0;
}
/*
** ===================================================================
**     Funtion Name :void SwUVP(void)
**     Description :软件欠压保护
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define VO_UVP_DSP 9927//*V欠压保护
void SwUVP(void)
{
    //过压保护判据保持计数器定义
    static  unsigned int  UVPCnt=0;

    //当输出电流小于*V，且保持*毫秒
    if ((SADC.VAvg < VO_UVP_DSP)&&(DF.SMFlag  ==Run))
    {
        //条件保持计时
        UVPCnt++;
        //条件保持*毫秒
        if(UVPCnt> 100)
        {
            //计时器清零
            UVPCnt=0;
            //禁止Burst功能
            clrRegBits(DF.CtrFlag, F_BURST);
            //关闭PWM
            PWMDis();
            //故障标志位
            setRegBits(DF.ErrFlag,F_SW_UVP);
            //跳转至故障状态
             DF.SMFlag  =Err;
        }
    }
    else
        UVPCnt = 0;
}
/*
** ===================================================================
**     Funtion Name :void SwOTP(void)
**     Description :散热器过温保护
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define     MAX_HIGHEST_TEMP    90 //50摄氏度
#define     MAX_HIGHEST_TEMP_RE    80 //40摄氏度
void SwOTP(void)
{
    static  unsigned int  OTPCnt=0;    //mos过温保护计时

    SADC.MosDeg = Temper[((unsigned int)SADC.SinkT1>>6)]+40;  //获取Mos实际的散热器温度值，以度数为单位，且向上相加40（没有负的）
    SADC.DioDeg = Temper[((unsigned int)SADC.SinkT2>>6)]+40;  //获取Dio实际的散热器温度值，以度数为单位，且向上相加40（没有负的）

    //D2D mos过温保护
    if(  (SADC.MosDeg > MAX_HIGHEST_TEMP) || (SADC.DioDeg > MAX_HIGHEST_TEMP) )
    {
        OTPCnt++;
        if(OTPCnt>200) //保持1S
        {
            OTPCnt=0;
            //禁止Burst功能
            clrRegBits(DF.CtrFlag, F_BURST);
            //关闭PWM
            PWMDis();
            //故障标志位
            setRegBits(DF.ErrFlag,F_SW_OTP);
            //跳转至故障状态
             DF.SMFlag  =Err;
        }
    }
    else//不满足 计时复位清零
        OTPCnt=0;

    //过温保护后恢复
    //当输出过温保护后，若散热器温度下降到40度以下，重新启动
    if(getRegBits(DF.ErrFlag,F_SW_OTP))
    {
       //所有器件(D2D两个MOS散热器、两个二极管散热器、PFC的散热器最高温度)温度均恢复至可重启温度范围以内
        if(  (SADC.MosDeg < MAX_HIGHEST_TEMP_RE) && (SADC.DioDeg < MAX_HIGHEST_TEMP_RE) )
           clrRegBits(DF.ErrFlag,F_SW_OTP);            //清除故障标志位
    }
}
/*
** ===================================================================
**     Funtion Name :void ShortOff(void)
**     Description :短路保护，可以重启10次
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define IO_SHORT_DSP     22342//短路电流判据*A
#define VO_SHORT_DSP     1103//短路电流判据*V
void ShortOff(void)
{
    static long RSCnt = 0;
    static unsigned char RSNum =0 ;

    //当输出电流大于*A，且电压小于*V时，可判定为发生短路保护
    if((SADC.Iout> IO_SHORT_DSP)&&(SADC.Vout <VO_SHORT_DSP))
    {
        //禁止Burst功能
        clrRegBits(DF.CtrFlag, F_BURST);
        //关闭PWM
        PWMDis();
        //故障标志位
        setRegBits(DF.ErrFlag,F_SW_SHORT);
        //跳转至故障状态
         DF.SMFlag  =Err;
    }
    //输出短路保护恢复
    //当发生输出短路保护，关机后等待4S后清楚故障信息，进入等待状态等待重启
    if((getRegBits(DF.ErrFlag,F_SW_SHORT))||(getRegBits(DF.ErrFlag,F_LLC_OCP)))
    {
        //等待故障清楚计数器累加
        RSCnt++;
        //等待*秒
        if(RSCnt >200000)
        {
            //计数器清零
            RSCnt=0;
            //短路重启只重启*0次，*次后不重启
            if(RSNum > 10)
            {
                //确保不清除故障，不重启
                RSNum =11;
                //禁止Burst功能
                clrRegBits(DF.CtrFlag, F_BURST);
                //关闭PWM
                PWMDis();
            }
            else
            {
                //短路重启计数器累加
                RSNum++;
                //清除过流保护故障标志位
                clrRegBits(DF.ErrFlag,F_SW_SHORT);
                //短路时LLC原边也会OCP，清楚OCP后重启
                clrRegBits(DF.ErrFlag,F_LLC_OCP);
            }
         }
    }
}
/*
** ===================================================================
**     Funtion Name : void CANLost(void)
**     Description : CAN丢失保护
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void CANLost(void)
{
    //6S内没收到上位机的任何信息
    if(CanDat.LostCnt  > 1200)
    {
        CanDat.LostCnt =0;
        //禁止Burst功能
        clrRegBits(DF.CtrFlag, F_BURST);
        //关闭PWM
        PWMDis();
        //故障标志位
        setRegBits(DF.ErrFlag,F_CAN_LOST);
        //跳转至故障状态
        DF.SMFlag  =Err;
    }
}
/*
** ===================================================================
**     Funtion Name :void PWMEn(void)
**     Description :打开PWM
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void PWMEn(void)
{
    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO0=1;
    GpioCtrlRegs.GPAMUX1.bit.GPIO1=1;
    EDIS;
}
/*
** ===================================================================
**     Funtion Name :void PWMDis(void)
**     Description :关闭PWM
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void PWMDis(void)
{
    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO0=0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO1=0;
    GpioCtrlRegs.GPADIR.bit.GPIO0=1;
    GpioCtrlRegs.GPADIR.bit.GPIO1=1;
    GpioDataRegs.GPACLEAR.bit.GPIO0=1;
    GpioDataRegs.GPACLEAR.bit.GPIO1=1;
    EDIS;
}
/*
** ===================================================================
**     Funtion Name :void PWMDAC(unsigned int para)
**     Description : 用PWM口模拟DAC输出
**     输入：数字量Q15代表3.3V，标定方式
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define PWMDAC_PERIOD 300
void PWMDAC(long para)
{
    static long Period=0;

    Period = para * PWMDAC_PERIOD>>15;
    EALLOW;
    EPwm4Regs.CMPA.half.CMPA =Period; // adjust duty for output EPWM4A
    EDIS;
}
