
#ifndef ISR2_H_
#define ISR2_H_

#include "Function.h"

extern struct  _Ctr_value   ID2DCtrValue;
extern struct  _FLAG    DF;
//标志位定义
struct  _FLAG
{
     unsigned int       SMFlag;//状态机标志位
     unsigned int       CtrFlag;//控制标志位
     unsigned int       ErrFlag;//故障标志位
};
//状态机枚举量
typedef enum
{
    Init,//初始化
    Wait,//空闲等待
    Rise,//软启
    Run,//正常运行
    Err//故障
}STATE_M;
/**************************************** 函数头文件*********************************************/
__interrupt void ISR_200Hz(void);
void StateM(void);
void CANVrefGet(void);
void CANIrefGet(void);
void SlowP(void);
void LEDShow(void);
#endif
