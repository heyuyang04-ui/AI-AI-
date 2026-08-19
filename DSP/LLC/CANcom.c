
#include "CANcom.h"
#include "Function.h"
#include "SCIcom.h"
#include "DSP28x_Project.h"

struct _PDU PDU={0};////协议数据单元（PDU）
struct _can_para CanDat={0};
/*
** ===================================================================
**     Funtion Name : void CanInit(void)
**     Description :Can初始化程序
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define CAN_ADD 0x80//模块地址
#define BROADCAST 0xFF//广播地址
#define MONITOR_ADD  0xA0//上位机地址
void CanInit(void)
{
    //部分Can的寄存器操作需要影子寄存器，不能直接修改寄存器
    struct ECAN_REGS ECanaShadow;
    //邮箱寄存器
    volatile struct MBOX *Mbox_pointer;
    int MboxCnt=0;

    EALLOW;
    //配置对应的CAN 的IO口
    GpioCtrlRegs.GPAPUD.bit.GPIO30 = 0;     // Enable pull-up for GPIO30 (CANRXA)
    GpioCtrlRegs.GPAPUD.bit.GPIO31 = 0;     // Enable pull-up for GPIO31 (CANTXA)
    GpioCtrlRegs.GPAQSEL2.bit.GPIO30 = 3;   // Asynch qual for GPIO30 (CANRXA)
    GpioCtrlRegs.GPAMUX2.bit.GPIO30 = 1;    // Configure GPIO30 for CANRXA operation
    GpioCtrlRegs.GPAMUX2.bit.GPIO31 = 1;    // Configure GPIO31 for CANTXA operation

    //基础配置
    //配置CAN发送功能， Configure eCAN  TX pins for CAN operation using eCAN regs
    ECanaShadow.CANTIOC.all = ECanaRegs.CANTIOC.all;
    ECanaShadow.CANTIOC.bit.TXFUNC = 1;
    ECanaRegs.CANTIOC.all = ECanaShadow.CANTIOC.all;
    //配置CAN接收功能，Configure eCAN  RX pins for CAN operation using eCAN regs
    ECanaShadow.CANRIOC.all = ECanaRegs.CANRIOC.all;
    ECanaShadow.CANRIOC.bit.RXFUNC = 1;
    ECanaRegs.CANRIOC.all = ECanaShadow.CANRIOC.all;

    // Configure eCAN for ecan(CAN2.0B) mode enable can
    ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
    ECanaShadow.CANMC.bit.SCB = 1;
    ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;

    //请发送ACK标志位，请接收挂起标志位，清中断标志位
    // TAn, RMPn, GIFn bits are all zero upon reset and are cleared again
    // as a matter of precaution.
    ECanaRegs.CANTA.all = 0xFFFFFFFF;       // Clear all TAn bits
    ECanaRegs.CANRMP.all = 0xFFFFFFFF;      // Clear all RMPn bits
    ECanaRegs.CANGIF0.all = 0xFFFFFFFF;     // Clear all interrupt flag bits
    ECanaRegs.CANGIF1.all = 0xFFFFFFFF;

    //初始化所有邮箱内容，上电复位后邮箱状态未定，需要初始化
    // 改写邮箱地址要求先关闭对应邮箱，先将所有邮箱关闭
    //Disable all Mailboxes, Required before writing the MSGIDs
    ECanaShadow.CANME.all = 0;
    ECanaRegs.CANME.all = ECanaShadow.CANME.all;
    Mbox_pointer = &ECanaMboxes.MBOX0;
    for(MboxCnt= 0;MboxCnt<32;MboxCnt++)
    {
        //完备性操作，将所有可能的随机数刷新到确定值0
        Mbox_pointer->MSGID.all = 0;
        Mbox_pointer->MSGCTRL.all = 0;
        Mbox_pointer->MDH.all = 0;
        Mbox_pointer->MDL.all = 0;
        //为所有邮箱配置
        Mbox_pointer->MSGCTRL.bit.DLC = 8;//数据传输长度8字节。
        Mbox_pointer->MSGCTRL.bit.RTR = 0;//没有远程帧
        Mbox_pointer = Mbox_pointer + 1;
    }

    ECanaRegs.CANMIM.all = 0;  //关闭所有邮箱的中断使能位，具体的中断使能情况，将通过CanMaiBoxPreset将邮箱的配置初始值整体写入。
    ECanaRegs.CANMIL.all = 0;    //中断频道，mail isr => int0，所有邮箱都选择Int0中断。集中到一个中断程序内处理。
    ECanaRegs.CANGIM.all = 0x05; //just enabel int0，异常中断在频道Int1。所以所有异常中断没有被使能。

    // Configure bit timing parameters for eCANA Set CCR = 1
    ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
    ECanaShadow.CANMC.bit.CCR = 1 ;
    ECanaShadow.CANMC.bit.ABO = 1;  // Bus自恢复。
    ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;
    // Wait until the CPU has been granted permission to change
    // the configuration registers
    do
    {
        ECanaShadow.CANES.all = ECanaRegs.CANES.all;
    } while(ECanaShadow.CANES.bit.CCE != 1 );   // Wait for CCE bit to be set

    //配置CAN通信波特率
    //复位时基寄存器
    //(60M/2)/((23+1)*(1+1+6+1+1)=125K
    ECanaShadow.CANBTC.all = 0;
    ECanaShadow.CANBTC.bit.BRPREG = 23;
    ECanaShadow.CANBTC.bit.TSEG2REG = 1;
    ECanaShadow.CANBTC.bit.TSEG1REG = 6;
    ECanaShadow.CANBTC.bit.SAM = 0;//The CAN module samples three times and make a majority decision.
    ECanaRegs.CANBTC.all = ECanaShadow.CANBTC.all;

    // Configure bit timing parameters for eCANA Set CCR = 0
    ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
    ECanaShadow.CANMC.bit.CCR = 0 ;
    ECanaShadow.CANMC.bit.STM =  0;  // diable CAN for self-test mode
    ECanaShadow.CANMC.bit.DBO = 0; //低字节优先
    ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;
    // Wait until the CPU no longer has permission to change
    // the configuration registers
    do
    {
        ECanaShadow.CANES.all = ECanaRegs.CANES.all;
    } while(ECanaShadow.CANES.bit.CCE != 0 );   // Wait for CCE bit to clear

    //Disable all Mailboxes, Required before writing the MSGIDs
    ECanaShadow.CANME.all = 0;
    ECanaRegs.CANME.all = ECanaShadow.CANME.all;

    // Configure Mailboxes 0 as Tx, 1as Rx
    ECanaRegs.CANMD.bit.MD0 = 0;//发送
    ECanaRegs.CANMD.bit.MD1= 1;//点对点接收
    ECanaRegs.CANMD.bit.MD2= 1;//广播接收

    //配置帧头
    ECanaMboxes.MBOX0.MSGID.all = ( ECanaMboxes.MBOX0.MSGID.all | CAN_ADD );
    ECanaMboxes.MBOX1.MSGID.all = ( ECanaMboxes.MBOX1.MSGID.all | (((unsigned int)CAN_ADD)<<8)|MONITOR_ADD);
    ECanaMboxes.MBOX2.MSGID.all = ( ECanaMboxes.MBOX2.MSGID.all | (((unsigned int)BROADCAST)<<8)|MONITOR_ADD);
    //启动MASK功能,启动MASK功能,只接收自身地址的命令和广播命令（分组命令控制）
    ECanaMboxes.MBOX1.MSGID.bit.AME = 1;
    ECanaMboxes.MBOX2.MSGID.bit.AME = 1;
    //只判断目标地址屏蔽-只接受上位机和自身地址都相同的数据
    ECanaLAMRegs.LAM1.all = 0x1FFF0000;
    ECanaLAMRegs.LAM2.all = 0x1FFF0000;
    //拓展帧
    ECanaMboxes.MBOX0.MSGID.bit.IDE=1;//拓展帧
    ECanaMboxes.MBOX1.MSGID.bit.IDE=1;//拓展帧
    ECanaMboxes.MBOX2.MSGID.bit.IDE=1;//拓展帧
    //Enable MBOX0 and MBOX1/MBOX2
    ECanaRegs.CANME.bit.ME0 = 1;
    ECanaRegs.CANME.bit.ME1= 1;
    ECanaRegs.CANME.bit.ME2= 1;

    //M1/2进入接收中断，接收中断，发送不做中断处理
    ECanaRegs.CANMIM.all = 0x06;
    ECanaRegs.CANMIL.all = 0;
    ECanaRegs.CANGIM.all = 0x05;
    EDIS;
}

/*
** ===================================================================
**     Funtion Name :__interrupt void sMailboxISR(void)
**     Description :CAN中断函数
**     Can中断，中断只完成中完成数据的转移。数据的解析，由绑定的数据结构自动完成。
**
**     Parameters  : none
**     Returns     : none
**
** ===================================================================
*/
unsigned char  CANRxFlag= 0;//接收数据中断标志位
unsigned char CANbroadFlag=0;//广播标志位
__interrupt void sMailboxISR(void)
{
    IER = M_INT1;  // 开中断INT1
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
    __asm("  NOP");
    EINT;

    //CAN中断影子寄存器
    union CANGIF0_REG CANGIF0_Shandow;
    unsigned int ISR_MailBoxNum;//邮箱号

    //影子寄存器获取中断寄存器数据
    CANGIF0_Shandow.all = ECanaRegs.CANGIF0.all;
    //获取触发中断的邮箱号。
    ISR_MailBoxNum = CANGIF0_Shandow.bit.MIV0;
    //接收数据标志位
    CANRxFlag =1;
    //广播数据
    if(ISR_MailBoxNum == 2)
        CANbroadFlag = 1;//广播标志位，不需要回复
    //转存数据
    PDU.P = (unsigned char)( ( ECanaMboxes.MBOX1.MSGID.all >>26 ) & 0x00000007 );
    PDU.R = (unsigned char)( ( ECanaMboxes.MBOX1.MSGID.all >>25 ) & 0x00000001 );
    PDU.DP = (unsigned char)( ( ECanaMboxes.MBOX1.MSGID.all >>24 ) & 0x00000001 );
    PDU.PF = (unsigned char)( ( ECanaMboxes.MBOX1.MSGID.all >>16 ) & 0x000000FF );
    PDU.PS = (unsigned char)( ( ECanaMboxes.MBOX1.MSGID.all >>8 ) & 0x000000FF );
    PDU.SA = (unsigned char)( ECanaMboxes.MBOX1.MSGID.all & 0x000000FF );
    PDU.data[0]=(unsigned char)( ( ECanaMboxes.MBOX1.MDL.all>>24 ) & 0x0000000FF);
    PDU.data[1]=(unsigned char)( ( ECanaMboxes.MBOX1.MDL.all>>16 ) & 0x0000000FF);
    PDU.data[2]=(unsigned char)( ( ECanaMboxes.MBOX1.MDL.all>>8 ) & 0x0000000FF);
    PDU.data[3]=(unsigned char)(  ECanaMboxes.MBOX1.MDL.all & 0x0000000FF );
    PDU.data[4]=(unsigned char)( ( ECanaMboxes.MBOX1.MDH.all>>24 ) & 0x0000000FF);
    PDU.data[5]=(unsigned char)( ( ECanaMboxes.MBOX1.MDH.all>>16 ) & 0x0000000FF);
    PDU.data[6]=(unsigned char)( ( ECanaMboxes.MBOX1.MDH.all>>8 ) & 0x0000000FF);
    PDU.data[7]=(unsigned char)(  ECanaMboxes.MBOX1.MDH.all & 0x0000000FF );
    //数据转存完成后清相关标志位
    ECanaRegs.CANRMP.all = 0x1Lu<<ISR_MailBoxNum;
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP9;
}
/*
** ===================================================================
**     Funtion Name : void CANMessage(void)
**     Description : CAN数据处理
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
#define  ERR_NULL 0//无错误
#define  FAIL 1//
void CANMessage(void)
{
    unsigned char errType = ERR_NULL;

    //当有数据接收
    if( CANRxFlag ==1)
    {
        //接收标志位清0
        CANRxFlag = 0;
        //CAN丢失计时清零
        CanDat.LostCnt=0;
        //有数据接收，清CAN丢失故障
        clrRegBits(DF.ErrFlag,F_CAN_LOST);
        //判断接收数据的类型-PGN
        switch(PDU.PF)
        {
            //根据命令帧的不同分别对数据进行缓存
            case PF_SET_CMD : errType = CANSetCmd();//设置命令
            break;
            case PF_HEART_BEAT : //不做任何处理
            break;
        }

        if(CANbroadFlag==1)//广播命令不需要回复
            CANbroadFlag=0;
        else
            CANAck(errType);
    }
    else
    {
        //遥信遥测上报数据-1s上报一次数据
        if(CanDat.RecordCnt> 200)
        {
            CanDat.RecordCnt=0;
            CanRecord();
        }
    }
}
/*
** ===================================================================
**     Funtion Name : unsigned char CANSetCmd(void)
**     Description : 接收的命令为上位机设置命令
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
unsigned char CANSetCmd(void)
{
    unsigned char errType = ERR_NULL;

    //CAN数据转存结构体RemoteCtlSG
    CanDat.OutEn = PDU.data[1];
    CanDat.Voref = ((((unsigned int)PDU.data[3])<<8) | PDU.data[2]);//给定电压
    CanDat.Ioref  = ((((unsigned int)PDU.data[5])<<8) | PDU.data[4]);//给定电流
    //设置命令更新
    errType = SetCmdUpdate();
    return errType;
}
/*
** ===================================================================
**     Funtion Name : unsigned char SetCmdUpdate(void)
**     Description : ，对命令数据进行解析处理，转换成控制程序所需的变量数据
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
unsigned char SetCmdUpdate(void)
{
    unsigned char errType = ERR_NULL;

    //充电回路主接触器状态
    if((CanDat.OutEn&0xFE)==0)
    {
        if(CanDat.OutEn)
            setRegBits(DF.CtrFlag, F_OUT_EN);
        else
            clrRegBits(DF.CtrFlag, F_OUT_EN);
    }
    else//数据不合法
    {
        errType = FAIL;//返回错误
        return errType;
    }
    //转存参考电压
    if((CanDat.Voref>4800)||(CanDat.Voref<2400))//判断参考电压合理性
    {
        errType = FAIL;
        return errType;//数据不合法
    }
    else
    {
        ID2DCtrValue.CanVoref = ((unsigned long)CanDat.Voref)*4706>>10;//将上位机发送的数据转换成控制变量
    }
    //转存参考电流
    if((CanDat.Ioref>1000)||(CanDat.Ioref<10))//判断参考电流合理性
    {
        errType = FAIL;
        return errType;
    }
    else//数据不合法
    {
        ID2DCtrValue.CanIoref = ((unsigned long)CanDat.Ioref)*3813>>8;//将上位机发送的数据转换成控制变量
    }

    return errType;
}
/*
** ===================================================================
**     Funtion Name : void CANAck(unsigned char type)
**     Description : ，回复上位机设置命令
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void CANAck(unsigned char type)
{
    unsigned long MailBoxID = 0;
    unsigned long temp=0;

    temp = PF_SET_CMD_ACK;
    temp = temp<<16;
    //转换ID值-将PDU.SA和PDU.PS兑换，源地址和目标地址兑换
    MailBoxID = ECanaMboxes.MBOX0.MSGID.all  &  0xF8000000;
    MailBoxID = MailBoxID | temp;
    temp = MONITOR_ADD;
    MailBoxID = MailBoxID | (temp<<8);
    MailBoxID = MailBoxID | CAN_ADD;

    temp = type;
    EALLOW;
    //配置ID和数据前关闭发送
    ECanaRegs.CANME.bit.ME0= 0;
    ECanaMboxes.MBOX0.MSGID.all = MailBoxID;
    //装载数据-增加成功失败标志位
    ECanaMboxes.MBOX0.MDL.all =  ECanaMboxes.MBOX1.MDL.all | (temp<<31) ;
    ECanaMboxes.MBOX0.MDH.all = ECanaMboxes.MBOX1.MDH.all ;
    ECanaRegs.CANME.bit.ME0= 1;
    EDIS;

    CANTx();//发送数据
}
/*
** ===================================================================
**     Funtion Name : void CANTx(void)
**     Description : ，CAN发送数据
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void CANTx(void)
{
    unsigned long TxStatus;

    TxStatus = ECanaRegs.CANTRS.bit.TRS0;
    //表明该邮箱没有在发送进程中，可以进行数据发送。
    if(TxStatus == 0)
        ECanaRegs.CANTRS.bit.TRS0=1;
    else
    {
        //表明邮箱没有完成上一次的数据发送，应重启邮箱发送通道。
        ECanaRegs.CANTRR.bit.TRR0=1;
        ECanaRegs.CANTA.bit.TA0=1;
    }
}
/*
** ===================================================================
**     Funtion Name : void CanRecord(void)
**     Description : ，每隔1S发送模块相关参数至上位机
**     Parameters  : none
**     Returns     : none
** ===================================================================
*/
void CanRecord(void)
{
    unsigned long MailBoxID = 0;
    unsigned long data[8]={0};
    unsigned long temp=0;

    //转换ID值-将PDU.SA和PDU.PS兑换，源地址和目标地址兑换
    MailBoxID = (ECanaMboxes.MBOX0.MSGID.all & 0xF8000000);
    MailBoxID = MailBoxID|0x18000000;
    //源地址
    temp = CAN_ADD;
    MailBoxID = MailBoxID | temp;
    //目标地址
    temp = MONITOR_ADD;
    temp = temp<<8;
    MailBoxID = MailBoxID | temp;
    //命令码
    temp = PF_RECORD_VALUE;
    temp = temp<<16;
    MailBoxID = MailBoxID | temp;

    //SADC.VAvg = ID2DCtrValue.Voref;
    //SADC.IAvg = ID2DCtrValue.CanIoref;
    //转存模块运行数据
    data[0]=  PFCData.State;
    data[1]=  PFCData.Err;
    data[2] = DF.SMFlag;
    data[3] = DF.ErrFlag;
    temp = SADC.VAvg*7130>>15;//转换成实际值 放大10倍
    data[4] = temp  & 0x00FF;
    data[5] = (temp >>8) & 0x00FF;
    temp = SADC.IAvg*2200>>15;//转换成实际值 放大10倍
    data[6] = temp & 0x00FF;
    data[7] = (temp>>8) & 0x00FF;

    EALLOW;
    //配置ID和数据前关闭发送
    ECanaRegs.CANME.bit.ME0= 0;
    ECanaMboxes.MBOX0.MSGID.all = MailBoxID;
    //装载数据-增加成功失败标志位
    ECanaMboxes.MBOX0.MDL.all =  (data[0]<<24) | (data[1]<<16) | (data[2]<<8) | data[3];
    ECanaMboxes.MBOX0.MDH.all = (data[4]<<24) | (data[5]<<16) | (data[6]<<8) | data[7];
    ECanaRegs.CANME.bit.ME0= 1;
    EDIS;

    CANTx();//发送数据
}

