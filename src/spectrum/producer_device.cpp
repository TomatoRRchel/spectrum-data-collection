// 频谱仪设备操作实现 — 集成 previous_porducer.cpp (SWP) 和 IQS_GetIQToTxt.cpp (IQ)

#include "producer_device.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "htra_api.h"
#include "example.h"

// ======================== 内部状态 ========================

struct PowerSpectrumState
{
    void *device;
    BootProfile_TypeDef boot_profile;
    BootInfo_TypeDef boot_info;
    SWP_Profile_TypeDef swp_profile_in;
    SWP_Profile_TypeDef swp_profile_out;
    SWP_TraceInfo_TypeDef trace_info; // 包含点数、跳数、RBW 等

    int dev_num;
    double start_freq_hz, stop_freq_hz, rbw_hz;
};

struct IQState
{
    void *device;
    BootProfile_TypeDef boot_profile;
    BootInfo_TypeDef boot_info;
    IQS_Profile_TypeDef iqs_profile_in;
    IQS_Profile_TypeDef iqs_profile_out;
    IQS_StreamInfo_TypeDef stream_info; // 包含采样点数、包数等

    int dev_num;
    double center_freq_hz, ref_level_dbm;
    int decimate_factor;
    uint32_t trigger_length;
};

// ======================== 设备打开 ========================

static int device_open_internal(void **device, int dev_num,
                                BootProfile_TypeDef *profile,
                                BootInfo_TypeDef *info)
{
    profile->DevicePowerSupply = USBPortAndPowerPort;
    profile->PhysicalInterface = USB;
    int status = Device_Open(device, dev_num, profile, info);
    Device_Open_ErrorHandling(status, device, dev_num, profile, info);
    return status;
}

// ======================== 功率谱 ========================

PowerSpectrumState *ps_open_and_config(int dev_num, double start_hz, double stop_hz, double rbw_hz)
{
    PowerSpectrumState *ps = (PowerSpectrumState *)calloc(1, sizeof(*ps));
    if (!ps)
        return NULL;
    ps->dev_num = dev_num;
    ps->start_freq_hz = start_hz;
    ps->stop_freq_hz = stop_hz;
    ps->rbw_hz = rbw_hz;

    memset(&ps->boot_profile, 0, sizeof(ps->boot_profile));
    memset(&ps->boot_info, 0, sizeof(ps->boot_info));
    if (device_open_internal(&ps->device, dev_num, &ps->boot_profile, &ps->boot_info) != 0)
    {
        fprintf(stderr, "[PS] Device open failed\n");
        free(ps);
        return NULL;
    }

    memset(&ps->swp_profile_in, 0, sizeof(ps->swp_profile_in));
    SWP_ProfileDeInit(&ps->device, &ps->swp_profile_in);
    ps->swp_profile_in.StartFreq_Hz = start_hz;
    ps->swp_profile_in.StopFreq_Hz = stop_hz;
    ps->swp_profile_in.RBW_Hz = rbw_hz;

    int status = SWP_Configuration(&ps->device, &ps->swp_profile_in,
                                   &ps->swp_profile_out, &ps->trace_info);
    if (status != APIRETVAL_NoError)
    {
        SWP_Configuration_ErrorHandling(status, &ps->device, dev_num,
                                        &ps->boot_profile, &ps->boot_info, &ps->swp_profile_in,
                                        &ps->swp_profile_out, &ps->trace_info);
        Device_Close(&ps->device);
        free(ps);
        return NULL;
    }
    printf("[PS] %.3f-%.3f Hz, RBW=%.1f Hz, pts=%u, hops=%u\n",
           start_hz, stop_hz, rbw_hz,
           ps->trace_info.FullsweepTracePoints, ps->trace_info.TotalHops);
    return ps;
}

int ps_reconfigure(PowerSpectrumState *ps, double start_hz, double stop_hz, double rbw_hz)
{
    if (!ps || !ps->device)
        return -1;
    ps->start_freq_hz = start_hz;
    ps->stop_freq_hz = stop_hz;
    ps->rbw_hz = rbw_hz;

    memset(&ps->swp_profile_in, 0, sizeof(ps->swp_profile_in));
    SWP_ProfileDeInit(&ps->device, &ps->swp_profile_in);
    ps->swp_profile_in.StartFreq_Hz = start_hz;
    ps->swp_profile_in.StopFreq_Hz = stop_hz;
    ps->swp_profile_in.RBW_Hz = rbw_hz;

    int status = SWP_Configuration(&ps->device, &ps->swp_profile_in,
                                   &ps->swp_profile_out, &ps->trace_info);
    if (status != APIRETVAL_NoError)
    {
        SWP_Configuration_ErrorHandling(status, &ps->device, ps->dev_num,
                                        &ps->boot_profile, &ps->boot_info, &ps->swp_profile_in,
                                        &ps->swp_profile_out, &ps->trace_info);
        return status;
    }
    printf("[PS] Freq: %.3f-%.3f Hz\n", start_hz, stop_hz);
    return 0;
}

int ps_acquire_sweep(PowerSpectrumState *ps, double *freq_data, float *power_data)
{
    if (!ps || !ps->device || !freq_data || !power_data)
        return -1;
    int hop_index = 0, frame_index = 0;
    MeasAuxInfo_TypeDef meas_aux;
    uint32_t pp = ps->trace_info.PartialsweepTracePoints;
    uint32_t th = ps->trace_info.TotalHops;

    for (uint32_t i = 0; i < th; i++)
    {
        int status = SWP_GetPartialSweep(&ps->device,
                                         freq_data + (size_t)i * pp,
                                         power_data + (size_t)i * pp,
                                         &hop_index, &frame_index, &meas_aux);
        if (status != APIRETVAL_NoError)
        {
            SWP_ErrorHandlingExceptOpenAndConfiguration(status, &ps->device, ps->dev_num,
                                                        &ps->boot_profile, &ps->boot_info, &ps->swp_profile_in,
                                                        &ps->swp_profile_out, &ps->trace_info);
            return status;
        }
    }
    return 0;
}

uint32_t ps_get_points(PowerSpectrumState *ps) { return ps ? ps->trace_info.FullsweepTracePoints : 0; }
double ps_get_rbw(PowerSpectrumState *ps) { return ps ? ps->rbw_hz : 0.0; }
void ps_get_frequency(PowerSpectrumState *ps, double *s, double *e)
{
    if (ps)
    {
        *s = ps->start_freq_hz;
        *e = ps->stop_freq_hz;
    }
}
void ps_close(PowerSpectrumState *ps)
{
    if (!ps)
        return;
    if (ps->device)
    {
        Device_Close(&ps->device);
        ps->device = NULL;
    }
    free(ps);
}

// ======================== IQ ========================

IQState *iq_open_and_config(int dev_num, double center_hz, double ref_level_dbm,
                            int decimate_factor, uint32_t trigger_length)
{
    IQState *iq = (IQState *)calloc(1, sizeof(*iq));
    if (!iq)
        return NULL;
    iq->dev_num = dev_num;
    iq->center_freq_hz = center_hz;
    iq->ref_level_dbm = ref_level_dbm;
    iq->decimate_factor = decimate_factor;
    iq->trigger_length = trigger_length;

    memset(&iq->boot_profile, 0, sizeof(iq->boot_profile));
    memset(&iq->boot_info, 0, sizeof(iq->boot_info));
    if (device_open_internal(&iq->device, dev_num, &iq->boot_profile, &iq->boot_info) != 0)
    {
        fprintf(stderr, "[IQ] Device open failed\n");
        free(iq);
        return NULL;
    }

    memset(&iq->iqs_profile_in, 0, sizeof(iq->iqs_profile_in));
    IQS_ProfileDeInit(&iq->device, &iq->iqs_profile_in);
    iq->iqs_profile_in.CenterFreq_Hz = center_hz;
    iq->iqs_profile_in.RefLevel_dBm = ref_level_dbm;
    iq->iqs_profile_in.DecimateFactor = decimate_factor;
    iq->iqs_profile_in.DataFormat = Complex16bit;
    iq->iqs_profile_in.TriggerMode = FixedPoints;
    iq->iqs_profile_in.TriggerSource = Bus;
    iq->iqs_profile_in.TriggerLength = trigger_length;

    int status = IQS_Configuration(&iq->device, &iq->iqs_profile_in,
                                   &iq->iqs_profile_out, &iq->stream_info);
    if (status != APIRETVAL_NoError)
    {
        IQS_Configuration_ErrorHandling(status, &iq->device, dev_num,
                                        &iq->boot_profile, &iq->boot_info, &iq->iqs_profile_in,
                                        &iq->iqs_profile_out, &iq->stream_info);
        Device_Close(&iq->device);
        free(iq);
        return NULL;
    }
    printf("[IQ] Center=%.3f Hz, samples=%lu, packets=%lu\n",
           center_hz, iq->stream_info.StreamSamples, iq->stream_info.PacketCount);
    return iq;
}

int iq_reconfigure(IQState *iq, double center_hz, double ref_level_dbm,
                   int decimate_factor, uint32_t trigger_length)
{
    if (!iq || !iq->device)
        return -1;
    iq->center_freq_hz = center_hz;
    iq->ref_level_dbm = ref_level_dbm;
    iq->decimate_factor = decimate_factor;
    iq->trigger_length = trigger_length;

    memset(&iq->iqs_profile_in, 0, sizeof(iq->iqs_profile_in));
    IQS_ProfileDeInit(&iq->device, &iq->iqs_profile_in);
    iq->iqs_profile_in.CenterFreq_Hz = center_hz;
    iq->iqs_profile_in.RefLevel_dBm = ref_level_dbm;
    iq->iqs_profile_in.DecimateFactor = decimate_factor;
    iq->iqs_profile_in.DataFormat = Complex16bit;
    iq->iqs_profile_in.TriggerMode = FixedPoints;
    iq->iqs_profile_in.TriggerSource = Bus;
    iq->iqs_profile_in.TriggerLength = trigger_length;

    int status = IQS_Configuration(&iq->device, &iq->iqs_profile_in,
                                   &iq->iqs_profile_out, &iq->stream_info);
    if (status != APIRETVAL_NoError)
    {
        IQS_Configuration_ErrorHandling(status, &iq->device, iq->dev_num,
                                        &iq->boot_profile, &iq->boot_info, &iq->iqs_profile_in,
                                        &iq->iqs_profile_out, &iq->stream_info);
        return status;
    }
    printf("[IQ] Center: %.3f Hz\n", center_hz);
    return 0;
}

int iq_acquire_stream(IQState *iq, int16_t *iq_buffer)
{
    if (!iq || !iq->device || !iq_buffer)
        return -1;

    int status = IQS_BusTriggerStart(&iq->device);
    if (status != APIRETVAL_NoError)
    {
        fprintf(stderr, "[IQ] TriggerStart failed: %d\n", status);
        return status;
    }

    IQStream_TypeDef iq_stream;
    uint32_t pkt_samples = iq->stream_info.PacketSamples;
    uint32_t pkt_count = iq->stream_info.PacketCount;
    uint32_t total = iq->stream_info.StreamSamples;

    for (uint32_t j = 0; j < pkt_count; j++)
    {
        status = IQS_GetIQStream_PM1(&iq->device, &iq_stream);
        if (status != APIRETVAL_NoError)
        {
            IQS_ErrorHandlingExceptOpenAndConfiguration(status, &iq->device, iq->dev_num,
                                                        &iq->boot_profile, &iq->boot_info, &iq->iqs_profile_in,
                                                        &iq->iqs_profile_out, &iq->stream_info);
            IQS_BusTriggerStop(&iq->device);
            return status;
        }
        int16_t *IQ = (int16_t *)iq_stream.AlternIQStream;
        uint32_t pts = (j == pkt_count - 1 && total % pkt_samples) ? total % pkt_samples : pkt_samples;
        size_t off = (size_t)pkt_samples * j * 2;
        for (uint32_t i = 0; i < pts; i++)
        {
            iq_buffer[off + i * 2] = IQ[i * 2];
            iq_buffer[off + i * 2 + 1] = IQ[i * 2 + 1];
        }
    }
    IQS_BusTriggerStop(&iq->device);
    return 0;
}

uint32_t iq_get_samples(IQState *iq) { return iq ? iq->stream_info.StreamSamples : 0; }
uint32_t iq_get_total_ints(IQState *iq) { return iq ? iq->stream_info.StreamSamples * 2 : 0; }
double iq_get_center_freq(IQState *iq) { return iq ? iq->center_freq_hz : 0.0; }
void iq_close(IQState *iq)
{
    if (!iq)
        return;
    if (iq->device)
    {
        Device_Close(&iq->device);
        iq->device = NULL;
    }
    free(iq);
}
