
#include "DSP28x_Project.h"
#include "SCIcom.h"
/**********************************************Variable declaration************************************************/
sci_dg  TxData[4];//PFC发送数据
struct _SCI_Reg    SciReg={0};
struct _Rx_PFC_DATA PFCData={0};
unsigned short Frame1[6]={0x11},Frame2[6]={0x33};//发送帧的数组
unsigned short TxFlag=SCI_TX1;//发送帧判断
/** ===================================================================
**     Interrupt handler : ISR_SCI
**     Description :中断接收数据函数
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
__interrupt void ISR_SCI(void)
{
    IER = M_INT1;  // 开中断INT1
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
    __asm("  NOP");
    EINT;

    if (SciaRegs.SCIRXST.bit.RXERROR == 1) //接收数据错误，软件复位错误数据
    {
        SciaRegs.SCICTL1.bit.SWRESET = 0x0;
        SciaRegs.SCICTL1.bit.SWRESET = 0x1;
    }
    if (SciaRegs.SCIRXST.bit.RXWAKE == 1)//接收数据为地址输入，用以判断数据内容类型，帧首字节
    {
        SciReg.RxBuffer=SciaRegs.SCIRXBUF.bit.RXDT;//获取SCI传输数据
        switch (SciReg.RxBuffer&0x0FF)        //根据首字节地址判断数据存储类型
        {
            case RCMD1:  MesRxCmd(MESSAGE1); break;
            case RCMD2:  MesRxCmd(MESSAGE2); break;
            default:break;   //其他数据不接收
        }
    }
    else//非帧首字节
    {
        SciReg.RxBuffer=SciaRegs.SCIRXBUF.bit.RXDT;//暂存SCI接收数据
        if (SciReg.cmd!=0)        //根据地址数据类型存储信息
            MesStore(SciReg.cmd);
    }
     PieCtrlRegs.PIEACK.all = PIEACK_GROUP9;  //清中断
}

/** ===================================================================
**     Interrupt handler : void MesRxCmd(int RxCmd)
**     Description :
**     Parameters  : RxCmd
**     Returns     : Nothing
** ===================================================================
*/
void MesRxCmd(int RxCmd)
{
    SciReg.RXCnt=0;    //接收计数器清零，首字节保存数据类型
    setRegBits(SciReg.cmd,RxCmd);    //数据类型标志位
    SciReg.RxFrame[SciReg.RXCnt]=SciReg.RxBuffer;    //接收数组更新
    SciReg.RXCnt++;    //数据计数器累加
}
/** ===================================================================
**     Interrupt handler : void MesStore(unsigned int mesId)
**     Description :中断接收数据处理函数，将SCI接收寄存器中的数据保存至ACK数组中，若连续接收到6个字节数据，
**     则认为一帧数据接收完成，判断接收数据类型，将对应数据赋值至对应的变量中。
**     Parameters  :mesId表明数据类型
**     Returns     : Nothing
** ===================================================================
*/
void MesStore(unsigned int mesId)
{
    SciReg.RxFrame[SciReg.RXCnt]=SciReg.RxBuffer;    //将SCI寄存器数据保存数组中
    SciReg.RXCnt++;    //接收字节数计数器累加
    if (SciReg.RXCnt>=6)    //当接收到第6个数据，判断一帧数据接收完毕
    {
        SciReg.RxFrame[0]=SciReg.RxFrame[0]&0x00FF;        //只取有效数据
        //计算校验和，若校验和准确，则正常运行，若不准确，则校验和错误计数器累加
        if(SciReg.RxFrame[5]==((~(SciReg.RxFrame[0]+SciReg.RxFrame[1]+SciReg.RxFrame[2]+SciReg.RxFrame[3]+SciReg.RxFrame[4]))&0x00FF))
        {
            switch (mesId)//判断接收的是ACK数据类型
            {
                case MESSAGE1:
                {
                    PFCData.Err=((SciReg.RxFrame[1]<<8)+SciReg.RxFrame[2]);
                    PFCData.State=((SciReg.RxFrame[3]<<8)+SciReg.RxFrame[4]);
                    break;
                }
                case MESSAGE2:
                {
                    PFCData.Vac=((SciReg.RxFrame[1]<<8)+SciReg.RxFrame[2]);
                    PFCData.Iac=((SciReg.RxFrame[3]<<8)+SciReg.RxFrame[4]);
                    break;
                }
            }
        }
        SciReg.RXCnt=0;//清接收数据Cnt，和接受标志位
        SciReg.cmd=0;
    }
}
/*
** ===================================================================
**     Funtion Name : void FrameSent(void)
**     Description :SCI内部通信主函数
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void FrameSent(void)
{
    switch (TxFlag)
    {
        case  SCI_TX1:
        {
            if(SciReg.TXCnt==0)
                FrameBuild(Frame1,&TxData[0],&TxData[1]);//构建一帧数据
            SCI_TX(Frame1);//调用SCI发送数据
            if (getRegBits(SciReg.Flag,TX_FINISH))            //若发送结束，则清发送标志位
            {
                clrRegBits(SciReg.Flag,NEED_SENT);                //清空标志位
                clrRegBits(SciReg.Flag,TX_FINISH);
                TxFlag = SCI_TX2;//下一次发送另外一组
            }
            break;
        }
        case  SCI_TX2:
        {
            if(SciReg.TXCnt==0)
                FrameBuild(Frame2,&TxData[2],&TxData[3]);//构建一帧数据
            SCI_TX(Frame2);//调用SCI发送数据
            if (getRegBits(SciReg.Flag,TX_FINISH))            //若发送结束，则清发送标志位
            {
                clrRegBits(SciReg.Flag,NEED_SENT);                //清空标志位
                clrRegBits(SciReg.Flag,TX_FINISH);
                TxFlag = SCI_TX1;//下一次发送另外一组
            }
            break;
        }
    }
}
/** ===================================================================
**     Funtion Name : void FrameBuild(unsigned short *frame,sci_dg *data1,sci_dg *data2)
**     构建一帧数据，保存于Frame中
**     Parameters  :
**     Returns     : none
** ===================================================================
*/
void FrameBuild(unsigned short *frame,sci_dg *data1,sci_dg *data2)
{
    unsigned short cmd_total=0;int i=1;
    *(frame+1)=(unsigned char)(data1->high);
    *(frame+2)=(unsigned char)(data1->low);
    *(frame+3)=(unsigned char)(data2->high);
    *(frame+4)=(unsigned char)(data2->low);
    for (i=0;i<5;i++)
        cmd_total=cmd_total+*(frame+i);
    *(frame+5)=(unsigned char)((~cmd_total)&0x0FF);
}
/*
** ===================================================================
**     Funtion Name : void SCI_TX(unsigned short *tx_pointer)
**     Description :SCI发送数据
**     Parameters  : *tx_poiter（数组，一帧数据）
**     Returns     : none
** ===================================================================
*/
void SCI_TX(unsigned short *tx_pointer)
{
    if (SciaRegs.SCICTL2.bit.TXEMPTY == 1)//发送寄存器为空
    {
        if(SciReg.TXCnt==0)//首字节增加地址标志位
            SciaRegs.SCICTL1.bit.TXWAKE = 1;
        SciaRegs.SCITXBUF=*(tx_pointer+SciReg.TXCnt);//开始发送
        SciReg.TXCnt++;
        if(SciReg.TXCnt>=6)        //发送一侦数据最后一个数据置相关标志位
        {
            SciReg.TXCnt=0;
            setRegBits(SciReg.Flag,TX_FINISH);
        }
    }
}
/*
** ===================================================================
**     Funtion Name : void TxDataRecord(void)
**     Description : 将需要发送的数据存储到TxData中
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void TxDataRecord(void)
{

}
/*
** ===================================================================
**     Funtion Name : void SciCom(void)
**     SCI内部通信主函数
**     Description :
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void SciCom(void)
{
    static unsigned int Cnt=0;

    Cnt++;
    if(Cnt>20)//100mS发送一帧数据
    {
        Cnt=0;
        TxDataRecord();//发送空数据
        setRegBits(SciReg.Flag, NEED_SENT);//置需要发送标志位
    }
    if(getRegBits(SciReg.Flag, NEED_SENT))//判断SCI是否需要发送数据，
        FrameSent();//开始发送
}
/*
** ===================================================================
**     Funtion Name : SCIValueInit
**     Description : 相关变量初始化
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void SCIValueInit(void)
{
    unsigned char cnt=0;

    clrRegBits(SciReg.Flag,NEED_SENT);    //需要会送标志位
    clrRegBits(SciReg.Flag,TX_FINISH);  //发送结束标志位
    for(cnt=0;cnt<4;cnt++)    //发送寄存器清零
    {
        TxData[cnt].high=0;
        TxData[cnt].low=0;
    }
    PFCData.Vac=0;
    PFCData.Iac=0;
    PFCData.Err=0x0F;
    PFCData.State=0;
 }
