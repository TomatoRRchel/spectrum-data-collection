#ifndef PRODUCER_DEVICE_H
#define PRODUCER_DEVICE_H

// 频谱仪设备操作封装 (封装 htra_api 交互)
// 功率谱路径: Device_Open → SWP_ProfileDeInit → SWP_Configuration → SWP_GetPartialSweep
// IQ 路径:     Device_Open → IQS_ProfileDeInit → IQS_Configuration → IQS_GetIQStream

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct PowerSpectrumState PowerSpectrumState;
    typedef struct IQState IQState;

    // ======================== 功率谱 ========================
    PowerSpectrumState *ps_open_and_config(int dev_num, double start_hz, double stop_hz, double rbw_hz);
    int ps_reconfigure(PowerSpectrumState *ps, double start_hz, double stop_hz, double rbw_hz);
    int ps_acquire_sweep(PowerSpectrumState *ps, double *freq_data, float *power_data);
    uint32_t ps_get_points(PowerSpectrumState *ps);
    double ps_get_rbw(PowerSpectrumState *ps);
    void ps_get_frequency(PowerSpectrumState *ps, double *start_hz, double *stop_hz);
    void ps_close(PowerSpectrumState *ps);

    // ======================== IQ ========================
    IQState *iq_open_and_config(int dev_num, double center_hz, double ref_level_dbm,
                                int decimate_factor, uint32_t trigger_length);
    int iq_reconfigure(IQState *iq, double center_hz, double ref_level_dbm,
                       int decimate_factor, uint32_t trigger_length);
    int iq_acquire_stream(IQState *iq, int16_t *iq_buffer);
    uint32_t iq_get_samples(IQState *iq);
    uint32_t iq_get_total_ints(IQState *iq);
    double iq_get_center_freq(IQState *iq);
    void iq_close(IQState *iq);

#ifdef __cplusplus
}
#endif
#endif
