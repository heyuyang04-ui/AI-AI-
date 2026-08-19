
#ifndef  ISR1_H_
#define ISR1_H_

#include "Function.h"
#include "ISR2.h"

extern  struct  _ADI SADC;
//采样变量结构体
struct _ADI
{
    long   Iout;//输出电流
    long   IoutOffset;//输出电流偏置
    long   IAvg;//输出电流平均值
    long   Vout;//输出电电压
    long   VAvg;//输出电电压平均值
    long   Vadj;//滑动变阻器电压值
    long   VaAvg;//滑动变阻器电压平均值
    long   SinkT1;//散热器温度1
    long   SinkT1Avg;//散热器温度1平均值
    long   SinkT2;//散热器温度2
    long   SinkT2Avg;//散热器温度2平均值
    long   MosDeg;//MOS实际温度值
    long   DioDeg;//二极管温度值
};
/***********************函数声明*********************/
#pragma CODE_SECTION(ISR_20US, "ramfuncs");
#pragma CODE_SECTION(ADCSample, "ramfuncs");
#pragma CODE_SECTION(VLimitCtlPI, "ramfuncs");
#pragma CODE_SECTION(ILoopCtlPI, "ramfuncs");
#pragma CODE_SECTION(BurstCtl, "ramfuncs");
#pragma CODE_SECTION(RegReflash, "ramfuncs");
#pragma CODE_SECTION(FastProtection, "ramfuncs");
__interrupt void ISR_20US(void);
void ADCSample(void);
void VLimitCtlPI(void);
void ILoopCtlPI(void);
void BurstCtl(void);
void RegReflash(void);
void FastProtection(void);

#endif /* ISR1_H_ */
