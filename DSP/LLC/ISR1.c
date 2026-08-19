
#include "ISR1.h"
#include "Function.h"
#include "DSP28x_Project.h"

struct  _ADI SADC={0};
/** ===================================================================
**     Funtion Name :   __interrupt void ISR_20US(void)
**     Description :   中断优先级为Level1，执行参数采样、环路计算及PWM寄存器更新、Burst控制、快速保护功能，配置ADC采样等。
**     Parameters  :无
**     Returns     :无
** ===================================================================*/
__interrupt void ISR_20US(void)
{
    ADCSample(); //ADC采样函数，采样输出电压电流等相关温度信息
    VLimitCtlPI();//输出限压环
    ILoopCtlPI();//输出恒流环
    BurstCtl();//空载或轻载情况,调频不能满足输出电压需求，Burst控制，
    RegReflash();   //PWM寄存器更新，频率更新
    FastProtection();   //模块快速保护函数，输出短路等

    CpuTimer0Regs.TCR.bit.TIF = 1;    //请中断标志位
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}
/*
** ===================================================================
**     Funtion Name :   void ADCSample(void)
**     Description :    采样输出电压、输出电流、滑动变阻器电压值（输出电压参考值设置）并对其求平均
**     Parameters  :
**     Returns     :
** ===================================================================
*/
void ADCSample(void)
{
    //滑动平均求和中间变量
    static long VSum = 0,ISum = 0;

    //输出电压,ADC本身为Q12位，统一将采样更换成Q15 （左移3位）
    SADC.Vout = AdcResult.ADCRESULT0<<3;
    //运放有零偏
    if(SADC.Vout<200)     SADC.Vout=0;
    //输出电流Q15(减去硬件上的偏置)
    SADC.Iout = (AdcResult.ADCRESULT1<<3)-SADC.IoutOffset;
    //采样限制，限制DSP采样本身存在零偏的情况
    if(SADC.Iout<0)     SADC.Iout=0;
    //采样滑动变阻器值（参考电压）Q15
    SADC.Vadj = AdcResult.ADCRESULT2<<3;
    //散热器温度Q15-MOS温度
    SADC.SinkT1 = AdcResult.ADCRESULT3<<3;
    //散热器温度Q15-二极管温度
    SADC.SinkT2 = AdcResult.ADCRESULT4<<3;

    //输出电压采样滑动求平均
    VSum =SADC.Vout+VSum-(VSum>>8);
    SADC.VAvg =VSum>>8;
    //输出电流采样滑动求平均
    ISum =SADC.Iout+ISum-(ISum>>8);
    SADC.IAvg =ISum>>8;
}
/*
** ===================================================================
**     Funtion Name :  void VLimitCtlPI(void)
**     Description :  限压环
**     当输出电压小于限压值时，限压环输出为零
**     当输出电压大于限压值时，限压环输出为正
**     其输出叠加在电流环的电流参考值上面，通过降低电流环参考值，降低输出电流的方式从而限制输出电压
**     Parameters  :无
**     Returns     :无
** ===================================================================
*/
#define DSP_VLIMIT_PI_KP 300//限压环PI环路P值    Q15
#define DSP_VLIMIT_PI_KI 80//限压环PI环路I值    Q15
void VLimitCtlPI(void)
{
    //电压误差Q15
    static  long   VErr=0;
    //限压环路的输出Q15
    static  long   Vn=0;
    //环路积分量Q30
    static  long   VIntegral=0;

    //计算电压误差量，当输出电压大于参考电压，输出量增加，慢速环
    VErr = SADC.Vout-ID2DCtrValue.Voref;
    //积分量=积分量+KI*误差量
    VIntegral = VIntegral+ VErr*DSP_VLIMIT_PI_KI ;
    //电流环路输出=积分量+KP*误差量
    Vn=(VIntegral + VErr*DSP_VLIMIT_PI_KP )>>15;

    //积分量限制，积分量最小限制
     if( VIntegral < 0)
         VIntegral = 0;
     //积分量限制，积分量最大值限制，10A数字量对应Q15
     if(VIntegral >488064465)
         VIntegral=488064465 ;
     //输出最小限制
      if( Vn < 0)
          Vn = 0;
      //输出最大限制10A
      if(Vn >14895)
          Vn= 14895;

      ID2DCtrValue.Vlimit = Vn;
}
/*
** ===================================================================
**     Funtion Name :  void ILoopCtlPI(void)
**     Description :   输出恒流PI环路计算
**     Parameters  :无
**     Returns     :无
** ===================================================================
*/
#define DSP_I_PI_KP    1000//电流环PI环路P值    Q15
#define DSP_I_PI_KI     100//电流环PI环路I值    Q15
#define MAX_INTE  28086857//LLC运行的最低频率35kHz，即最小PWM周期量    857*Q15
#define MIN_INTE 6553600//LLC运行的最高频率150kHz，即最小PWM周期量   200*Q15
#define MIN_LOOPOUT 199//环路输出最小量限制,最高频率，最小周期量，最小占空比,150kHz   -1余量
#define MAX_LOOPOUT  857//环路输出最大量限，最低频率，最大周期量，最大占空比,35kHz
void ILoopCtlPI(void)
{
    //电流误差Q15
    static  long   IErr=0;
    //电流环路的输出
    static  long   In=0;
    //环路积分量Q30
    static  long  Integral=0;

    //计算电流误差量，当参考电流大于输出电流，频率往小调，周期量往大调，输出量增加
    IErr= ID2DCtrValue.Ioref - SADC.Iout-ID2DCtrValue.Vlimit;
    //积分量=积分量+KI*误差量
    Integral = Integral+IErr*DSP_I_PI_KI ;
    //电流环路输出=KP*误差量+积分量
    In=(IErr *DSP_I_PI_KP +Integral)>>15;

    //积分量限制，积分量最小限制，最高频限制
     if(Integral < MIN_INTE)
         Integral = MIN_INTE;
     //积分量限制，积分量最大值限制，最低频限制，启动过程中ID2DCtrValue.MaxPD软启限制
     if(Integral > (ID2DCtrValue.MaxPD<<15))
         Integral = ID2DCtrValue.MaxPD<<15;

     if(RsingInitFlag==0)//待机状态下环路量清0
     {
         Integral = MIN_INTE;
         In =MIN_LOOPOUT;
     }

     //最终输出量限制，最小值限制
     if(In <= MIN_LOOPOUT)
         In = MIN_LOOPOUT;
     //最终输出量限制，最大值限制
     if(In > MAX_LOOPOUT)
         In = MAX_LOOPOUT;
     //环路输出赋值
     ID2DCtrValue.PD = In;
}
/*
** ===================================================================
**     Funtion Name : void BurstCtl(void)
**     Description :   当调频和调宽的情况下还不能满足低压或轻载情况，则进入Burst模式，关闭PWM，
**     Parameters  :无
**     Returns     :无
** ===================================================================
*/
void BurstCtl(void)
{
    //PWM关闭标志位
    static unsigned char Flag=0;

    //Burst使能放开情况下
    if(getRegBits(DF.CtrFlag, F_BURST))
    {
        // 恒压环的数量足够小的，则关闭PWM
        if (ID2DCtrValue.PD <  MIN_BURST)
        {
            // 关闭PWM，标志位置1
            Flag=1;
            // 关闭PWM，
            PWMDis();
        }
        else
        {
           //检测PWM是否关闭
            if(Flag)
            {
                // 清标志位
                Flag=0;
                // 继续发波
                PWMEn();
            }
        }
    }
}
/*
** ===================================================================
**     Funtion Name : void RegReflash(void)
**     Description :  PWM寄存器更新函数，更新频率周期值，更新LLC运行频率
**     Parameters  :无
**     Returns     :无
** ===================================================================
*/
void RegReflash(void)
{
	//定义频率对应的周期量
    static long  Period = MIN_BURST;
	
    //当环路输出量大于设定最大值时，MaxPD值启动时逐渐减小，用以高频和小占空比启动，限制启动电流过冲
    if(ID2DCtrValue.PD > ID2DCtrValue.MaxPD)
        ID2DCtrValue.PD = ID2DCtrValue.MaxPD;
    if(ID2DCtrValue.PD <MIN_LOOPOUT)
        ID2DCtrValue.PD = MIN_LOOPOUT;
    if(ID2DCtrValue.DT<MIN_DT)
        ID2DCtrValue.DT = MIN_DT;

    Period = ID2DCtrValue.PD ;
	EALLOW;
	//跟新周期值
	EPwm1Regs.TBPRD =Period;
	//更新比较值
	EPwm1Regs.CMPA.half.CMPA=Period>>1;
	//更新寄存器值
	EPwm1Regs.DBFED= ID2DCtrValue.DT;
	EPwm1Regs.DBRED =ID2DCtrValue.DT;
	EDIS;
}
/*
** ===================================================================
**     Funtion Name :void FastProtection(void)
**     Description :  保护函数，输出短路保护和原边硬件过流封波
**     Parameters  :无
**     Returns     :无
** ===================================================================
*/
void FastProtection(void)
{
    if(DF.SMFlag!=Init)
    {
        //输出短路保护
        ShortOff();
        //LLC原边硬件过流封波
        HwOpp();
        //PFC_OK信号检测
        PFCokCheck();
    }
}
