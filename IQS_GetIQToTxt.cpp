#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string.h>
#include <vector>
#include <chrono>
#include "htra_api.h"
#include "example.h"
using namespace std;

#define IS_USB 1 // 默认使用的是USB型设备，若使用的是网口型设备则将IS_USB定义为0。

int IQS_GetIQToTxt()
{
    int Status = 0;      // 函数的返回。
    void *Device = NULL; // 当前设备的内存地址。
    int DevNum = 0;      // 指定设备号。

    BootProfile_TypeDef BootProfile; // 启动配置结构体，包括物理接口、供电方式等。
    BootInfo_TypeDef BootInfo;       // 启动信息结构体，包括设备信息、USB速率等。

    BootProfile.DevicePowerSupply = USBPortAndPowerPort; // 使用USB数据端口及独立电源端口双供电。

#if IS_USB == 1
    // 配置USB接口。
    BootProfile.PhysicalInterface = USB;
#else
    // 配置ETH接口。
    BootProfile.PhysicalInterface = ETH;
    BootProfile.ETH_IPVersion = IPv4;
    BootProfile.ETH_RemotePort = 5000;
    BootProfile.ETH_ReadTimeOut = 5000;
    BootProfile.ETH_IPAddress[0] = 192;
    BootProfile.ETH_IPAddress[1] = 168;
    BootProfile.ETH_IPAddress[2] = 1;
    BootProfile.ETH_IPAddress[3] = 100;
#endif

    Status = Device_Open(&Device, DevNum, &BootProfile, &BootInfo); // 打开设备。

    Device_Open_ErrorHandling(Status, &Device, DevNum, &BootProfile, &BootInfo); // 当Status不为0时，根据Status的返回值进行相对应的错误处理。

    IQS_Profile_TypeDef IQS_ProfileIn;  // IQS输入配置，包括起始频率、终止频率、RBW、参考电平等。
    IQS_Profile_TypeDef IQS_ProfileOut; // IQS输出配置。
    IQS_StreamInfo_TypeDef StreamInfo;  // 当前配置下IQ数据信息，包括带宽, IQ单路采样率等。

    Status = IQS_ProfileDeInit(&Device, &IQS_ProfileIn); // 初始化配置IQS模式的相关参数。

    IQS_ProfileIn.CenterFreq_Hz = 1e9;       // 配置中心频率。
    IQS_ProfileIn.RefLevel_dBm = 0;          // 配置参考电平。
    IQS_ProfileIn.DecimateFactor = 2;        // 配置抽取倍数。
    IQS_ProfileIn.DataFormat = Complex16bit; // 配置IQ数据格式。
    IQS_ProfileIn.TriggerMode = FixedPoints; // 配置触发模式。
    IQS_ProfileIn.TriggerSource = Bus;       // 配置触发源为内部总线触发.
    IQS_ProfileIn.TriggerLength = 16242;     // 配置单次触发采集的点数。仅当TriggerMode设置为FixedPoints时生效。

    Status = IQS_Configuration(&Device, &IQS_ProfileIn, &IQS_ProfileOut, &StreamInfo); // 下发IQS模式的相关配置。

    IQS_Configuration_ErrorHandling(Status, &Device, DevNum, &BootProfile, &BootInfo, &IQS_ProfileIn, &IQS_ProfileOut, &StreamInfo); // 当Status不为0时，根据Status的返回值进行相对应的错误处理。

    IQStream_TypeDef IQStream;                        // 存放IQ数据包数据，包括IQ数据、配置信息等。
    vector<int16_t> I_Data(StreamInfo.StreamSamples); // 创建I路数据数组。
    vector<int16_t> Q_Data(StreamInfo.StreamSamples); // 创建Q路数据数组。

    int num = 0; // 控制获取IQ数据的循环次数，当前设置为1次。

    // 获取数据并存入IQdata.txt文件
    fstream File;
    char FilePath_s[] = "./IQdata.txt";            // 储存文件路径。
    File.open(FilePath_s, ios::app | ios::binary); // 以二进制格式打开。

    while (num < 1)
    {
        Status = IQS_BusTriggerStart(&Device); // 触发设备。若触发源为外部触发，则不需要调用此函数。

        for (int j = 0; j < StreamInfo.PacketCount; j++)
        {
            Status = IQS_GetIQStream_PM1(&Device, &IQStream); // 获取IQ数据包、触发信息、I路数据最大值及最大值数组下标。

            if (Status == APIRETVAL_NoError)
            {
                // UserCode here
                // 注意：实际使用IQ模式时，建议开一个线程专门调用IQS_GetIQStream获取IQ数据，不能与处理IQ数据放在同一个线程里。

                int16_t *IQ = (int16_t *)IQStream.AlternIQStream;
                uint32_t Points = StreamInfo.PacketSamples;

                if (j == StreamInfo.PacketCount - 1 && StreamInfo.StreamSamples % StreamInfo.PacketSamples != 0) // 可能最后一包不满一整包（16242个点）；所以只需要循环不满一包的点数
                {
                    Points = StreamInfo.StreamSamples % StreamInfo.PacketSamples;
                }

                for (uint32_t i = 0; i < Points; i++)
                {
                    I_Data[i + StreamInfo.PacketSamples * j] = IQ[i * 2];
                    Q_Data[i + StreamInfo.PacketSamples * j] = IQ[i * 2 + 1];
                    File << IQ[i * 2] << "\t" << IQ[i * 2 + 1] << "\n";
                }
            }

            else // 当Status不为0时，根据Status的返回值进行相对应的错误处理。
            {
                IQS_ErrorHandlingExceptOpenAndConfiguration(Status, &Device, DevNum, &BootProfile, &BootInfo, &IQS_ProfileIn, &IQS_ProfileOut, &StreamInfo);
            }
        }

        num++;
    }

    File.close();

    Status = IQS_BusTriggerStop(&Device); // 停止触发设备。若触发源为外部触发，则不需要调用此函数。

    Device_Close(&Device); // 关闭设备。

    return 0;
}