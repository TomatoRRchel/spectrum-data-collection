#include <iostream>
#include "htra_api.h"

using namespace std;

int Config_flag = 0;

//"Device_Open"
void Device_Open_ErrorHandling(int Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo)
{
    //"0"
    if (Status == APIRETVAL_NoError)
    {
        cout << "Device is opened successfully" << endl;
    }
    else
    {
        while (1)
        {
            switch (Status)
            {
                //"-1"
            case APIRETVAL_ERROR_BusOpenFailed:
                cout << "Error - please check the driver installation and data cable connection." << endl;
                break;
                //"-3"
            case APIRETVAL_ERROR_RFACalFileIsMissing:
                cout << "Error - RF calibration file is missing, please copy the RF calibration file to the CalFile folder." << endl;
                break;
                //"-4"
            case APIRETVAL_ERROR_IFACalFileIsMissing:
                cout << "Error - IF calibration file is missing, please copy the IF calibration file to the CalFile folder" << endl;
                break;
                //"-7"
            case APIRETVAL_ERROR_UpdateStrategyFailed:
                cout << "Error - Failed to deliver the configuration policy to the device, check whether multiple programs call the device at the same time." << endl;
                break;
                //"-8"
            case APIRETVAL_ERROR_BusError:
                cout << "Error - Bus transmission error, please check the power supply of the device" << endl;
                break;
                //"-43"
            case APIRETVAL_ERROR_IQCalFileIsMissing:
                cout << "Error - IQ calibration file is missing, please copy the IQ calibration file to the CalFile folder." << endl;
                break;
                //"-44"
            default:
                cout << "Error - The device is turned on incorrectly." << endl;
                break;
            }
            cout << "Status = " << Status << endl;
            Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
            if (Status == APIRETVAL_NoError)
            {
                cout << "Device is opened successfully" << endl;
                break;
            }
        }
    }
}

//"SWP_Configuration"
void SWP_Configuration_ErrorHandling(int Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo, SWP_Profile_TypeDef *SWP_ProfileIn, SWP_Profile_TypeDef *SWP_ProfileOut, SWP_TraceInfo_TypeDef *TraceInfo)
{
    //"0"
    if (Status == APIRETVAL_NoError)
    {
        cout << "Configuration delievery succeeded." << endl;
    }
    else
    {
        switch (Status)
        {
            //"错误码10051、10060、10062直接重新打开设备进行参数下发"
        case APIRETVAL_ERROR_ETHTimeOut:
        case APIRETVAL_ERROR_ETHDisconnected:
        case APIRETVAL_ERROR_ETHDataError:
            cout << "Error - A network exception caused the error." << endl;
            while (1)
            {
                cout << "Status = " << Status << endl;
                Status = Device_Close(Device);
                cout << "The device is turned off and is being turned back on." << endl;
                Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
                Device_Open_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo);
                Status = SWP_Configuration(Device, SWP_ProfileIn, SWP_ProfileOut, TraceInfo);
                if (Status == APIRETVAL_NoError)
                {
                    cout << "Configuration delievery succeeded." << endl;
                    break;
                }
                else
                {
                    cout << "Error - The configuration delivery fails, try again." << endl;
                }
            }
            break;

        default:
            for (int i = 0; i < 10; i++)
            {
                switch (Status)
                {
                    //"-11"
                case APIRETVAL_ERROR_BusDownLoad:
                    cout << "Error - Bus configuration parameter failed." << endl;
                    break;
                default:
                    cout << "Error - Failed to deliver the configuration." << endl;
                    break;
                }
                cout << "Status = " << Status << endl;
                Status = SWP_Configuration(Device, SWP_ProfileIn, SWP_ProfileOut, TraceInfo);
                if (Status == APIRETVAL_NoError)
                {
                    Config_flag = 1;
                    break;
                }
            }
            if (Config_flag == 1)
            {
                cout << "Configuration delievery succeeded." << endl;
                Config_flag = 0;
            }
            else
            {
                while (1)
                {
                    Status = Device_Close(Device);
                    cout << "The device is turned off and is being turned back on." << endl;
                    Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
                    Device_Open_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo);
                    Status = SWP_Configuration(Device, SWP_ProfileIn, SWP_ProfileOut, TraceInfo);
                    if (Status == APIRETVAL_NoError)
                    {
                        cout << "Configuration delievery succeeded." << endl;
                        break;
                    }
                    else
                    {
                        cout << "Error - The configuration delivery fails, try again." << endl;
                    }
                }
            }
            break;
        }
    }
}

//"IQS_Configuration"
void IQS_Configuration_ErrorHandling(int Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo, IQS_Profile_TypeDef *IQS_ProfileIn, IQS_Profile_TypeDef *IQS_ProfileOut, IQS_StreamInfo_TypeDef *StreamInfo)
{
    //"0"
    if (Status == APIRETVAL_NoError)
    {
        cout << "Configuration delievery succeeded." << endl;
    }
    else
    {
        switch (Status)
        {
            //"错误码10051、10060、10062直接重新打开设备进行参数下发"
        case APIRETVAL_ERROR_ETHTimeOut:
        case APIRETVAL_ERROR_ETHDisconnected:
        case APIRETVAL_ERROR_ETHDataError:
            cout << "Error - A network exception caused the error." << endl;
            while (1)
            {
                cout << "Status = " << Status << endl;
                Status = Device_Close(Device);
                cout << "The device is turned off and is being turned back on." << endl;
                Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
                Device_Open_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo);
                Status = IQS_Configuration(Device, IQS_ProfileIn, IQS_ProfileOut, StreamInfo);
                if (Status == APIRETVAL_NoError)
                {
                    cout << "Configuration delievery succeeded." << endl;
                    break;
                }
                else
                {
                    cout << "Error - The configuration delivery fails, try again." << endl;
                }
            }
            break;

        default:
            for (int i = 0; i < 10; i++)
            {
                switch (Status)
                {
                    //"-11"
                case APIRETVAL_ERROR_BusDownLoad:
                    cout << "Error - Bus configuration parameter failed." << endl;
                    break;
                default:
                    cout << "Error - Failed to deliver the configuration." << endl;
                    break;
                }
                cout << "Status = " << Status << endl;
                Status = IQS_Configuration(Device, IQS_ProfileIn, IQS_ProfileOut, StreamInfo);
                if (Status == APIRETVAL_NoError)
                {
                    Config_flag = 1;
                    break;
                }
            }
            if (Config_flag == 1)
            {
                cout << "Configuration delievery succeeded." << endl;
                Config_flag = 0;
            }
            else
            {
                while (1)
                {
                    Status = Device_Close(Device);
                    cout << "The device is turned off and is being turned back on." << endl;
                    Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
                    Device_Open_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo);
                    Status = IQS_Configuration(Device, IQS_ProfileIn, IQS_ProfileOut, StreamInfo);
                    if (Status == APIRETVAL_NoError)
                    {
                        cout << "Configuration delievery succeeded." << endl;
                        break;
                    }
                    else
                    {
                        cout << "Error - The configuration delivery fails, try again." << endl;
                    }
                }
            }
            break;
        }
    }
}

//"DET_Configuration"
void DET_Configuration_ErrorHandling(int Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo, DET_Profile_TypeDef *DET_ProfileIn, DET_Profile_TypeDef *DET_ProfileOut, DET_StreamInfo_TypeDef *StreamInfo)
{
    //"0"
    if (Status == APIRETVAL_NoError)
    {
        cout << "Configuration delievery succeeded." << endl;
    }

    else
    {
        switch (Status)
        {
            //"错误码10051、10060、10062直接重新打开设备进行参数下发"
        case APIRETVAL_ERROR_ETHTimeOut:
        case APIRETVAL_ERROR_ETHDisconnected:
        case APIRETVAL_ERROR_ETHDataError:
            cout << "Error - A network exception caused the error." << endl;
            while (1)
            {
                cout << "Status = " << Status << endl;
                Status = Device_Close(Device);
                cout << "The device is turned off and is being turned back on." << endl;
                Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
                Device_Open_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo);
                Status = DET_Configuration(Device, DET_ProfileIn, DET_ProfileOut, StreamInfo);
                if (Status == APIRETVAL_NoError)
                {
                    cout << "Configuration delievery succeeded." << endl;
                    break;
                }
                else
                {
                    cout << "Error - The configuration delivery fails, try again." << endl;
                }
            }
            break;

        default:
            for (int i = 0; i < 10; i++)
            {
                switch (Status)
                {
                    //"-11"
                case APIRETVAL_ERROR_BusDownLoad:
                    cout << "Error - Bus configuration parameter failed." << endl;
                    break;
                default:
                    cout << "Error - Failed to deliver the configuration." << endl;
                    break;
                }
                cout << "Status = " << Status << endl;
                Status = DET_Configuration(Device, DET_ProfileIn, DET_ProfileOut, StreamInfo);
                if (Status == APIRETVAL_NoError)
                {
                    Config_flag = 1;
                    break;
                }
            }
            if (Config_flag == 1)
            {
                cout << "Configuration delievery succeeded." << endl;
                Config_flag = 0;
            }
            else
            {
                while (1)
                {
                    Status = Device_Close(Device);
                    cout << "The device is turned off and is being turned back on." << endl;
                    Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
                    Device_Open_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo);
                    Status = DET_Configuration(Device, DET_ProfileIn, DET_ProfileOut, StreamInfo);
                    if (Status == APIRETVAL_NoError)
                    {
                        cout << "Configuration delievery succeeded." << endl;
                        break;
                    }
                    else
                    {
                        cout << "Error - The configuration delivery fails, try again." << endl;
                    }
                }
            }
            break;
        }
    }
}

//"RTA_Configuration"
void RTA_Configuration_ErrorHandling(int Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo, RTA_Profile_TypeDef *RTA_ProfileIn, RTA_Profile_TypeDef *RTA_ProfileOut, RTA_FrameInfo_TypeDef *FrameInfo)
{
    //"0"
    if (Status == APIRETVAL_NoError)
    {
        cout << "Configuration delievery succeeded." << endl;
    }
    else
    {
        switch (Status)
        {
            //"错误码10051、10060、10062直接重新打开设备进行参数下发"
        case APIRETVAL_ERROR_ETHTimeOut:
        case APIRETVAL_ERROR_ETHDisconnected:
        case APIRETVAL_ERROR_ETHDataError:
            cout << "Error - A network exception caused the error." << endl;
            while (1)
            {
                cout << "Status = " << Status << endl;
                Status = Device_Close(Device);
                cout << "The device is turned off and is being turned back on." << endl;
                Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
                Device_Open_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo);
                Status = RTA_Configuration(Device, RTA_ProfileIn, RTA_ProfileOut, FrameInfo);
                if (Status == APIRETVAL_NoError)
                {
                    cout << "Configuration delievery succeeded." << endl;
                    break;
                }
                else
                {
                    cout << "Error - The configuration delivery fails, try again." << endl;
                }
            }
            break;

        default:
            for (int i = 0; i < 10; i++)
            {
                switch (Status)
                {
                    //"-11"
                case APIRETVAL_ERROR_BusDownLoad:
                    cout << "Error - Bus configuration parameter failed." << endl;
                    break;
                default:
                    cout << "Error - Failed to deliver the configuration." << endl;
                    break;
                }
                cout << "Status = " << Status << endl;
                Status = RTA_Configuration(Device, RTA_ProfileIn, RTA_ProfileOut, FrameInfo);
                if (Status == APIRETVAL_NoError)
                {
                    Config_flag = 1;
                    break;
                }
            }
            if (Config_flag == 1)
            {
                cout << "Configuration delievery succeeded." << endl;
                Config_flag = 0;
            }
            else
            {
                while (1)
                {
                    Status = Device_Close(Device);
                    cout << "The device is turned off and is being turned back on." << endl;
                    Status = Device_Open(Device, DevNum, BootProfile, BootInfo);
                    Device_Open_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo);
                    Status = RTA_Configuration(Device, RTA_ProfileIn, RTA_ProfileOut, FrameInfo);
                    if (Status == APIRETVAL_NoError)
                    {
                        cout << "Configuration delievery succeeded." << endl;
                        break;
                    }
                    else
                    {
                        cout << "Error - The configuration delivery fails, try again." << endl;
                    }
                }
            }
            break;
        }
    }
}

//"SWP ErrorHandlingExceptOpenAndConfiguration "
void SWP_ErrorHandlingExceptOpenAndConfiguration(int &Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo, SWP_Profile_TypeDef *SWP_ProfileIn, SWP_Profile_TypeDef *SWP_ProfileOut, SWP_TraceInfo_TypeDef *TraceInfo)
{

    if (Status != APIRETVAL_NoError)
    {
        switch (Status)
        {
            //"警告代码-12、-14、-15、-16、-17、-18、-19、-36、-37、-38、-39" 警告代码提醒之后，将错误码置零继续获取数据即可
        case APIRETVAL_WARNING_IFOverflow:
        case APIRETVAL_WARNING_ReconfigurationIsRecommended:
        case APIRETVAL_WARNING_ClockUnlocked_SYSCLK:
        case APIRETVAL_WARNING_ClockUnlocked_ADCCLK:
        case APIRETVAL_WARNING_ClockUnlocked_RXIFLO:
        case APIRETVAL_WARNING_ClockUnlocked_RXRFLO:
        case APIRETVAL_WARNING_ADCConfigError:
        case APIRETVAL_WARNING_ClockRelocked_SYSCLK:
        case APIRETVAL_WARNING_ClockRelocked_ADCCLK:
        case APIRETVAL_WARNING_ClockRelocked_RXIFLO:
        case APIRETVAL_WARNING_ClockRelocked_RXRFLO:
            switch (Status)
            {
            case APIRETVAL_WARNING_IFOverflow:
                cout << "Warning: - The midrange is saturated, it is recommended to adjust the reference level." << endl;
                SWP_ProfileIn->RefLevel_dBm += 5;
                break; // 中频饱和为警告，数据获取仍然进行中，不断上调参考电平即可，此处演示以5为步进自动上调参考电平直至恢复正常
            case APIRETVAL_WARNING_ReconfigurationIsRecommended:
                cout << "Warning - If the temperature changes greatly since the last configuration, the parameters will be re-issued." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_SYSCLK:
                cout << "Warning - The system clock is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_ADCCLK:
                cout << "Warning - The ADC clock is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_RXIFLO:
                cout << "Warning - The receiver IF local oscillator is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_RXRFLO:
                cout << "Warning - The receiving RF local oscillator is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ADCConfigError:
                cout << "Warning - The ADC is misconfigured, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_SYSCLK:
                cout << "Warning - The system clock is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_ADCCLK:
                cout << "Warning: The ADC clock is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_RXIFLO:
                cout << "Warning: The receiving IF local oscillator is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_RXRFLO:
                cout << "Warning: The receiving RF local oscillator is relocked, please reseat the device." << endl;
                break;
            default:
                break;
            }
            cout << "Status = " << Status << endl;
            Status = SWP_Configuration(Device, SWP_ProfileIn, SWP_ProfileOut, TraceInfo);
            SWP_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, SWP_ProfileIn, SWP_ProfileOut, TraceInfo);
            break;

            //"错误码10051、10060、10062直接传入Configuration_ErrorHandling重启设备使用"
        case APIRETVAL_ERROR_ETHTimeOut:
        case APIRETVAL_ERROR_ETHDisconnected:
        case APIRETVAL_ERROR_ETHDataError:
            SWP_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, SWP_ProfileIn, SWP_ProfileOut, TraceInfo);
            break;

        default:
            cout << "Error - Get data exceptions." << endl;
            cout << "Status = " << Status << endl;
            cout << "Re-deliver the parameters" << endl;
            for (int i = 0; i < 5; i++)
            {
                Status = SWP_Configuration(Device, SWP_ProfileIn, SWP_ProfileOut, TraceInfo);
                if (Status == APIRETVAL_NoError)
                {
                    Config_flag = 1;
                    break;
                }
                SWP_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, SWP_ProfileIn, SWP_ProfileOut, TraceInfo);
            }
            if (Config_flag == 1)
            {
                cout << "The parameters are configured." << endl;
                Config_flag = 0;
            }
            break;
        }
    }
}

//"IQS ErrorHandlingExceptOpenAndConfiguration"
void IQS_ErrorHandlingExceptOpenAndConfiguration(int &Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo, IQS_Profile_TypeDef *IQS_ProfileIn, IQS_Profile_TypeDef *IQS_ProfileOut, IQS_StreamInfo_TypeDef *StreamInfo)
{

    if (Status != APIRETVAL_NoError)
    {
        switch (Status)
        {
            //"警告代码-12、-14、-15、-16、-17、-18、-19、-36、-37、-38、-39" 警告代码提醒之后，将错误码置零继续获取数据即可
        case APIRETVAL_WARNING_IFOverflow:
        case APIRETVAL_WARNING_ReconfigurationIsRecommended:
        case APIRETVAL_WARNING_ClockUnlocked_SYSCLK:
        case APIRETVAL_WARNING_ClockUnlocked_ADCCLK:
        case APIRETVAL_WARNING_ClockUnlocked_RXIFLO:
        case APIRETVAL_WARNING_ClockUnlocked_RXRFLO:
        case APIRETVAL_WARNING_ADCConfigError:
        case APIRETVAL_WARNING_ClockRelocked_SYSCLK:
        case APIRETVAL_WARNING_ClockRelocked_ADCCLK:
        case APIRETVAL_WARNING_ClockRelocked_RXIFLO:
        case APIRETVAL_WARNING_ClockRelocked_RXRFLO:
            switch (Status)
            {
            case APIRETVAL_WARNING_IFOverflow:
                cout << "Warning: - The midrange is saturated, it is recommended to adjust the reference level." << endl;
                IQS_ProfileIn->RefLevel_dBm += 5;
                break; // 中频饱和为警告，数据获取仍然进行中，不断上调参考电平即可，此处演示以5为步进自动上调参考电平直至恢复正常
            case APIRETVAL_WARNING_ReconfigurationIsRecommended:
                cout << "Warning - If the temperature changes greatly since the last configuration, the parameters will be re-issued." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_SYSCLK:
                cout << "Warning - The system clock is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_ADCCLK:
                cout << "Warning - The ADC clock is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_RXIFLO:
                cout << "Warning - The receiver IF local oscillator is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_RXRFLO:
                cout << "Warning - The receiving RF local oscillator is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ADCConfigError:
                cout << "Warning - The ADC is misconfigured, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_SYSCLK:
                cout << "Warning - The system clock is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_ADCCLK:
                cout << "Warning: The ADC clock is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_RXIFLO:
                cout << "Warning: The receiving IF local oscillator is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_RXRFLO:
                cout << "Warning: The receiving RF local oscillator is relocked, please reseat the device." << endl;
                break;
            default:
                break;
            }
            cout << "Status = " << Status << endl;
            Status = IQS_Configuration(Device, IQS_ProfileIn, IQS_ProfileOut, StreamInfo);
            IQS_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, IQS_ProfileIn, IQS_ProfileOut, StreamInfo);
            if (IQS_ProfileOut->TriggerSource == Bus)
            {
                IQS_BusTriggerStart(Device);
            }
            break;

            //"错误码10051、10060、10062直接传入Configuration_ErrorHandling重启设备使用"
        case APIRETVAL_ERROR_ETHTimeOut:
        case APIRETVAL_ERROR_ETHDisconnected:
        case APIRETVAL_ERROR_ETHDataError:
            IQS_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, IQS_ProfileIn, IQS_ProfileOut, StreamInfo);
            if (IQS_ProfileOut->TriggerSource == Bus)
            {
                IQS_BusTriggerStart(Device);
            }
            break;

            //"-9"与"-10"总线数据错误重新下发参数即可
        default:
            if (Status == APIRETVAL_ERROR_BusDataError)
            {
                cout << "Error - Bus data error, check if the device is turned on more." << endl;
            }
            else if (Status == APIRETVAL_WARNING_BusTimeOut)
            {
                cout << "Error - Get data timed out, please reconfigure the parameters." << endl;
            }
            else
            {
                cout << "Error - Error in getting data." << endl;
            }
            cout << "Status = " << Status << endl;
            cout << "Re-deliver the parameters." << endl;
            for (int i = 0; i < 5; i++)
            {
                Status = IQS_Configuration(Device, IQS_ProfileIn, IQS_ProfileOut, StreamInfo);
                if (Status == APIRETVAL_NoError)
                {
                    Config_flag = 1;
                    break;
                }
                IQS_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, IQS_ProfileIn, IQS_ProfileOut, StreamInfo);
            }
            if (Config_flag == 1)
            {
                cout << "The parameters are configured." << endl;
                Config_flag = 0;
            }
            if (IQS_ProfileOut->TriggerSource == Bus)
            {
                IQS_BusTriggerStart(Device);
            }
            break;
        }
    }
}

//"DET ErrorHandlingExceptOpenAndConfiguration"
void DET_ErrorHandlingExceptOpenAndConfiguration(int &Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo, DET_Profile_TypeDef *DET_ProfileIn, DET_Profile_TypeDef *DET_ProfileOut, DET_StreamInfo_TypeDef *StreamInfo)
{

    if (Status != APIRETVAL_NoError)
    {
        switch (Status)
        {
            //"警告代码-12、-14、-15、-16、-17、-18、-19、-36、-37、-38、-39" 警告代码提醒之后，将错误码置零继续获取数据即可
        case APIRETVAL_WARNING_IFOverflow:
        case APIRETVAL_WARNING_ReconfigurationIsRecommended:
        case APIRETVAL_WARNING_ClockUnlocked_SYSCLK:
        case APIRETVAL_WARNING_ClockUnlocked_ADCCLK:
        case APIRETVAL_WARNING_ClockUnlocked_RXIFLO:
        case APIRETVAL_WARNING_ClockUnlocked_RXRFLO:
        case APIRETVAL_WARNING_ADCConfigError:
        case APIRETVAL_WARNING_ClockRelocked_SYSCLK:
        case APIRETVAL_WARNING_ClockRelocked_ADCCLK:
        case APIRETVAL_WARNING_ClockRelocked_RXIFLO:
        case APIRETVAL_WARNING_ClockRelocked_RXRFLO:
            switch (Status)
            {
            case APIRETVAL_WARNING_IFOverflow:
                cout << "Warning: - The midrange is saturated, it is recommended to adjust the reference level." << endl;
                DET_ProfileIn->RefLevel_dBm += 5;
                break; // 中频饱和为警告，数据获取仍然进行中，不断上调参考电平即可，此处演示以5为步进自动上调参考电平直至恢复正常
            case APIRETVAL_WARNING_ReconfigurationIsRecommended:
                cout << "Warning - If the temperature changes greatly since the last configuration, the parameters will be re-issued." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_SYSCLK:
                cout << "Warning - The system clock is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_ADCCLK:
                cout << "Warning - The ADC clock is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_RXIFLO:
                cout << "Warning - The receiver IF local oscillator is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_RXRFLO:
                cout << "Warning - The receiving RF local oscillator is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ADCConfigError:
                cout << "Warning - The ADC is misconfigured, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_SYSCLK:
                cout << "Warning - The system clock is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_ADCCLK:
                cout << "Warning: The ADC clock is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_RXIFLO:
                cout << "Warning: The receiving IF local oscillator is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_RXRFLO:
                cout << "Warning: The receiving RF local oscillator is relocked, please reseat the device." << endl;
                break;
            default:
                break;
            }
            cout << "Status = " << Status << endl;
            Status = DET_Configuration(Device, DET_ProfileIn, DET_ProfileOut, StreamInfo);
            DET_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, DET_ProfileIn, DET_ProfileOut, StreamInfo);
            if (DET_ProfileOut->TriggerSource == Bus)
            {
                DET_BusTriggerStart(Device);
            }
            break;

            //"错误码10051、10060、10062直接传入Configuration_ErrorHandling重启设备使用"
        case APIRETVAL_ERROR_ETHTimeOut:
        case APIRETVAL_ERROR_ETHDisconnected:
        case APIRETVAL_ERROR_ETHDataError:
            DET_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, DET_ProfileIn, DET_ProfileOut, StreamInfo);
            if (DET_ProfileOut->TriggerSource == Bus)
            {
                DET_BusTriggerStart(Device);
            }
            break;

            //"-9"与"-10"总线数据错误重新下发参数即可
        default:
            if (Status == APIRETVAL_ERROR_BusDataError)
            {
                cout << "Error - Bus data error, check if the device is turned on more." << endl;
            }
            else if (Status == APIRETVAL_WARNING_BusTimeOut)
            {
                cout << "Error - Get data timed out, please reconfigure the parameters." << endl;
            }
            else
            {
                cout << "Error - Error in getting data." << endl;
            }
            cout << "Status = " << Status << endl;
            cout << "Re-deliver the parameters." << endl;
            for (int i = 0; i < 5; i++)
            {
                Status = DET_Configuration(Device, DET_ProfileIn, DET_ProfileOut, StreamInfo);
                if (Status == APIRETVAL_NoError)
                {
                    Config_flag = 1;
                    break;
                }
                DET_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, DET_ProfileIn, DET_ProfileOut, StreamInfo);
            }
            if (Config_flag == 1)
            {
                cout << "The parameters are configured." << endl;
                Config_flag = 0;
            }
            if (DET_ProfileOut->TriggerSource == Bus)
            {
                DET_BusTriggerStart(Device);
            }
            break;
        }
    }
}

//"RTA ErrorHandlingExceptOpenAndConfiguration"
void RTA_ErrorHandlingExceptOpenAndConfiguration(int &Status, void **Device, int DevNum, BootProfile_TypeDef *BootProfile, BootInfo_TypeDef *BootInfo, RTA_Profile_TypeDef *RTA_ProfileIn, RTA_Profile_TypeDef *RTA_ProfileOut, RTA_FrameInfo_TypeDef *FrameInfo)
{

    if (Status != APIRETVAL_NoError)
    {
        switch (Status)
        {
            //"警告代码-12、-14、-15、-16、-17、-18、-19、-36、-37、-38、-39" 警告代码提醒之后，将错误码置零继续获取数据即可
        case APIRETVAL_WARNING_IFOverflow:
        case APIRETVAL_WARNING_ReconfigurationIsRecommended:
        case APIRETVAL_WARNING_ClockUnlocked_SYSCLK:
        case APIRETVAL_WARNING_ClockUnlocked_ADCCLK:
        case APIRETVAL_WARNING_ClockUnlocked_RXIFLO:
        case APIRETVAL_WARNING_ClockUnlocked_RXRFLO:
        case APIRETVAL_WARNING_ADCConfigError:
        case APIRETVAL_WARNING_ClockRelocked_SYSCLK:
        case APIRETVAL_WARNING_ClockRelocked_ADCCLK:
        case APIRETVAL_WARNING_ClockRelocked_RXIFLO:
        case APIRETVAL_WARNING_ClockRelocked_RXRFLO:
            switch (Status)
            {
            case APIRETVAL_WARNING_IFOverflow:
                cout << "Warning: - The midrange is saturated, it is recommended to adjust the reference level." << endl;
                RTA_ProfileIn->RefLevel_dBm += 5;
                break; // 中频饱和为警告，数据获取仍然进行中，不断上调参考电平即可，此处演示以5为步进自动上调参考电平直至恢复正常
            case APIRETVAL_WARNING_ReconfigurationIsRecommended:
                cout << "Warning - If the temperature changes greatly since the last configuration, the parameters will be re-issued." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_SYSCLK:
                cout << "Warning - The system clock is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_ADCCLK:
                cout << "Warning - The ADC clock is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_RXIFLO:
                cout << "Warning - The receiver IF local oscillator is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockUnlocked_RXRFLO:
                cout << "Warning - The receiving RF local oscillator is out of lock, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ADCConfigError:
                cout << "Warning - The ADC is misconfigured, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_SYSCLK:
                cout << "Warning - The system clock is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_ADCCLK:
                cout << "Warning: The ADC clock is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_RXIFLO:
                cout << "Warning: The receiving IF local oscillator is relocked, please reseat the device." << endl;
                break;
            case APIRETVAL_WARNING_ClockRelocked_RXRFLO:
                cout << "Warning: The receiving RF local oscillator is relocked, please reseat the device." << endl;
                break;
            default:
                break;
            }
            cout << "Status = " << Status << endl;
            Status = RTA_Configuration(Device, RTA_ProfileIn, RTA_ProfileOut, FrameInfo);
            RTA_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, RTA_ProfileIn, RTA_ProfileOut, FrameInfo);
            if (RTA_ProfileOut->TriggerSource == Bus)
            {
                RTA_BusTriggerStart(Device);
            }
            break;

            //"错误码10051、10060、10062直接传入Configuration_ErrorHandling重启设备使用"
        case APIRETVAL_ERROR_ETHTimeOut:
        case APIRETVAL_ERROR_ETHDisconnected:
        case APIRETVAL_ERROR_ETHDataError:
            RTA_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, RTA_ProfileIn, RTA_ProfileOut, FrameInfo);
            if (RTA_ProfileOut->TriggerSource == Bus)
            {
                RTA_BusTriggerStart(Device);
            }
            break;

            //"-9"与"-10"总线数据错误重新下发参数即可
        default:
            if (Status == APIRETVAL_ERROR_BusDataError)
            {
                cout << "Error - Bus data error, check if the device is turned on more." << endl;
            }
            else if (Status == APIRETVAL_WARNING_BusTimeOut)
            {
                cout << "Error - Get data timed out, please reconfigure the parameters." << endl;
            }
            else
            {
                cout << "Error - Error in getting data." << endl;
            }
            cout << "Status = " << Status << endl;
            cout << "Re-deliver the parameters." << endl;
            for (int i = 0; i < 5; i++)
            {
                Status = RTA_Configuration(Device, RTA_ProfileIn, RTA_ProfileOut, FrameInfo);
                if (Status == APIRETVAL_NoError)
                {
                    Config_flag = 1;
                    break;
                }
                RTA_Configuration_ErrorHandling(Status, Device, DevNum, BootProfile, BootInfo, RTA_ProfileIn, RTA_ProfileOut, FrameInfo);
            }
            if (Config_flag == 1)
            {
                cout << "The parameters are configured." << endl;
                Config_flag = 0;
            }
            if (RTA_ProfileOut->TriggerSource == Bus)
            {
                RTA_BusTriggerStart(Device);
            }
            break;
        }
    }
}