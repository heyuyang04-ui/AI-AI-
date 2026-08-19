

#ifndef __Events_H
#define __Events_H

#include "ISR1.h"
#include "ISR2.h"

struct  _Ctr_value
{
    long     PD;//恒压环路最终输出量，对应频率的周期量
    long     MaxPD;//最大周期值，对应最低频率值
    long     DT;//死区变量
    long     Voref;//输出参考电压
    long     CanVoref;//上位机发送的参考电压
    long     Ioref;//参考电流
    long     CanIoref;//上位机发送的参考电流
    long     Vlimit;//限压变量
};

//软启动枚举变量
typedef enum
{
	SSInit,//软启初始化
	SSWait,//软启等待
	SSRun//开始软启
 } SState_M;

 extern SState_M   STState;
 extern unsigned  char RsingInitFlag;

#define     MIN_BURST 200//最高运行频率 150kHz

#define     F_OUT_EN   0x0001
#define     F_PFC_OK    0x0002
#define     F_BURST      0x0004

#define     MIN_DT        20//最小死区
#define     MAX_DT       190//最大死区
 /*****************************故障类型*****************/
#define     F_NOERR      0x0000//无故障
#define     F_SW_OVP    0x0001//输出过压
#define     F_SW_OCP    0x0002//输出过流
#define     F_LLC_OCP    0x0004//器件过温
#define     F_SW_SHORT  0x0008//输出短路
#define     F_SW_UVP  0x0010//输出欠压
#define     F_CAN_LOST  0x0020//CAN丢失
#define     F_SW_OTP  0x0040//过温标志位

/****************************函数声明***************************/
void StateMInit(void);
void StateMWait(void);
void StateMRise(void);
void StateMRun(void);
void StateMErr(void);
void ValInit(void);
char IoutOffsetCal(void);
void CanOutEn(void);
void PFCokCheck(void);
void HwOpp(void);
void SwOCP(void);
void SwOVP(void);
void SwUVP(void);
void SwOTP(void);
void  ShortOff(void);
void CANLost(void);
void  PWMEn(void);
void  PWMDis(void);
void PWMDAC(long para);

#define setRegBits(reg, mask)  (reg |= (unsigned int)(mask))
#define clrRegBits(reg, mask)  (reg &= (unsigned int)(~(unsigned int)(mask)))
#define getRegBits(reg, mask)  (reg & (unsigned int)(mask))
#define getReg(reg)   (reg)

#endif /* __Events_H*/

