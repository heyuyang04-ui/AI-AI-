/*
 * CANcom.h
 *
 *  Created on: 2021年8月5日
 *      Author: chenge-bjb06
 */

#ifndef CANCOM_H_
#define CANCOM_H_

extern struct _can_para CanDat;

//CAN通信
struct _PDU//协议数据单元（PDU）
{
    unsigned char P;// P为优先权
    unsigned char R;// R位保留位： 备今后开发使用， 本标准设为0。
    unsigned char DP;//为数据页
    unsigned char PF;//PF为PDU格式： 用来确定PDU的格式， 以及数据域对应的参数组编号。
    unsigned char PS;// PS为PDU特定格式： PS值取决于PDU格式。 本标准中采用PDU1格式， PS值为目标地址
    unsigned char SA;// SA为源地址： 发送此报文的源地址
    unsigned char data[8];//DATA为数据域： 若给定参数组数据长度≤8字节， 按照8字节进行传输， 缺省值为00H。 若给定参数组数据长度为9 ～1785， 数据传输需要多个CAN数据帧， 通过协议传输功能通信， 详见7.5的规定。址
};

//模块本身相关信息
struct _can_para
{
    unsigned long LostCnt;//Heatbeat计时，若在一定时间内没被清零，溢出则发生故障
    unsigned long RecordCnt;//遥信遥测数据恢复时间
    unsigned char OutEn;//开关机使能
    unsigned int Voref;//参考电压
    unsigned int Ioref;//参考电流
};

#define PF_SET_CMD 0x01//命令帧
#define PF_SET_CMD_ACK 0x02//命令帧回复
#define PF_HEART_BEAT 0x20//心跳帧
#define PF_RECORD_VALUE 0x10//状态上报帧

void CanInit(void);
__interrupt void sMailboxISR(void);
void CANMessage(void);
unsigned char CANSetCmd(void);
unsigned char SetCmdUpdate(void);
void CANAck(unsigned char type);
void CANTx(void);
void CanRecord(void);

#endif /* CANCOM_H_ */
