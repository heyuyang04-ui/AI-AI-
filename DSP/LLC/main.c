
#include "DSP28x_Project.h"
#include "ISR1.h"
#include "ISR2.h"
#include "SCIcom.h"
#include "CANcom.h"

//******************函数声明*********************//
//系统时钟初始化函数声明
void SysCtrlInit(void);
//定时器0初始化函数声明
void Timer0Init(void);
//定时器1初始化函数声明
void Timer1Init(void);
//PWM1初始化函数声明
void EPWMInit(void);
//硬件过流封波
void TZInit(void);
//ADC采样初始化配置函数声明
void ADCInit(void);
//IO口配置初始化函数声明
void GPIOInit(void);
//SCI通信初始化
void SCIInit(void);
//ePWM初始化函数(调试用)
void ePWMDebugInit(void);

extern Uint16 RamfuncsLoadStart;
extern Uint16 RamfuncsLoadSize;
extern Uint16 RamfuncsRunStart;

void main(void)
{
    DINT;//关闭所有中断

    SysCtrlInit(); //初始化时钟
    InitPieCtrl();//复位中断向量
    InitPieVectTable();//初始化中断向量表

    EALLOW;
    PieVectTable.TINT0 = &ISR_20US;//50kHz定时中断函数向量定义
    PieVectTable.TINT1 = &ISR_200Hz;//200Hz定时中断函数向量定义
    PieVectTable.SCIRXINTA=&ISR_SCI;//SCI接收中断
    PieVectTable.ECAN0INTA=&sMailboxISR;    // 函数sMailboxISR设定为CanInt0中断指针
    EDIS;
    
    //将程序复制至RAM内存中运行-此程序可以不需要
    memcpy((Uint16 *)&RamfuncsRunStart,(Uint16 *)&RamfuncsLoadStart,(unsigned long)&RamfuncsLoadSize);
    //调用初始化Flash
    InitFlash();
    //定时器0初始化
    Timer0Init();
    //定时器1初始化
    Timer1Init();
    //PWM1初始化
    EPWMInit();
    //硬件过流封波
    TZInit();
    //ePWM4初始化函数(调试用)
    ePWMDebugInit();
    //ADC采样初始化配置
    ADCInit();
    //IO口配置初始化
    GPIOInit();
    //SCI初始化
    SCIInit();
    //CAN功能初始化
    CanInit();
   //延时5mS后
    DELAY_US(5000);

    EALLOW;//保护,防 止破坏性写入Edit allow
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;//timer 0 中断
    PieCtrlRegs.PIEIER9.bit.INTx1 = 1; // SCI中断使能；
    PieCtrlRegs.PIEIER9.bit.INTx5=1;  // PIE Group 9,使能canA Int0
    EDIS;//Edit disable

    PieCtrlRegs.PIEACK.all = 0xFFFF;
    IER = M_INT1|M_INT9|M_INT13;
    EINT;//开中断
    ERTM;// //使能调试事件
    //延时
    DELAY_US(5000);
    //启动CPU定时器
    CpuTimer0Regs.TCR.bit.TSS = 0;
    CpuTimer1Regs.TCR.bit.TSS = 0;

    EALLOW;
    //开启PWM计时器TB模块时钟功能
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;

    for(;;)
    {
        CANMessage();//CAN数据处理
    }
}

/************************************初始化函数*******************************/

/*****************************************************************************
 *  函    数:void SysCtrlInit(void)
 *  功    能:系统时钟初始化
 *  配置内容：
 *
 *  矫正时钟信号（必须先使能ADC时钟信号）
 *  禁用时钟输出功能
 *  系统时钟配置如下：
 *  系统时钟1使用内部时钟1
 *  系统时钟2使用外部时钟（实际不起作用）
 *  看门狗使用内部时钟1
 *  CUP定时器2使用内部时钟1
 *  CPU定时器2不分频
 *  使能内容部晶振1（时钟1）
 *  暂停时，时钟1信号暂停（默认）
 *  关闭内部晶振2（时钟2）
 *  暂停时，时钟2信号暂停（默认）
 *  暂停时，看门狗时钟信号暂停（默认）
 *  禁止外部时钟输入功能
 *  禁止外部晶振功能
 *  时钟丢失时，立即无延时复位（默认）
 *
 * PLL设置如下：
 * 确认系统时钟正常，系统时钟丢失将停止PLL设置；
 *  确保PLL分频为0，如果非0，将其写0；
 *  关闭丢失时钟检测 PLL切换时防止检测到系统时钟报错
 *  9分频，设置时钟频率，CLKIN*6=60Mhz   CLKIN = 10Mhz;
 *  一直等待，直到PLL时钟锁存；
 *  丢失时钟检测开启；
 *  最终系统时钟： CLKIN*6/1 = 60Mhz;SYSCLKOUT=60Mhz
 *
*****************************************************************************/
void SysCtrlInit(void)
{
    EALLOW;
    //使能ADC时钟
    SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;
    //时钟矫正函数，在这之前要使能ADC时钟
    (*Device_cal)();
    //关闭ADC时钟
    SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 0;

    // 禁用外部时钟功能
    SysCtrlRegs.XCLK.bit.XCLKOUTDIV = 3;

    //时钟控制寄存器配置
    //系统时钟1使用内部时钟1
    SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 0;
    //系统时钟2使用外部时钟（实际不起作用）
    SysCtrlRegs.CLKCTL.bit.OSCCLKSRC2SEL = 0;
    //看门狗使用内部时钟1
    SysCtrlRegs.CLKCTL.bit.WDCLKSRCSEL = 0;
    //CUP定时器2使用内部时钟1
    SysCtrlRegs.CLKCTL.bit.TMR2CLKSRCSEL = 2;
    // CPU定时器2不分频
    SysCtrlRegs.CLKCTL.bit.TMR2CLKPRESCALE = 0;
    //使能内容部晶振1（时钟1）
    SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 0;
    //暂停时，时钟1信号暂停（默认）
    SysCtrlRegs.CLKCTL.bit.INTOSC1HALTI = 0;
    //关闭内部晶振2（时钟2）
    SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 1;
    //暂停时，时钟2信号暂停（默认）
    SysCtrlRegs.CLKCTL.bit.INTOSC2HALTI = 0;
    //暂停时，看门狗时钟信号暂停（默认）
    SysCtrlRegs.CLKCTL.bit.WDHALTI = 0;
    //禁止外部时钟输入功能
    SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 1;
    //禁止外部晶振功能
    SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 1;
    //时钟丢失时，立即无延时复位（默认）
    SysCtrlRegs.CLKCTL.bit.NMIRESETSEL = 0;

    // PLL设置
    // 先将PLL分频寄存器清0，关闭系统时钟检测，修改PLL倍频寄存器，等待PLL时钟锁存；

    //确认系统时钟正常，系统时钟丢失将停止PLL设置；
    if(SysCtrlRegs.PLLSTS.bit.MCLKSTS)
    {
        // 清除丢时钟标志位
        SysCtrlRegs.PLLSTS.bit.MCLKCLR = 1;
        // 系统停止工作
        ESTOP0;
    }
    // 确保PLL分频为0，如果非0，将其写0；
    if((SysCtrlRegs.PLLSTS.bit.DIVSEL == 2) || (SysCtrlRegs.PLLSTS.bit.DIVSEL == 3))
        // PLL分频寄存器写0；
        SysCtrlRegs.PLLSTS.bit.DIVSEL  = 0;
    // 关闭丢失时钟检测 PLL切换时防止检测到系统时钟报错
    SysCtrlRegs.PLLSTS.bit.MCLKOFF = 1;
    // CLKIN*6=60Mhz   CLKIN = 10Mhz;
    SysCtrlRegs.PLLCR.bit.DIV  = 6;
    // 一直等待，直到PLL时钟锁存；
    while(!SysCtrlRegs.PLLSTS.bit.PLLLOCKS);
    // 丢失时钟检测开启；
    SysCtrlRegs.PLLSTS.bit.MCLKOFF = 0;
    // CLKIN*6/1 = 60Mhz;SYSCLKOUT=60Mhz
    SysCtrlRegs.PLLSTS.bit.DIVSEL = 3;

    // ADC CLK开启
    SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;
    // EPWM7 CLK关闭
    SysCtrlRegs.PCLKCR1.bit.EPWM7ENCLK = 0;
    // EPWM6 CLK关闭
    SysCtrlRegs.PCLKCR1.bit.EPWM6ENCLK = 0;
    // EPWM5 CLK开启
    SysCtrlRegs.PCLKCR1.bit.EPWM5ENCLK = 0;
    // EPWM4 CLK开启
    SysCtrlRegs.PCLKCR1.bit.EPWM4ENCLK = 1;
    // EPWM3 CLK开启
    SysCtrlRegs.PCLKCR1.bit.EPWM3ENCLK = 0;
    // EPWM2 CLK开启
    SysCtrlRegs.PCLKCR1.bit.EPWM2ENCLK = 0;
    // EPWM1 CLK开启
    SysCtrlRegs.PCLKCR1.bit.EPWM1ENCLK = 1;
    // CPU Timer1 CLK开启
    SysCtrlRegs.PCLKCR3.bit.CPUTIMER1ENCLK = 1;
    // CPU Timer0 CLK开启
    SysCtrlRegs.PCLKCR3.bit.CPUTIMER0ENCLK = 1;
    // COMP1 CLK开启
    SysCtrlRegs.PCLKCR3.bit.COMP1ENCLK = 1;
    // ECANA CLK开启
    SysCtrlRegs.PCLKCR0.bit.ECANAENCLK = 1;
    // SCIA CLK关闭
    SysCtrlRegs.PCLKCR0.bit.SCIAENCLK = 1;
    // SPIA CLK关闭
    SysCtrlRegs.PCLKCR0.bit.SPIAENCLK = 0;
    // SPIB CLK关闭
    SysCtrlRegs.PCLKCR0.bit.SPIBENCLK = 0;
    // I2C CLK开启
    SysCtrlRegs.PCLKCR0.bit.I2CAENCLK = 1;
    // TBC CLK关闭
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    // LINA CLK关闭
    SysCtrlRegs.PCLKCR0.bit.LINAENCLK = 0;
    // HRPWM CLK关闭
    SysCtrlRegs.PCLKCR0.bit.HRPWMENCLK = 0;
    // EQEP CLK关闭
    SysCtrlRegs.PCLKCR1.bit.EQEP1ENCLK = 0;
    // ECAP2 CLK关闭
    SysCtrlRegs.PCLKCR1.bit.ECAP1ENCLK = 0;
    // CPU Timer2 CLK关闭
    SysCtrlRegs.PCLKCR3.bit.CPUTIMER2ENCLK = 0;
    // COMP3 CLK关闭
    SysCtrlRegs.PCLKCR3.bit.COMP3ENCLK = 0;
    // COMP2 CLK关闭
    SysCtrlRegs.PCLKCR3.bit.COMP2ENCLK =0;
    EDIS;
}

/*****************************************************************************
 * 函    数:void Timer0Init(void)
 * 功    能:20us(50Khz)时钟中断;对信号采集、环路值计算与更新PWM，并计算模块
 * 快速保护功能
 * Parameters  : none
 * Returns     : none
*****************************************************************************/
void Timer0Init(void)
{
    // 关闭Timer0
    CpuTimer0Regs.TCR.bit.TSS = 1;
    // 清除Timer0中断标志
    CpuTimer0Regs.TCR.bit.TIF = 1;
    // 分频1（0x1）,时钟分频为TPR+1;TDDR寄存器为0
    CpuTimer0Regs.TPRH.bit.TDDRH = 0x00;
    CpuTimer0Regs.TPR.bit.TDDR = 0x00;
    // 时钟60Mhz分频3,PRD = 60Mhz/1/50Khz = 1200（0x4B0）
    CpuTimer0Regs.PRD.all = 0x000004B0;
    // 将PRD数据载入TIM,TDDR数据载入PSC；
    CpuTimer0Regs.TCR.bit.TRB = 1;
    // 中断使能
    CpuTimer0Regs.TCR.bit.TIE = 1;
}
/*****************************************************************************
 * 函    数:void Timer1Init(void)
 * 功    能:5ms(200hz)时钟中断
 * Parameters  : none
 * Returns     : none
*****************************************************************************/
void Timer1Init(void)
{
    // 关闭Timer0
    CpuTimer1Regs.TCR.bit.TSS = 1;
    // 清除Timer0中断标志
    CpuTimer1Regs.TCR.bit.TIF = 1;
    // 分频30,时钟分频为TPR+1;TDDR寄存器为29(0x1D）
    CpuTimer1Regs.TPRH.bit.TDDRH = 0x00;
    CpuTimer1Regs.TPR.bit.TDDR = 0x1D;
    // 时钟60Mhz分频3,PRD = 60Mhz/30/200hz = 10000（0x2710）
    CpuTimer1Regs.PRD.all = 0x00002710;
    // 将PRD数据载入TIM,TDDR数据载入PSC；
    CpuTimer1Regs.TCR.bit.TRB = 1;
    // 中断使能
    CpuTimer1Regs.TCR.bit.TIE = 1;
}
/*****************************************************************************
 * 函    数:void EPWMInit(void)
 * 功    能:EPWM1A/B初始化，互补配置；占空比50%；
 * Parameters  : none
 * Returns     : none
*****************************************************************************/
void EPWMInit(void)
{
    EALLOW;

    // GPIO0 <-> EPWM1A-预先设置为普通IO口并拉低
   GpioCtrlRegs.GPAMUX1.bit.GPIO0=0;
   // GPIO1 <-> EPWM1B
   GpioCtrlRegs.GPAMUX1.bit.GPIO1=0;
   // GPIO0 <-> EPWM1A
   GpioCtrlRegs.GPADIR.bit.GPIO0=1;
   // GPIO1 <-> EPWM1B
   GpioCtrlRegs.GPADIR.bit.GPIO1=1;
   // GPIO0 <-> EPWM1A
   GpioDataRegs.GPACLEAR.bit.GPIO0=1;
   // GPIO1 <-> EPWM1B
   GpioDataRegs.GPACLEAR.bit.GPIO1=1;

    //150kHz
    EPwm1Regs.TBPRD =200;
    //不相移，计数器与时钟信号同步
    EPwm1Regs.TBPHS.half.TBPHS = 0;
    //采用向上计数模式；
    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN;
    //禁用相移功能 Master方式
    EPwm1Regs.TBCTL.bit.PHSEN = TB_DISABLE;
    // 使用镜像寄存器,不直接操作TBPRD寄存器
    EPwm1Regs.TBCTL.bit.PRDLD = TB_SHADOW;
    // CTR = ZERO 时，发出同步时钟，
    EPwm1Regs.TBCTL.bit.SYNCOSEL = TB_CTR_ZERO;
    // TBCLK时钟分频； TBCLK = SYSCLKOUT/(CLKDIV * HSPCLKDIV),CLKDIV = 1;
    EPwm1Regs.TBCTL.bit.CLKDIV=TB_DIV1;
    // HSPCLKDIV = 1;  TBCLK = SYSCLKOUT(60Mhz);
    EPwm1Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;
    // CMPA寄存器使用镜像模式；
    EPwm1Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    // CMPB寄存器使用镜像模式；
    EPwm1Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    // 当CTR = 0 时，将CMPA镜像中的数据加载到CMPA执行寄存器中；
    EPwm1Regs.CMPCTL.bit.LOADAMODE = CC_CTR_PRD;
    // 当CTR = 0 时，将CMPB镜像中的数据加载到CMPA执行寄存器中；
    EPwm1Regs.CMPCTL.bit.LOADBMODE = CC_CTR_PRD;
    // 当CTR = 0,拉高
    EPwm1Regs.AQCTLA.bit.CAU = AQ_SET;
    // 当CTR = PERIOD，拉低
    EPwm1Regs.AQCTLA.bit.CAD= AQ_CLEAR;
    // enable Dead-band module  DBA_ALL
    EPwm1Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
    // 上升沿延迟器输出端口不翻转，下升沿延迟器输出端口翻转；
    EPwm1Regs.DBCTL.bit.POLSEL = DB_ACTV_HIC;
    //死区150*1/60MHz
    EPwm1Regs.DBFED = 150;
    //死区150*1/60MHz
    EPwm1Regs.DBRED = 150;
    //50%占空比
    EPwm1Regs.CMPA.half.CMPA = 100;

    //在每个PWM的中点位置采样，避开mos开通和关断的干扰对采样的影响
    //使能SOCA触发功能
    EPwm1Regs.ETSEL.bit.SOCAEN  = 1;
    //Cnt=PERIOD信号
    EPwm1Regs.ETSEL.bit.SOCASEL = 2;
    //立即触发Generate pulse on 1st event
    EPwm1Regs.ETPS.bit.SOCAPRD  = 1;
    EDIS;
}
/*****************************************************************************
  * 函    数: void TZInit(void)
  * 功    能:硬件过流封波
  * 用以判定过流
  * 配置如下
  *
 *****************************************************************************/
void TZInit(void)
{
    EALLOW;
    //COMP_OUT GPIO42 <-> OPP_COMP1_OUT
    GpioCtrlRegs.GPBMUX1.bit.GPIO42 = 3;
    // AI02<-> COPM1+
    GpioCtrlRegs.AIOMUX1.bit.AIO2 = 2;
    // GPIO15 <-> TZ1
    GpioCtrlRegs.GPAMUX1.bit.GPIO15 = 1;

    // 接受数据连续两个SYSCLKOUT时钟一致，COMPOUT才输出；
    Comp1Regs.COMPCTL.bit.QUALSEL = 5;
    // 输出信号饭庄；COMP+>COMP-,输出低电平；反之，输出高；
    Comp1Regs.COMPCTL.bit.CMPINV = 1;
    // COMP-链接内部DAC模块
    Comp1Regs.COMPCTL.bit.COMPSOURCE = 0;
    // 内部DAC有DAC Valu寄存器控制；
    Comp1Regs.DACCTL.bit.DACSOURCE = 0;
    //保护点
    Comp1Regs.DACVAL.bit.DACVAL =500;
    // 比较器和DAC使能；
    Comp1Regs.COMPCTL.bit.COMPDACEN = 1;

    //TZ1 will be one shot signal for EPWM1
    EPwm1Regs.TZSEL.bit.OSHT1=TZ_ENABLE;
    //TZ will Force EPWM1A to a low state
    EPwm1Regs.TZCTL.bit.TZA=TZ_FORCE_LO;
    //TZ will Force EPWM1B to a low state
    EPwm1Regs.TZCTL.bit.TZB=TZ_FORCE_LO;
    // TZ中断标志位全部清0；
    EPwm1Regs.TZCLR.all = 0xffff;
    // 中断均不使能；
    EPwm1Regs.TZEINT.all = 0;
    // 数值比较器暂不适用；
    EPwm1Regs.TZDCSEL.all = 0;
    EDIS;
}

/*****************************************************************************
 * 函    数:void ePWMDebugInit(void)
 * 功    能:ePWM用以配置用来模拟DAC的效果，用以调试用
 *  配置情况：
 *  ePWM4A配置
 *  配置周期值，200kHz 计数上升下降方式 EPwm1Regs.TBPRD=60M/200k=300
 *  不相移，计数器与时钟信号同步
 *  计数上升下降模式
 *  禁用相移功能 Master方式
 *  周期寄存器使用镜像寄存器，不立即使用
 *  同步模式，同步信号来自系统时钟，且在计数器为0时同步
 *  使用系统时钟作为TB信号时钟
 *  CMPA使用镜像寄存器
 *  CMPA在计数器为0时装载PWM
 *  向上计数时，计数器=0时，置高电平
 *  向下计数时，计数器=CMPA时，置低电平
 *  *****************************************************************************/
void ePWMDebugInit(void)
{
    EALLOW;
    // GPIO0 <-> EPWM4A
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1;

    //配置周期值，200kHz 计数上升下降方式 EPwm1Regs.TBPRD=60M/200k=300
    EPwm4Regs.TBPRD = 300;
    //不相移，计数器与时钟信号同步
    EPwm4Regs.TBPHS.half.TBPHS = 0;
    //计数上升下降模式
    EPwm4Regs.TBCTL.bit.CTRMODE = TB_COUNT_UP;
    //禁用相移功能 Master方式
    EPwm4Regs.TBCTL.bit.PHSEN = TB_DISABLE;
    //周期寄存器使用镜像寄存器，不立即使用
    EPwm4Regs.TBCTL.bit.PRDLD = TB_SHADOW;
    //同步模式，同步信号来自系统时钟，且在计数器为0时同步
    EPwm4Regs.TBCTL.bit.SYNCOSEL = TB_CTR_ZERO;
    //使用系统时钟作为TB信号时钟
    EPwm4Regs.TBCTL.bit.CLKDIV=TB_DIV1;
    EPwm4Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;
    //CMPA使用镜像寄存器
    EPwm4Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    //CMP4在计数器为0时装载PWM
    EPwm4Regs.CMPCTL.bit.LOADAMODE = CC_CTR_PRD;
    //向上计数时，计数器=ZRO 时，置高电平
    EPwm4Regs.AQCTLA.bit.ZRO = AQ_SET;
    //向上计数时，计数器=CMPA时，置低电平
    EPwm4Regs.AQCTLA.bit.CAU = AQ_CLEAR;
    //向上计数时，计数器=ZRO 时，置高电平
    EPwm4Regs.AQCTLB.bit.ZRO = AQ_NO_ACTION;
    //向下计数时，计数器=CMPA时，置低电平
    EPwm4Regs.AQCTLB.bit.CBU = AQ_NO_ACTION;
    // adjust duty for output EPWM4A
    EPwm4Regs.CMPA.half.CMPA =0;
    EDIS;
}

/*****************************************************************************
 * 函    数: ADCInit(void)
 * 功    能:输出电压采样(ADCINTA0);
 *          输出电流采样(ADCINTB0);
 *          温度采样(ADCCINTA3,ADCINTB1);
 *          滑动变阻器采样(ADCINTA1);
 * Parameters  : none
 * Returns     : none
*****************************************************************************/
void ADCInit(void)
{
    EALLOW;
    // ADC 采集数据允许倍覆盖；
    AdcRegs.ADCCTL2.bit.ADCNONOVERLAP = 0;
    // ADCCLK = SYSCLKOUT(60Mhz)
    AdcRegs.ADCCTL2.bit.CLKDIV2EN=0;
    // ADC ONESHOT不启动
    AdcRegs.SOCPRICTL.bit.ONESHOT = 0;

    // SOC0~1，SOC2~3,SOC4~5同时采样；
    AdcRegs.ADCSAMPLEMODE.bit.SIMULEN0=1;
    AdcRegs.ADCSAMPLEMODE.bit.SIMULEN2=1;
    AdcRegs.ADCSAMPLEMODE.bit.SIMULEN4=1;
    // SOC0~5优先级最高
    AdcRegs.SOCPRICTL.bit.SOCPRIORITY = 0x06;

    // SOC0<->ADCINTA0  ADC_VOUT输出电压
    // SOC0<->ADCINTB0  ADC_IOUT输出电流
    AdcRegs.ADCSOC0CTL.bit.CHSEL = 0;
    // ePWM1 SOCA触发采样
    AdcRegs.ADCSOC0CTL.bit.TRIGSEL = 5;
    // 采样窗口16*CLK
    AdcRegs.ADCSOC0CTL.bit.ACQPS   = 0x15;

    // SOC2<->ADCINTA1  ADC_VADJ电位器电压
    // SOC3<->ADCINTB1  DC_MOS_T散热器温度
    AdcRegs.ADCSOC2CTL.bit.CHSEL = 1;
    // ePWM1 SOCA触发采样
    AdcRegs.ADCSOC2CTL.bit.TRIGSEL = 5;
    // 采样窗口16*CLK
    AdcRegs.ADCSOC2CTL.bit.ACQPS   = 0x15;

    // SOC4<->ADCINTA3  散热器温度
    // SOC5<->ADCINTB3  ***
    AdcRegs.ADCSOC4CTL.bit.CHSEL = 3;
    // ePWM1 SOCA触发采样
    AdcRegs.ADCSOC4CTL.bit.TRIGSEL = 5;
    // 采样窗口16*CLK
    AdcRegs.ADCSOC4CTL.bit.ACQPS   = 0x15;

    // ADC 模拟电路内部核模块上电
    AdcRegs.ADCCTL1.bit.ADCPWDN = 1;
    // ADC 参考电路内部核模块上电
    AdcRegs.ADCCTL1.bit.ADCREFPWD = 1;
    // ADC Bandgap电路内部核模块上电
    AdcRegs.ADCCTL1.bit.ADCBGPWD = 1;
    // ADC启动；
    AdcRegs.ADCCTL1.bit.ADCENABLE = 1;
    // ADC开始 转换前延迟1ms
    DELAY_US(1000l);
    EDIS;
}
/*****************************************************************************
 * 函    数:void GPIOInit(void)
 * 功    能:IO口初始化配置
 * Parameters  : none
 * Returns     : none
*****************************************************************************/
void GPIOInit(void)
{
    EALLOW;
    // GPIOA DATA数据全清0
    GpioDataRegs.GPADAT.all = 0ul;
    // GPIOB DATA数据全清0
    GpioDataRegs.GPBDAT.all = 0ul;
    //LED G
    GpioCtrlRegs.GPBMUX1.bit.GPIO32=0;//GPIO为普通端口
    GpioCtrlRegs.GPBDIR.bit.GPIO32=1;//方向为输出
    //LED Y
    GpioCtrlRegs.GPAMUX2.bit.GPIO22=0;
    GpioCtrlRegs.GPADIR.bit.GPIO22=1;
    //LED R
    GpioCtrlRegs.GPBMUX1.bit.GPIO33=0;
    GpioCtrlRegs.GPBDIR.bit.GPIO33=1;
    //BUS OK 输入
    GpioCtrlRegs.GPAMUX2.bit.GPIO24=0;
    GpioCtrlRegs.GPADIR.bit.GPIO24=0;

    //调试端口
    GpioCtrlRegs.GPAMUX1.bit.GPIO2=0; //普通IO口
    GpioCtrlRegs.GPADIR.bit.GPIO2=1; //输出配置
    GpioCtrlRegs.GPAMUX1.bit.GPIO3=0; //普通IO口
    GpioCtrlRegs.GPADIR.bit.GPIO3=1; //输出配置
    EDIS;
}

/*****************************************************************************
  * 函    数: void SCIInit(void)
  * 功    能:SCI通信初始化
  * 与后级DCDC通信
  * 配置如下
  *
 *****************************************************************************/
void SCIInit(void)
{
    EALLOW;
    //SCI通信
    GpioCtrlRegs.GPAMUX1.bit.GPIO7 = 2; // GPIO12<-> SCI_TX
    GpioCtrlRegs.GPAMUX1.bit.GPIO12 = 2; // GPIO7 <-> SCI_RX

    //1位停止位
    SciaRegs.SCICCR.bit.STOPBITS = 0x0;
    //8字节数据
    SciaRegs.SCICCR.bit.SCICHAR = 0x7;
    //No parity
    SciaRegs.SCICCR.bit.PARITYENA = 0x0;
    // 禁止Loop模式
    SciaRegs.SCICCR.bit.LOOPBKENA = 0x0;
    // 添加地址为
    SciaRegs.SCICCR.bit.ADDRIDLE_MODE = 0x1;
    //使能RX接收端
    SciaRegs.SCICTL1.bit.RXENA = 0x1;
    //使能TX接收端
    SciaRegs.SCICTL1.bit.TXENA = 0x1;
    //禁止接受错误中断功能
    SciaRegs.SCICTL1.bit.RXERRINTENA = 0x0;
    //禁止睡眠模式
    SciaRegs.SCICTL1.bit.SLEEP = 0x0;
    //允许接收中断使能
    SciaRegs.SCICTL2.bit.RXBKINTENA = 0x1;
    //禁止发送中断使能
    SciaRegs.SCICTL2.bit.TXINTENA = 0x0;
    //60M/4/((0x186+1)*8)=4800  LOSPCP
    SciaRegs.SCIHBAUD    =0x0001;
    SciaRegs.SCILBAUD    =0x0086;
    // Relinquish SCI from Reset
    SciaRegs.SCICTL1.bit.SWRESET = 0x1;

    EDIS;
}
